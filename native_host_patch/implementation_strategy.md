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

## Post-update RVA rebase status

The core native headers have now been rebased against the updated Darktide build.

Currently treated as validated against the new binary:

- LAN/browser wrappers
- browser callback/parser thunks
- outer/channel callback heads
- `LanLobby`, `LanLobbyBrowser`, and `LanClient` wrapper vtables/base vtables
- low-level packet helpers
- key `GameSession` helper RVAs
- key lobby/member send/build helpers

This does not make the binary future-proof, but it does mean the active implementation work is no longer resting on obviously stale pre-update addresses.

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

### Missing-host-role split

Current best decomposition:

**Likely reusable stock Darktide pieces**
- outer control / lightweight handshake listener
- create-channel / bind / listener setup
- outer channel-envelope routing
- post-join lobby replication and dirty-flag senders

**Likely synthetic pieces we still must recreate**
- host discovery receive (`outer 0x12`)
- host discovery-reply send (`outer 0x13`)
- host join-request receive/accept (`inner 0x1c` -> `0x17`)

This is a much narrower target than “rebuild all host networking”.

## VT2-guided Darktide host bootstrap target

Current strongest field correspondences from VT2 host/create constructor to Darktide are:

- generated/random lobby id -> `LanLobby+0x58`
- create/host state -> `LanLobby+0x7C`
- create/max-members config -> `LanLobby+0x78`
- backend transport/listener owner -> `LanLobby+0x68`

Current code-side planning now reflects that with a disabled `SyntheticHostLanLobbyPlan` describing:

- `lobby_id`
- `max_members`
- `state`
- `game_session_host`
- whether control/channel/join listener families are expected to be registered

This does not create a host lobby yet; it formalizes the intended host-mode target shape for later implementation.

### Join-side refinement

Latest comparison against VT2 host-side join handling suggests:

- the best Darktide hook point for synthetic join acceptance is the inner `LanLobby` dispatcher `0x14056E6E0`
- specifically, synthetic handling should normalize/redirect missing inner `0x1c` request handling into the existing `0x17` accept/deny state path at `0x14056EBC6`

Important caveat:

- a synthetic `0x17` reply alone is not enough for coherent later traffic
- host-side admission/state mutation equivalent to VT2’s pending->member promotion is also required so that later `0x19`, `0x14`, and `0x15` packets make sense to the client

New Darktide-native admission lead:

- `0x14056E0D0` now looks like a plausible direct synthetic admission/add primitive

Current best recipe:

1. prepare a zero-safe `0x60`-byte temp record
2. set:
   - `temp+0x00 = peer_id`
   - owner/interface pointers at `temp+0x40` and `temp+0x58` to `this+8`
3. pass desired port in `R8D`
4. ensure `this+0x70` already contains a matching `peer_id -> sockaddr` entry
5. call `0x14056E0D0(this, &temp, port)`

If those preconditions hold, `0x14056E0D0` should:

- append the live member record
- resolve/write address data
- set dirty flags that feed stock `0x19`, `0x14`, and `0x15` senders

Important address-side detail:

- the supporting `peer_id -> address` entry must use a full `0x1c` `sockaddr_in6`
- for IPv4, stock code expects IPv4-mapped IPv6 form
- a minimal shortened blob is not enough for stock parser/format helpers

Current code-side primitive set now exists for later admission work:

- build IPv4-mapped `sockaddr_in6`
- upsert `peer_id -> address` via `0x140587380`
- synthetic join accept plan struct
- synthetic admission record plan struct
- direct `perform_synthetic_member_admission(...)` helper that chains:
  - address upsert
  - `0x14056E0D0`
- `build_synthetic_join_accept_packet(...)` helper for the minimal inner `0x17` accept frame
- `send_synthetic_join_accept(...)` helper that:
  - finds the admitted member record
  - reads its send-target handle at `member+0x08`
  - builds the stock-like `0x17` packet with `0x1406C5780`
  - emits it through transport `+0x108`

## Current highest-impact strike-team host mismatch

At the current level of implementation, the highest-impact remaining mismatch on the strike-team host path is:

- remote `profile_chunks_array`

Reason:

- loading progression explicitly waits for profile sync completion
- the current fallback is only a minimal stock-like stub, not a real remote profile

