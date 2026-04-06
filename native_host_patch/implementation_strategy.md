# Synthetic Host Strategy

Current best conclusion from static reversing:

- retail Darktide still contains a usable LAN client/join path
- retail Darktide still contains most lobby/channel state handling
- the stock host-side receive/bootstrap path for discovery and join requests appears missing or unwired

Therefore the likely path forward is not “call hidden host API”, but:

1. Explicitly load the companion patch through the stable `dxva2` path.
2. Resolve the live `LanClient` singleton and its deep transport interface at `LanClient+0x68`.
3. Register custom outer callbacks on the surviving transport registry rather than replacing stock ones.
4. Synthesize the missing host-side message flow.

## VT2-guided conclusion

Installed VT2 binary analysis materially strengthened the host-side direction:

- VT2 still ships a real Lua-exposed `create_lan_lobby(...)` path.
- It sits directly next to `join_lan_lobby(...)` and `create_lobby_browser(...)`.
- Its constructor performs host/create field initialization, generates a lobby id, registers multiple callbacks immediately, and enters a host/create state.

What that means for Darktide:

- a byte-for-byte transplant is not realistic.
- a behavior-level port is realistic.

So the best host-side plan is now:

1. use VT2 as the reference for the missing host bootstrap *shape*
2. recreate that shape in Darktide against Darktide’s own `LanLobby` fields, callback tables, and session host path
3. hand off to Darktide’s already-existing post-bootstrap lobby/session machinery as early as possible

## Parallel strike-team/session direction

In parallel to the LAN/browser work, Darktide source now suggests a second viable direction:

1. keep the current strike-team party state intact
2. continue to use `PartyImmateriumManager.join_game_session()` as the top-level entrypoint
3. redirect `PartyImmateriumMissionSessionBoot` at the point where it fetches dedicated server details
4. provide a player-hosted target description instead of backend dedicated-server details
5. reuse the existing browser/lobby/client/session boot path from there if possible

This makes the strike-team path look less like “invent a new session flow” and more like “replace the dedicated-server-details phase with a player-hosted equivalent”.

## Most plausible native plan

### Stage 1: Observe runtime state

- use recovered globals:
  - `network owner = 0x1412a4258`
  - `lan client = 0x1412a43e0`
- read from `LanClient`:
  - transport at `+0x68`
  - active lobby list at `+0x80/+0x88`
  - known peers at `+0x590/+0x598`

Goal:

- verify these pointers are live in hub/menu runtime
- confirm callback registration behaves as expected under the real process

### Stage 2: Add custom outer callback

Use transport registration vfunc `+0x38`.

Known stock registrations:

- `0x14056e4f0` : outer connection/setup callback
- `0x14056f5d0` : outer connectionless lobby callback

Static evidence suggests the registry is token-based and multi-entry, so adding a third callback is plausible.

Custom goal:

- accept incoming outer `0x12` (`discover_lobby`) which stock code does not appear to handle
- parse enough of the request to identify requester peer id / host blob
- emit a synthetic outer `0x13` (`discover_lobby_reply`) with a valid lobby id

Current send-path note:

- outer discovery-like sends appear to use transport vfunc `+0x20`
- outer explicit-destination sends appear to use transport vfunc `+0x30`
- this means a synthetic `0x13` reply will likely need the explicit-destination form, while stock browser refresh uses the discovery-like form
- current strongest static conclusion: no stock outer `0x13 discover_lobby_reply` sender exists to reuse, so this part will almost certainly need fully synthetic packet construction + send

Current send contracts:

- discovery/broadcast send (`+0x20`) is now strongly modeled as:
  - `bool send_broadcast(backend, buffer, byte_len, port)`
- explicit connectionless send (`+0x30`) is now strongly modeled as:
  - `bool send_explicit(backend, sockaddr_in6_like*, flags_or_route, buffer, byte_len)`

Addressing fallback now known:

- stock code also has a `u64 source_id -> sockaddr_in6` resolution path (`0x14056e0d0` + `0x140587380`)
- and at least one separate stock reply helper that sends directly by `peer/source id` (`0x140353A80`)
- so future reply work may have two viable addressing strategies:
  1. resolve callback source id to explicit sockaddr and use `+0x30`
  2. find/reuse a peer-id-based send primitive if it can be generalized to browser traffic

