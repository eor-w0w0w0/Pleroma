# VT2 Binary LAN Notes

This file records the strongest binary-level LAN host-side findings from the installed
`vermintide2_dx12.exe` and why they matter for Darktide.

## Main result

Vermintide 2 retains a real Lua-exposed `create_lan_lobby(...)` path that Darktide does not.

This is the clearest binary evidence so far that the host-side LAN creation/bootstrap logic
was still present in VT2 as shipped, while Darktide appears to retain mostly the browse/join side.

## Relevant VT2 strings present in binary

Observed in `Warhammer Vermintide 2/binaries_dx12/vermintide2_dx12.exe`:

- `create_lan_lobby`
- `join_lan_lobby`
- `create_lobby_browser`
- `discover_lobby`
- `discover_lobby_reply`
- `request_connection_reply`
- `lobby_request_join`
- `lobby_request_join_reply`
- `LanLobby`
- `LanLobbyBrowser`
- `LanClient`
- `LanTransport`
- `make_game_session_host`
- `set_game_session_host`
- `lobby_host`

Strings notably absent in both VT2 and Darktide as plain strings:

- `host_lan_lobby`
- `start_server_handshake`
- `server_handshake_status`

## Lua wrapper cluster

The `create_lan_lobby` string is not dead data; it is part of the same Lua registration block as
`join_lan_lobby` and `create_lobby_browser`.

Recovered wrapper relationships:

- `create_lan_lobby`
  - string anchor: `0x1408B4658`
  - registration stub: `0x140178E4C`
  - Lua handler: `0x140177E40`
  - constructor path: `0x140230EA0`

- `join_lan_lobby`
  - string anchor: `0x1408B4698`
  - registration stub: `0x140178E9A`
  - Lua handler: `0x140177F90`
  - constructor path: `0x140231310`

- `create_lobby_browser`
  - string anchor: `0x1408B4628`
  - registration stub: `0x140178F36`
  - Lua handler: `0x1401789E0`

## VT2 `LanLobby` constructor comparison

### `create_lan_lobby` path (`0x140230EA0`)

High-confidence behavior:

- allocates a `0x648`-byte `LanLobby` object
- writes host/create-specific fields:
  - `this+0x68 = 1`
  - `this+0x6C = caller-provided dword`
  - `this+0x70 = 2`
- generates a random 64-bit value into `this+0x48`
- registers several transport callbacks immediately, storing handles in:
  - `+0x5B8`
  - `+0x5BC`
  - `+0x5C0`
  - `+0x5C4`
- performs listener/bind-style setup via helper `0x1402336E0`

Strong interpretation:

- `+0x68` is host/create mode flag
- `+0x6C` is likely max-members or create-time configuration count
- `+0x70 = 2` is the create/host state
- `+0x48` is a generated lobby id / host instance id

### `join_lan_lobby` path (`0x140231310`)

High-confidence behavior:

- constructs the same `LanLobby` type
- writes join-specific fields:
  - `this+0x68 = 0`
  - `this+0x6C = 0`
  - `this+0x70 = 1`
- copies an incoming 64-bit value into `this+0x48`
- registers fewer callbacks initially
- defers more setup into helper `0x140231B10`
- `0x140231B8B` can change `this+0x70 = 3` on join lookup failure

Strong interpretation:

- `+0x70 = 1` is join state
- `+0x70 = 3` is failure state
- join path consumes an externally supplied lobby id instead of generating one

## Why this matters for Darktide

Darktide currently appears to retain:

- browser/discovery send path
- explicit connect request path
- lobby join constructor
- lobby/channel/session replication after join

What Darktide appears to lack:

- a Lua-exposed `create_lan_lobby`
- a clearly active host-side discovery receive/reply path
- a clearly active lobby join-request receive path

VT2 suggests the missing Darktide host side was likely once a sibling of the existing join path, not a completely separate subsystem.

## Reuse potential

### Most promising

Behavior-level porting, not raw binary transplant.

The clearest candidate ideas to port from VT2 into Darktide are:

1. **Host/create constructor semantics**
   - mode flag field
   - generated lobby id field
   - create-state field

2. **Immediate callback registration pattern**
   - host-side callbacks registered at create time rather than after join

3. **Listener/bind setup helper role**
   - whatever `0x1402336E0` represents in VT2 is likely conceptually similar to the missing Darktide host bootstrap step

### Less promising

Raw code transplant by copying bytes from VT2 into Darktide.

Reasons:

- different object size (`0x648` in VT2 vs `0x650` in Darktide)
- different vtable layouts and callback targets
- different surrounding manager/global layouts
- likely different transport backend details between the two games

## Host-side receive plumbing in VT2

Global protocol table scan showed active message/schema entries for:

- `request_connection_reply`
- `discover_lobby`
- `discover_lobby_reply`
- `lobby_request_join`
- `lobby_request_join_reply`

This is stronger than what we currently have in Darktide because VT2 also has the explicit `create_lan_lobby` creation path alongside those messages.

## Working conclusion

VT2 does not give us a safe byte-for-byte transplant target, but it does give us a convincing template for what Darktide’s missing host side probably looked like:

- create-host lobby constructor
- generated lobby id
- host-mode state flag
- immediate callback/listener registration
- discovery/join receive plumbing active in the same LAN subsystem

That makes VT2 the best reference for a Darktide behavior-level reimplementation of the missing host-side LAN bootstrap.