So the next high-value refinement on the strike-team path is a better remote profile source, not reserve/claim semantics or additional top-level host events.

## Parallel strike-team/session direction

In parallel to the LAN/browser work, Darktide source now suggests a second viable direction:

1. keep the current strike-team party state intact
2. continue to use `PartyImmateriumManager.join_game_session()` as the top-level entrypoint
3. redirect `PartyImmateriumMissionSessionBoot` at the point where it fetches dedicated server details
4. provide a player-hosted target description instead of backend dedicated-server details
5. reuse the existing browser/lobby/client/session boot path from there if possible

This makes the strike-team path look less like “invent a new session flow” and more like “replace the dedicated-server-details phase with a player-hosted equivalent”.

Current narrowing result:

- the experimental host’s `"connected"` event path is likely already enough to engage the stock `LoadingRemoteStateMachine`
- so the most likely remaining blockers are not top-level connection-manager compatibility
- they are instead later stock expectations around:
  - real lobby/channel scaffolding
  - host/session-host state
  - loading/profile/package sync completion

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

Current synthetic split in code:

- discovery uses `SyntheticDiscoverReplyPlan`
- join acceptance now has a separate disabled `SyntheticJoinAcceptPlan`
- this mirrors the current architectural conclusion that discovery and join acceptance are distinct missing host roles

## Current best minimal synthetic host sequence

The strictest likely ordering for a first minimally coherent player-host flow is now:

1. Host browser callback sees actionable browser event `0x11`
2. Host learns callback `source_id`
3. Host resolves `source_id -> sockaddr_in6`
4. Host sends synthetic outer browser reply (`0x13` path)
5. Client proceeds into connection/request path
6. Host reaches the earliest stable request-connection receive/parser path where peer id and port coexist
7. Host upserts `peer_id -> sockaddr_in6` with `0x140587380`
8. Host admits/adds the peer with `0x14056E0D0`
9. Host sends synthetic inner `0x17` accept reply
10. Stock `0x19 / 0x14 / 0x15` follow from dirty flags and existing senders

Why this order matters:

- `0x14056E0D0` requires the address-table entry to already exist
- synthetic `0x17` alone is not enough
- admitting first gives the later stock member/lobby senders coherent state to serialize

## Canonical option-1 staged host sequence

The current best end-to-end host-side plan is now:

1. capture the real browser callback registration via `0x140351330`
2. on browser callback event `0x11`, resolve `source_id -> sockaddr_in6`
3. send synthetic outer `0x13`
4. wait for the client to enter the request-connection / join-side parser path
5. at `0x1403383D0-0x140338C52`, use the first point where stable `peer_id` and source `port` coexist
6. call `0x140587380` to upsert `peer_id -> sockaddr_in6`
7. call `0x14056E0D0` to admit/add the member into the real `LanLobby`
8. send synthetic inner `0x17` accept reply
9. let stock `0x19 / 0x14 / 0x15` senders take over via dirty flags and stock setters

This is now the clearest integration target for option 1.

Current most concrete join-side hook target:

- request-connection parser post-success edge around `0x1403387F9-0x140338818`

Current input mapping at that edge:

- `peer_id` = stable parser source id
- `port_host_order` = recoverable parsed request port
- `ipv4_host_order` = derive from resolved `sockaddr_in6` (`addr[12..15]`)
- `lan_lobby` = resolve by matching parsed lobby id against `LanLobby+0x58`

Current code-side state:

- the host-side post-discovery path is now represented by a single disabled helper:
  - `run_synthetic_join_sequence(...)`
- it performs, in order:
  1. `perform_synthetic_member_admission(...)`
  2. `send_synthetic_join_accept(...)`

So the post-discovery host join sequence now exists in code as one coherent step, even though it is not yet wired to a live hook.

Host-side state checkpoints now logged by those helpers:

- `HOST_ADMISSION_PERFORMED`
- `HOST_ADMISSION_STATE`
- `HOST_JOIN_ACCEPT_SENT` / `HOST_JOIN_ACCEPT_FAILED_*`
- `HOST_JOIN_ACCEPT_STATE` / `HOST_JOIN_ACCEPT_FAILED_STATE`

Those lines report live `LanLobby` state after admission / `0x17`, including:

- `lobby_id`
- `game_session_host`
- `members_count`
- dirty flags

Next hook target now scaffolded:

- request-connection parser window
  - start: `0x1403383A0`
  - end: `0x140338C22`
- current code includes a disabled hook scaffold for this parser window
- once enabled and refined, this is the intended live insertion point for calling `run_synthetic_join_sequence(...)`
- a small logging stub (`log_request_connection_hook_plan(...)`) is now present to reflect the intended extracted inputs there:
  - `peer_id`
  - `port`
  - `lobby_id`

Planned safe-fail behavior:

- the future parser hook should only call the synthetic join sequence if all of these are present:
  - valid `lan_client`
  - valid transport
  - non-zero `lobby_id`
  - non-zero `peer_id`
  - active `LanLobby*` resolved from `lobby_id`
  - resolvable `peer_id -> sockaddr_in6`
- otherwise it should log the first missing prerequisite and return without mutating state

Additional helper now present:

- `find_active_lan_lobby_by_id(...)`
- `run_synthetic_join_sequence_from_lobby_and_peer(...)`

This closes the last obvious data gap in the canonical host sequence by giving the future hook path a stock-shaped way to resolve:

- `LanLobby*` from `lobby_id`
- `sockaddr_in6` from `peer_id`
- then run admission + `0x17` as one operation

Addressing fallback now known:

- stock code also has a `u64 source_id -> sockaddr_in6` resolution path (`0x14056e0d0` + `0x140587380`)
- and at least one separate stock reply helper that sends directly by `peer/source id` (`0x140353A80`)
- so future reply work may have two viable addressing strategies:
  1. resolve callback source id to explicit sockaddr and use `+0x30`
  2. find/reuse a peer-id-based send primitive if it can be generalized to browser traffic

Current browser-side conclusion:

- the browser callback `source_id` is now confirmed to be the correct key into the backend transport’s peer-address table
- explicit browser reply sending can therefore use:
  1. `source_id`
  2. resolve `source_id -> sockaddr_in6` via the transport table
  3. call explicit send (`+0x30`) with flags `0`

This part of the discovery reply path is no longer speculative.

Current status of the discovery responder:

- structurally complete up to explicit send
- remaining uncertainty is now runtime-only:
  - whether the captured browser transport is the true live browser callback transport in the armed run
  - whether the synthetic `0x13` frame is accepted end-to-end by the retail browser path

Current host/client sequence checkpoints:

Host should now log:
- browser callback registration
- browser callback event `0x11`
- synthetic reply send attempt
- synthetic reply send count / source id / lobby id

Client should now log:
- `HOSTED_FOLLOW_RECEIVED ...`
- browser refresh result
- visibility of the expected hosted lobby
- `HOSTED_FOLLOW_READY_TO_JOIN ...`
- handshake/connect attempt lines

That makes the next runtime correlation much more deterministic than before.

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

Latest live-event correction:

- the first synthetic browser reply trigger should now be attempted on observed browser callback event `0x11`
- not on the earlier guessed wire-level discovery id `0x12`

Encouraging browser-visibility result:

- current evidence suggests the first visible discovery milestone may only require:
  - a consistent `u64 lobby_id`
  - a valid fixed-width 0x25 browser name field
- later `LanLobby` sync (`0x19`, `0x14`, `0x15`) is likely needed for real join/bootstrap, but not just to make a lobby appear in browser results

## Client follow path after lobby join

Current source-level conclusion:

- reaching `Network.join_lan_lobby(lobby_id)` is not enough
- stock mission/hub boot then explicitly creates `ConnectionClient:new(...)`

So the eventual automatic follow path likely needs this ordering:

1. browser result visible
2. handshake/connect
3. `Network.join_lan_lobby(...)`
4. wait for lobby `joined`
5. explicit `ConnectionClient` bootstrap
6. `Managers.connection:set_connection_client(...)`
7. stock `LoadingClient` path

The main currently missing input for step 5 is a valid `jwt_ticket` / server-details equivalent.

Current seeding rule in code:

- prefer a real engine-lobby id if one is already present
- otherwise fall back to the existing stable synthetic `u64` seed
- keep the browser-visible name synthetic for the first visibility milestone unless a better source is proven necessary

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
- create `binaries/SoloPlayNativeHostPatch.enable_registration_capture`
  - enables an opt-in hook on the shared callback registration implementation
  - captures the real browser/context/backend transport when the browser callback thunk is registered