Useful recovered framing detail:

- the explicit-destination connectionless sender `0x14056efd0` uses a lower-level `0x3EE5` envelope with bit-packed fields and a trailing 1-byte payload marker `0x21`
- future synthetic `0x13` work will likely need to reproduce that envelope style rather than only writing a naked high-level packet id
- high-confidence token rule for that trailing 1-byte marker is now `(event_id << 1) | 1`
  - `0x10 -> 0x21`
  - `0x12 -> 0x25`
  - likely `0x13 -> 0x27`

Nested browser-service framing now known:

- `service_id` (`4 bits`) should be `0` for `lobby browser`
- `reserved/version` (`8 bits`) should be `0`
- `payload_len_bytes` is varbit-encoded
- best current synthetic `0x13` payload is:
  - `0x27`
  - followed by `u64 lobby_id / descriptor key`

Important semantic constraint:

- that `u64` is a real `lobby_id`
- it becomes the browser entry key and must stay consistent through later connect/join handling
- so future synthetic discovery cannot use a throwaway random id unless the rest of the synthetic flow also consistently reuses it

Encouraging browser-visibility result:

- current evidence suggests the first visible discovery milestone may only require:
  - a consistent `u64 lobby_id`
  - a valid fixed-width 0x25 browser name field
- later `LanLobby` sync (`0x19`, `0x14`, `0x15`) is likely needed for real join/bootstrap, but not just to make a lobby appear in browser results

Disabled scaffold status:

- native code now contains a disabled `SyntheticDiscoverReplyPlan`
- it currently captures only the static pieces we are confident about:
  - envelope tag `0x3EE5`
  - reserved `24-bit 0`
  - 64-bit descriptor key
  - 0x25-byte browser name field slot
  - browser event token `0x27`
- it does not send anything yet
- native code also now contains a disabled `send_synthetic_discover_reply_explicit(...)` helper that:
  - builds the current best-effort outer `0x13` frame
  - resolves transport vfunc `+0x30`
  - would send to an explicit `sockaddr_in6`-like destination if the feature flag were enabled
- native code now also includes a fixed-width browser-name helper that pads a 36-byte text field and terminates with NUL in the final byte, matching the parser’s current expectations

### Stage 3: Add custom inner callback

Use per-channel registration vfunc `+0x50`.

Known stock registrations:

- `0x14056e6e0` : inner lobby packet callback
- `0x14056f9e0` : outer channel-envelope follow-up callback

Custom goal:

- accept incoming inner `0x1c` join request if the stock path truly no longer does
- synthesize either:
  - `0x17` positive join reply (1-bit accepted)
  - or `0x18` reject control packet

### Stage 4: Bridge into game session host

If discovery and join request can be synthesized successfully, the next problem is no longer LAN reachability but attaching the resulting lobby to game-session host state.

Likely surviving helpers:

- `make_game_session_host`
- `set_game_session_host`
- `game_session_host`

Current best model:

- `GameSession +0xd0` is the live `_game_session_host` peer-id field
- `make_game_session_host` most likely promotes an existing peer entry to host state instead of allocating a separate host object
- `LanLobby:set_game_session_host(...)` stores a separate opaque 64-bit `%016llx` value at `LanLobby+0x80`

Implication:

- the eventual bridge may need both:
  - a valid promoted host peer inside `GameSession`
  - a matching opaque host/session identifier in `LanLobby+0x80`

This is the point where the native patch may need to create or inject a host-side game-session handle that Lua can then expose through `LanLobby:set_game_session_host(...)`.

## Packet-level working model

### Outer layer

- `0x10` connectionless connect request
- `0x11` request-connection / connect-to-server reply
- `0x12` discover_lobby
- `0x13` discover_lobby_reply
- `0x14` outer lobby channel envelope

### Inner layer after outer `0x14`

- `0x14` lobby_data
- `0x15` lobby_member_data
- `0x17` lobby_request_join_reply
- `0x18` reject/control packet
- `0x19` lobby_members
- `0x1c` join_request send helper path, with payload currently decoded as:
  - `6 bits` value `0x16`
  - `64 bits` value from `LanLobby+0x58`
  - `16 bits` local port from `getsockname + ntohs`

Known reply shapes:

- inner `0x17` reply payload: `1-bit accepted`
- inner `0x18` reject/control payload: empty beyond opcode