Safety gates on those flags:

- browser transport must look plausible
- browser transport callback vfunc must look plausible
- control callback must already be registered successfully
- browser transport must appear valid for 3 consecutive worker polls before registration is attempted
- native worker now polls more slowly and logs transport state only on change to reduce startup overhead
- registration capture remains fully off unless the dedicated marker file is present

Default diagnostics policy now:

- passive browser validation is on by default
- passive transport/browser/session state logging is on by default
- active synthetic behavior still remains opt-in:
  - synthetic discovery reply
  - registration capture hook
  - request-connection parser hook

Runtime cleanup behavior:

- if browser-host feature flags are turned off while the game is still running, the native worker now unregisters the browser callback automatically
- this is intended to keep later solo/offline launches from inheriting stale browser-host test state in the same process

Latest testing ergonomics improvement:

- the native worker now hot-reloads feature flags during polling instead of reading them only once at startup
- `Start With Strike Team (Host)` now automatically enables the main host-test mode flag before it boots the hosted path
- `Start With Strike Team (Host)` now also creates/refreshes the host-side browser object immediately in the same run
- `Start With Strike Team (Host)` now dumps a combined snapshot of:
  - native feature state
  - browser status
  - engine lobby id
  - mission context
- immediately after installing the experimental host boot it also dumps:
  - current `_session_boot`
  - current `_session`
  - current `Managers.loading` host/client state
- practical effect: the host no longer needs a restart just to arm the native browser/discovery side

Latest browser attachment correction:

- browser transport resolution no longer trusts `LanClient+0x18` first
- it now prefers the recovered `create_wan_client` owner chain:
  - global owner -> `+0x418` nested object -> `+0x68` backend transport
- `LanClient+0x18` remains only a fallback source

Next likely pivot if runtime still fails to arm browser callbacks:

- stop resolving browser transport indirectly
- capture the real `LanLobbyBrowser` object at creation time at `0x14048AF80`
- derive the backend transport directly from `browser+0x50`

Cleaner variant of that pivot now identified:

- hook the shared callback registration implementation at `0x140351330`
- filter for:
  - `callback_thunk == 0x14056F5B0`
  - `reg_class == 1`
- capture:
  - transport (`RCX`)
  - browser/context pointer (5th arg)

Current behavior after capture:

- once the registration-capture hook sees the browser callback registration, the browser resolver now prefers that captured transport over any indirect global-source heuristic
- if registration capture is explicitly enabled, browser callback arming now waits for a captured browser transport instead of falling back to indirect resolution first

Latest browser event-delivery nudge:

- after successful browser callback registration, native code now performs a one-shot browser refresh on the captured browser object using the real native refresh path
- rationale: ensure at least one post-registration refresh occurs in the same run, instead of depending on perfect manual timing

This is likely safer than patching the middle of `create_lobby_browser` itself.

Latest confidence increase:

- browser thunk `0x14056F5B0` appears unique in this build
- the registration helper at `0x140351330` only uses the first 5 logical args and preserves the `out_handle` return convention
- so the registration-capture hook is now the cleanest known browser attachment strategy for option 1

Current runtime policy again:

- `host_test_enabled` now re-arms registration capture automatically again, because the relevant RVAs were rebased and the stale-address risk was the main reason it had been forced off

Important same-run fix:

- the native worker now attempts `maybe_install_registration_capture_hook()` after each feature-flag reload
- this means pressing the hosted button in the current run can arm capture without needing a restart just for hook installation
- worker poll interval is now `1s` instead of `3s`, to reduce same-run delay between the hosted button press and native browser/capture arming

## First Two-Client Discovery Smoke Test

Host client setup:

1. Create empty marker files in `binaries/`:
   - `SoloPlayNativeHostPatch.enable_browser_callback`
   - `SoloPlayNativeHostPatch.enable_discovery_reply`
2. Launch with the normal `dxva2` override.
3. Wait at least `20-30s` after reaching menu/hub for delayed patch load and worker polling.
4. Browser priming is manual to reduce menu/login overhead; use `/solo_native_host_prime` only after the host is fully loaded.

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