Reusable reply-side helper:

- a stock inner `0x17` join-reply builder/emitter helper still exists at `0x1406c56f0`
- so the likely hybrid split is:
  - fully synthetic outer `0x13`
  - possibly reused stock inner `0x17`

Known post-join sync shapes:

- inner `0x14` (`lobby_data`) expects a chunked opaque blob and swaps it live when complete
- inner `0x15` (`lobby_member_data`) expects a per-member chunked opaque blob addressed by `u64 member_id`

Useful surviving stock helpers:

- `0x14056ca10` still builds/sends `lobby_members (0x19)`
- `0x14056ce00` / `0x14056ce60` still build/send `lobby_data`
- `0x14056d1b0` / `0x14056d200` still build/send `lobby_member_data`

Useful exposed `LanLobby` setters:

- `set_game_session_host` drives dirty flag `+0x648` -> stock `0x19` send path
- `set_data` drives dirty flag `+0x649` -> stock `0x14` send path
- `set_member_data` drives dirty flag `+0x64a` -> stock `0x15` send path

Important clarification:

- `lobby_host` and `game_session_host` are not the same field
- `lobby_host` resolves from the first lobby member entry
- `game_session_host` is the separate opaque `%016llx` value stored at `LanLobby+0x80`

Useful exposed channel APIs:

- `open_channel` -> stock native helper `0x14056dbf0`
- `close_channel` -> stock native helper `0x14056ddb0`
- `kick` -> stock native helper `0x14056dfb0`, using the same `0x18` control packet path

Implication:

- if a synthetic host can create a real `LanLobby`, some of the later per-peer channel setup may be drivable through these exposed stock APIs instead of fully custom transport calls

Implication:

- a minimal synthetic host that only emits `0x13` and `0x17` may still fail to progress cleanly unless the client also receives plausible follow-up `0x19`, `0x14`, and `0x15` traffic
- the best-case hybrid approach is now:
  - synthesize missing discovery/join request receive/reply pieces
  - then reuse surviving stock builders for `0x19`, `0x14`, and `0x15`, ideally by driving the real `LanLobby` dirty flags through stock setters

## Static constraints

- no active stock receive path was found for outer `0x12`
- no active stock receive path was found for inner `0x1c`
- no stock sender for inner `0x17` was found in the active LAN sender cluster

These three constraints are the strongest argument that synthetic native host behavior is required.

## Best next implementation step

Implement explicit-load diagnostics plus a no-op custom callback registration attempt on the deep transport interface to confirm:

1. token-based callback registration works at runtime
2. custom callbacks coexist with stock ones
3. the callback ABI inferred from static analysis is correct

Once that succeeds, the first functional experiment should be synthetic handling of outer `0x12` -> send `0x13`.

## Current validation scaffold

Implemented in the current native build:

1. `dxva2.dll` proxy now schedules a delayed companion load automatically.
2. Delay is currently `20000 ms` after proxy initialization.
3. `SoloPlayNativeHostPatch.dll` starts a worker thread after initialization.
4. The worker polls for a live `LanClient` global.
5. Once available, it resolves both:
   - `LanClient+0x68` control/session transport
6. It attempts a harmless outer callback registration only on the control/session transport using vfunc `+0x38`.
7. The registered callback only logs raw argument values and returns `0`; it does not parse reader state or mutate packet state.

Important runtime lesson from the first dual-transport attempt:

- before sign-in / before transport initialization, `LanClient+0x18` may temporarily contain a sentinel-like value such as `0x1`
- naive registration against that field caused a crash inside `SoloPlayNativeHostPatch.dll`
- callback-side reader decoding also proved unsafe while the exact ABI remained uncertain
- the validator is now narrowed back to the safer control transport path and raw-argument logging only

Stable current control-callback interpretation:

- `arg2` = callback class/token discriminator
- `arg3` = remote/source id
- `arg4` = opaque route/context pointer
- `arg6` = real event code

This is now stable enough that later synthetic work can target control events without guessing at every argument position again.

Next browser-validation strategy:

- do not attach to `LanClient+0x18` by default
- only enable browser/discovery callback registration behind an explicit native feature flag
- require all of the following before arming it:
  - browser transport pointer is plausible
  - browser transport vtable is plausible
  - control transport registration is already stable in the same run
- log raw browser callback arguments first; do not parse reader state yet
- use stock transport unregister vfunc `+0x68` for the browser callback token if the experiment is later enabled

Likely next hook target after browser validation:

- primary: browser connectionless handler entry `0x14056f5d0`
- fallback surgical bypass: reject gate `0x14056f5fe`

Hooking groundwork now present:

- native code contains a minimal inline-jump detour utility (unused for now)
- it can install/remove a 12-byte `mov rax, imm64 ; jmp rax` patch with `VirtualProtect` + `FlushInstructionCache`
- this is intended to support a future deliberate hook of `0x14056f5d0` or `0x14056f5fe`

Current code state:

- the browser/discovery callback scaffold now exists in native code
- it is still disabled by `kEnableBrowserValidation = 0`
- enabling it later will be a small intentional diff rather than another structural rewrite
- disabled unregister helpers now exist for both control and browser callback tokens, using stock transport vfunc `+0x68`

Runtime file flags now supported:

- create `binaries/SoloPlayNativeHostPatch.enable_browser_callback`
  - enables browser transport callback registration in addition to the safe control callback
- create `binaries/SoloPlayNativeHostPatch.enable_discovery_reply`
  - enables browser callback registration
  - enables synthetic `0x13` reply sending on observed browser event `0x12`

Safety gates on those flags:

- browser transport must look plausible
- browser transport callback vfunc must look plausible
- control callback must already be registered successfully
- browser transport must appear valid for 3 consecutive worker polls before registration is attempted

Latest browser attachment correction:

- browser transport resolution no longer trusts `LanClient+0x18` first
- it now prefers the recovered `create_wan_client` owner chain:
  - global owner -> `+0x418` nested object -> `+0x68` backend transport
- `LanClient+0x18` remains only a fallback source

## First Two-Client Discovery Smoke Test

Host client setup:

1. Create empty marker files in `binaries/`:
   - `SoloPlayNativeHostPatch.enable_browser_callback`
   - `SoloPlayNativeHostPatch.enable_discovery_reply`
2. Launch with the normal `dxva2` override.
3. Wait at least `20-30s` after reaching menu/hub for delayed patch load and worker polling.

Second client setup:

1. Launch normally without the marker files.
2. Run `/solo_lan_browser_refresh`.
3. Run `/solo_lan_browser_status`.

Expected host log milestones:

- `feature browser callback = 1`
- `feature discovery reply = 1`
- `validation control callback registered`
- `validation browser transport waiting for stability`
- `validation browser callback registered`
- on discovery:
  - `validation browser event = 0x12`
  - `synthetic discover destination ...`
  - `synthetic discover reply sent`

Expected second-client signs of success:

- browser status shows at least one result/entry
- the visible browser name should come from the fixed synthetic name field (`Synthetic Solo Host` unless later changed)

Important architectural split:

- `Network.init_wan_client` returns a script-facing `LanClient` wrapper object
- the actual browser/discovery callback registration happens on the underlying `LanTransport` backend, not on the wrapper object itself
- this makes browser-side synthetic work a little cleaner conceptually: hook the backend transport, not the higher-level wrapper

Expected log flow on a successful validation run:

- `dxva2 proxy scheduled delayed patch load`
- `dxva2 proxy attempting delayed patch load`
- `companion patch loaded`
- `companion patch initialized`
- `validation worker thread scheduled`
- `validation worker thread started`
- `validation lan client = ...`
- `validation browser transport = ...`
- `validation control transport = ...`
- `validation browser callback registered`
- `validation control callback registered`

Expected packet-observation logs afterward:

- `validation browser callback count = ...`
- `validation browser event = ...`
- `validation control callback count = ...`
- `validation control event = ...`

Lua-side triggers now available through DMF commands:

- `/solo_lan_browser_refresh`
  - creates a persistent `LanLobbyBrowser` if needed
  - calls `browser:refresh()`
  - dumps browser status to the console log
  - intended purpose: generate outer LAN discovery traffic for the native validation callback

- `/solo_lan_browser_status`
  - dumps current persistent browser status

- `/solo_lan_browser_clear`
  - destroys the persistent browser created for validation

If callback registration succeeds but no packet logs appear, the next likely issue is either:

- the wrong callback class/registration bucket was used, or
- the runtime path under test did not generate matching outer traffic
