# Native LAN Reverse Engineering

Current focus: recover the native-only player-host / LAN lobby bootstrap that still exists in retail `Darktide.exe` but is not exposed to Lua.

## Stable entry point

- `dxva2.dll` proxy is currently the stable no-injector native entry point under Proton.
- `xinput9_1_0.dll` proxy proved unstable even when acting as a pure forwarder.

## Lua/native registration map

The Lua binding block around `0x14048b570` registers these runtime-facing LAN classes and methods.

### `Network`

- `join_lan_lobby` -> wrapper `0x14048a1f0`
- `leave_lan_lobby` -> wrapper `0x14048a520`

### `LanClient`

- `create_lobby_browser` -> wrapper `0x14048aec0`
- `destroy_lobby_browser` -> wrapper `0x14048b070`

### `LanLobby`

- `state` -> wrapper `0x14048a600`
- `open_channel`
- `close_channel`
- `set_game_session_host`
- `game_session_host`
- `lobby_host`
- `members`
- `set_max_members`
- `kick`
- `set_member_data`
- `own_data`
- `member_data`

### `LanLobbyBrowser`

- `refresh`
- `num_lobbies`
- `lobby`
- `is_refreshing`
- `start_client_handshake`
- `client_handshake_status`
- `establish_connection_to_server`
- `disconnect`

## `LanLobby` object

### Construction

- `join_lan_lobby` allocates `0x650` bytes at `0x14048a25f`
- live vtable is written at `0x14048a2a8`
- live `LanLobby` vtable address: `0x140f44500`
- the wrapper explicitly sets `state = 1` at `0x14048a426`

### Teardown

- deleting-destructor wrapper: `0x14056c210`
- destructor/reset body: `0x14056c2e0`
- destructor body tears down transport handles, channel state, member arrays, and sub-objects before re-stamping base vtables

### State field

- state field offset: `+0x7c`
- wrapper `LanLobby.state()` reads `+0x7c` and indexes a 4-entry native string table at `0x140f444d8`

Decoded state names:

- `0 = creating`
- `1 = joining`
- `2 = joined`
- `3 = failed`

Important observation:

- no second confirmed native constructor/bootstrap path was found that creates a `LanLobby` in `creating` state
- the only confirmed live constructor path is `join_lan_lobby`
- all clearly identified native state writes in the LAN update cluster set `2` or `3`, not `0`

### Confirmed key fields

- `+0x58` : `lobby_id`
- `+0x60` : local peer id / own member id
- `+0x80` : opaque 64-bit game-session-host handle / peer id exposed to Lua as hex string

Supporting notes:

- `join_lan_lobby` stores the incoming 64-bit lobby id into `+0x58`
- `lobby_host` wrapper returns `members[0]`, not `+0x60`
- `+0x60` is used for self-vs-host comparisons and is therefore the local peer id
- `set_game_session_host` / `game_session_host` read and write `+0x80` as a `%016llx` hex-string value
- `lobby_host` remains a separate concept resolved from the first member in the member array

### Exposed setters and dirty flags

- `set_game_session_host` -> `0x14048a910`
  - writes `QWORD [this+0x80]`
  - sets dirty flag `BYTE [this+0x648] = 1`

- `game_session_host` -> `0x14048a9c0`
  - returns `QWORD [this+0x80]`

- `lobby_host` -> `0x14048aa20`
  - returns first member id from the member array, not `+0x80`

- `set_data` -> `0x14048a7e0`
  - updates lobby-level key/value map at `this+0x90`
  - sets dirty flag `BYTE [this+0x649] = 1`

- `data` -> `0x14048a8a0`
  - reads lobby-level key/value map at `this+0x90`

- `set_member_data` -> `0x14048acb0`
  - updates local member-data map at `this+0xa8`
  - sets dirty flag `BYTE [this+0x64a] = 1`

- `own_data` -> `0x14048ad70`
  - reads local member-data map at `this+0xa8`

- `member_data` -> `0x14048add0`
  - reads own map if `peer_id == this+0x60`
  - otherwise scans member array and reads the selected member’s live data buffer/state

### Channel wrappers

- `open_channel` -> `0x14048a660`
  - takes `Network` plus optional channel-name string
  - calls native helper `0x14056dbf0`
  - ultimately uses the deep transport object at `LanLobby+0x68`
  - creates/registers a lobby-scoped channel and stores the result in the channel map at `+0x5d0`

- `close_channel` -> `0x14048a740`
  - takes one integer channel id
  - calls native helper `0x14056ddb0`
  - removes the channel from the lobby-local map and invokes transport cleanup via vfuncs `+0x68` and `+0xC0`

### Other useful wrappers

- `set_max_members` -> `0x14048aba0`
  - host-only/local-ownership-gated
  - writes scalar `DWORD [this+0x78]`
  - does not directly broadcast or resize arrays in the wrapper itself

- `kick` -> `0x14048ac10`
  - resolves a target peer/member and calls native helper `0x14056dfb0`
  - emits the same `0x18` control packet path used by the native reject/kick flow

## `LanClient` and `LanLobbyBrowser`

### Script-facing `LanClient` wrapper

- `Network.init_wan_client` is registered to `0x140514870`
- it constructs a `0xA0` script-facing wrapper tagged as `LanClient`
- live wrapper vtable: `0x140f44590`
- underlying backend object is created by `0x1405869b0`
- backend vtable family: `0x140f455d0` (`LanTransport`-family)

Important split:

- the script-visible `LanClient` wrapper is not itself the deep transport object
- the browser/discovery callback registration lives on the backend transport stored at wrapper `+0x10` / copied into browser `+0x50`

### Likely `LanClient` fields

- global LAN owner: `0x1412a4258`
- global `LanClient` singleton: `0x1412a43e0`
- `+0x18` : browser/discovery transport object
- `+0x68` : LAN transport pointer
- `+0x80/+0x88` : active lobby count / active lobby array
- `+0x90` : self/local peer id or current local identity
- `+0x590/+0x598` : known-peer count / known-peer array
- `+0x5a0` : known-peer owner/self record

### `LanLobbyBrowser`

- live vtable: `0x140f1f2f8`
- object size: `0xb8`
- constructor path: `create_lobby_browser` wrapper `0x14048aec0`
- refined fields:
  - `+0x48` : hash/index owner for browser entries
  - `+0x50` : browser/discovery transport pointer
  - `+0x58/+0x60` : browser result count / result array
  - `+0x68` : allocator / memory context
  - `+0x70` : callback registration handle
  - `+0x78` : cached 64-bit descriptor/lobby key value
  - `+0x80..+0xa4` : cached 0x25-byte textual browser/lobby name field
  - `+0xb0/+0xb4` : refresh flag / refresh timer-ish state

## `LanLobby` update / transport cluster

### Core regions

- `0x14056c6e0` : join/request bootstrap helper called from `join_lan_lobby`
- `0x14056d1b0` : helper used to emit/update member data payloads
- `0x14056d7b0` : `LobbyTransport::update` region
- `0x14056e0d0` : helper that refreshes host/member address data and sets dirty flags
- `0x14056e430` : helper that removes a member from the lobby member list by peer id
- `0x14056e6e0` : LAN lobby channel packet dispatch
- `0x14056f5d0` : connectionless packet parser
- `0x14056f2e0` : lobby discovery send path
- `0x14056eff0` : connectionless connect request send path

### High-confidence packet ids

Outer LAN layer:

- `0x10` -> connectionless connect request send path
- `0x11` -> request-connection / connect-to-server reply
- `0x12` -> lobby discovery send path
- `0x13` -> discover-lobby reply
- `0x14` -> outer lobby channel envelope routed by `0x14056f9c0`

Inner `LanLobby` packet dispatch at `0x14056e6e0` after the outer `0x14` envelope:

- `0x14` -> `lobby_data`
- `0x15` -> `lobby_member_data`
- `0x17` -> `lobby_request_join_reply`
- `0x18` -> reject/control packet with empty payload; receive side forces failure, send side exists at `0x14056e003`
- `0x19` -> `lobby_members`
- `0x1c` -> actual join-request packet sent by client bootstrap at `0x14056c926`

Recovered inner packet payloads:

- inner join request payload from `0x14056c6e0`:
  - `6 bits` value `0x16`
  - `64 bits` value from `LanLobby+0x58`
  - `16 bits` local socket port from `getsockname(...)+ntohs(...)`
- inner join reply payload at `0x14056ebc6`:
  - `1 bit` accepted flag
- inner reject/control `0x18` payload at `0x14056e003`:
  - no payload beyond the opcode itself

Recovered post-join data payloads:

- inner `0x14` (`lobby_data`) is a chunked opaque blob:
  - `4 bits` unknown/fragment metadata
  - `4 bits` size-bit-count
  - `size-bit-count bits` total blob size in bytes
  - `8 bits` chunk size in bytes
  - `chunk size` raw blob bytes
- accumulated staging buffer lives at `LanLobby+0xC0/+0xC4/+0xC8`
- completed live buffer lives at `LanLobby+0x90/+0x94/+0x98`

- inner `0x15` (`lobby_member_data`) is a chunked opaque blob targeted to one member:
  - `64 bits` member id
  - `4 bits` size-bit-count
  - `size-bit-count bits` total blob size in bytes
  - `8 bits` chunk size in bytes
  - `chunk size` raw blob bytes
- per-member live buffer is at member-record `+0x30/+0x34/+0x38`
- per-member staging buffer is at member-record `+0x48/+0x4C/+0x50`

Confirmed active-cluster send helpers:

- `0x14056ca10` : builds and broadcasts `lobby_members` (`0x19`)
- `0x14056ce00` : sends current live `lobby_data` buffer
- `0x14056ce60` : builds/serializes outbound `lobby_data`
- `0x14056d1b0` : sends current live per-member data buffer
- `0x14056d200` : builds/serializes outbound `lobby_member_data`

Recovered `0x19 lobby_members` wire shape from `0x14056ca10` / `0x14056e775`:

- `6 bits` opcode `0x19`
- `4 bits` bit-width of member count
- `member_count` encoded in that many bits
- repeated for each member:
  - `64 bits` member id from member-record `+0x00`
  - `4 bits` bit-width of address-string length
  - address-string length encoded in that many bits
  - raw address string from member-record `+0x14`
- trailing `64 bits` value from `LanLobby+0x80` serialized by the stock sender; static semantics now point to a separate game-session-host handle rather than the first member id

Important implication:

- the active LAN cluster still contains outbound builders for `0x19`, `0x14`, and `0x15`
- this means synthetic host work may be able to reuse stock post-join data builders even if discovery/join request handling must be synthesized manually

Important clarification:

- the immediate `0x1c` at `0x14056c926` is a `getsockname` name-length argument, not the serialized packet opcode
- the actual first serialized field in that payload is the `6-bit` value `0x16`

Related outer helpers:

- `0x14056f9c0` accepts only outer `0x14` and routes it to a specific lobby by channel id
- `0x14056ee70` is likely outer create-channel message handling
- `0x14056f5d0` is the active connectionless parser for `0x04`, `0x11`, and `0x13`

### Callback registration findings

- constructor-time transport callback registration uses transport virtual `+0x38`
  - `0x14048a41f` registers `0x14056e4f0`
  - `0x14048af80` registers `0x14056f5b0 -> 0x14056f5d0`
- per-channel callback registration uses transport virtual `+0x50`
  - `0x14056c8a2` registers `0x14056e6c0 -> 0x14056e6e0`
  - `0x14056f765` registers `0x14056f9c0 -> 0x14056f9e0`
- strongest current interpretation:
  - `+0x20` sends outer discovery-like payloads
  - `+0x30` sends outer explicit-destination payloads
  - `+0x38` registers outer connectionless/control callbacks
  - `+0x50` registers per-channel callbacks
  - `+0x78` starts/opens a peer/channel flow
  - `+0x100` accepts/installs a reply-created connection/channel state
  - `+0x108` sends assembled payloads
  - `+0x158` aborts/denies with a reason code
- registrations appear token-based and likely support multiple concurrent callbacks rather than a single overwrite-only slot
- the deep callback/transport interface is the separate object stored at `LanClient+0x68`, not `LanClient` itself
- `LanLobbyBrowser` uses a sibling transport from `LanClient+0x18`, not the `LanClient+0x68` transport

### Stable control callback ABI

Based on the latest safe runtime log plus the `0x14056e4f0` family:

- likely ABI:
  - `cb(self, arg2_token_class, arg3_source_id, arg4_route_ctx, arg5_reserved, arg6_event_code, arg7_reader)`
- stable meanings:
  - `arg2` = callback class/token discriminator
  - `arg3` = remote/source id
  - `arg4` = opaque route/context pointer
  - `arg5` = reserved/unused in this stock path
  - `arg6` = real event code
  - `arg7` = reader/payload object for events that carry payload

Observed stable control event codes:

- `0x04`
- `0x07`
- `0x11`
- `0x22`

Current best semantic guesses:

- `0x04` : payload-bearing connection/setup event accepted by stock control handler
- `0x07` : connect/handshake progress or connect-ack style control event
- `0x11` : payload-bearing control event accepted by stock handler
- `0x22` : disconnect / connection-lost / handshake-abort style control event

### Browser/discovery callback ABI

From the thunk `0x14056f5b0 -> 0x14056f5d0`:

- likely original transport ABI:
  - `cb(self, arg2_unused, arg3_source, arg4_unused, arg5_unused, arg6_event, arg7_reader)`
- thunked stock handler view:
  - `handler(browser, source, event_code, reader)`

Stable static conclusions:

- original `arg3` maps to source/peer id
- original `arg6` maps to the real browser event code
- original `arg7` maps to the reader/payload object
- original args `2`, `4`, and `5` are ignored by the stock thunk path

Known browser event handling in `0x14056f5d0`:

- `0x04`
- `0x11`
- `0x13`
- other values hit the stock unsupported/default path

Best hook points for synthetic `0x12` handling:

- `0x14056f5d0` entry: best general hook point for real custom browser event handling
- `0x14056f5fe` reject gate: best micro-patch point if the only goal is bypassing the stock `0x12` drop
- `0x14056f5b0` is only the thunk and is not a good hook target

### `establish_connection_to_server` limitations

- wrapper `0x14048b470` is only a thin Lua entrypoint
- Lua args provide a target endpoint string and port
- helper `0x1406c5600` builds a `sockaddr_in6`-shaped object from those args:
  - `+0x00` family `AF_INET6 (0x17)`
  - `+0x02` network-endian port
  - `+0x08..+0x17` IPv6 address bytes
  - `+0x18` scope id
- however, `0x14056efd0` still depends on browser-cached descriptor state from `+0x78` and `+0x80..+0xA4`
- practical implication: `establish_connection_to_server` is not a fully manual connect primitive; it only becomes useful after valid browser discovery state already exists

Important clarification:

- the fixed `0x25` browser blob carried in connectionless browser packets is treated as text by the parser, not as a sockaddr/endpoint string structure
- this text becomes the browser-visible name string path, not an internal key
- actual endpoint parsing for connect flows uses a separate path (`0x1406c5470` / `0x1406c5600`) and a `sockaddr_in6`-like object
- accepted parser fast-path wants exactly 36 non-zero bytes followed by the final NUL at byte `0x24`
- current synthetic name helper follows that rule by padding with spaces and terminating only at the final byte

### Minimum visible discovery result state

- strongest current evidence says a browser-visible discovery result only needs:
  - the `u64 lobby_id`
  - the fixed `0x25` browser-visible name field
- later `LanLobby` `0x14`/`0x15` sync does not appear to be required just for `LanLobbyBrowser.lobby()` visibility

Runtime caveat:

- at least one live run showed `LanClient+0x18 == 0x1` and `LanClient+0x68 == 0x0` during sign-in/start-screen
- this means these fields cannot be treated as valid object pointers until later runtime phases
- browser/discovery callback ABI remains less trusted than the control/session callback path and is currently left unhooked in the safer runtime-validation build

### Callback cleanup findings

- browser-side callback/token records are stored in vectors/maps rather than raw singleton fields
- important helpers:
  - `0x1405702f0` : append browser/token record
  - `0x140570990` : get/create hash entry by 64-bit key
  - `0x140570a20` : erase hash entry by 64-bit key
  - `0x140570bd0` : find hash slot by 64-bit key
  - `0x140570020` : resize/shrink vector and destruct removed entries
  - `0x14056f930` : element destructor for browser/token record type
  - `0x14056fb60` : stock browser cleanup loop that unregisters per-entry callbacks and destroys their records
- stock cleanup strongly suggests callback registrations are removable by returned token/object and are not fire-and-forget
- browser callback handle at `LanLobbyBrowser+0x70` is unregistered through transport vfunc `+0x68`
- browser refresh-reset cleanup uses the same `+0x68` vfunc for per-entry query/listener tokens, plus transport vfunc `+0xC0` for entry-local transport cleanup

### Peer/source-id to address mapping

- `0x140587380` is the concrete `u64 peer_id -> sockaddr_in6` map writer/update helper
- the table is a linear array keyed by `u64` at entry `+0x00`, with stored sockaddr body at `+0x08..+0x23`
- `0x14056e0d0` is the main callback-side resolver:
  - local/self case resolves to loopback
  - remote case scans the peer table for matching `u64 source_id`
  - on success it writes a real `sockaddr_in6` into the pending callback record

Potentially important separate primitive:

- `0x140353A80` is a stock “send request connection reply to %llx” helper that appears to send by `peer/source id` rather than explicit sockaddr
- this is currently tied to the connection service, not confirmed reusable for browser `0x13`, but it is the strongest stock example of source-id-based reply transmission found so far

## Packet/bitstream helpers

Reliable generic helpers recovered from LAN send/parse paths:

- `0x140332e30` : write up to 32 bits into a bitstream
- `0x140330de0` : write up to 64 bits into a bitstream
- `0x140332f20` : write raw blob bits into a bitstream
- `0x140330ed0` : read up to 32 bits from a bitstream
- `0x140331050` : read up to 64 bits from a bitstream
- `0x140330fb0` : skip bits in a bitstream
- `0x140330d80` : read raw bytes from a bitstream

Bitstream state fields are most likely:

- `+0x08` current cursor
- `+0x14` error/overflow flag
- `+0x18` bit state
- `+0x1c` scratch byte/word

Important clarification:

- `0x140e6bfc8` is not a packet helper; it is `getsockname`
- `0x140e6bef0` is not a packet helper; it is `ntohs`

## Transport send descriptor

Current best interpretation of the send call used through transport vfunc `+0x108`:

- `rcx = transport`
- `edx = destination/remote connection id`
- `r8d = system/channel id`
- `r9b = reliable flag`
- `[rsp+0x20] = payload pointer`
- `[rsp+0x28] = payload bit count`
- `[rsp+0x30] = flags/user/callback token`

The temporary writer/descriptor used by LAN send paths consistently looks like:

- `+0x00` start pointer
- `+0x08` current pointer
- `+0x10` capacity/limit
- `+0x14` overflow/error/status
- `+0x18` pending bit state

## Connectionless outer wire formats

### Explicit-destination send path `0x14056efd0`

This path uses transport vfunc `+0x30` and emits a fixed 54-byte frame.

Recovered wire layout:

1. `u16 opcode = 0x3EE5`
2. `u24 reserved = 0`
3. `u64 endpoint_key = *(u64*)(obj+0x78)`
4. `byte descriptor[0x25] = *(byte[0x25]*)(obj+0x80)`
5. `u4 trailer_kind = 0`
6. `u8 trailer_reserved = 0`
7. `u4 lenbits = 1`
8. `u1 payload_len = 1`
9. align to next byte
10. `byte payload[1] = { 0x21 }`

Confidence: high for this sender path specifically.

Important note:

- the outer runtime callback event codes (`0x04`, `0x11`, `0x13`, etc.) are not identical to this raw wire opcode field
- `0x3EE5` appears to be a lower-level connectionless envelope marker

Confirmed high-level token encoding:

- token byte is encoded as `(event_id << 1) | 1`
- confirmed examples:
  - `0x10 -> 0x21`
  - `0x12 -> 0x25`
- strongest current inference:
  - synthetic outer `0x13` would use token byte `0x27`

### Nested browser service packet framing

Inside the connectionless envelope, browser/service traffic uses:

1. `service_id` (`4 bits`)
2. `reserved/version` (`8 bits`, must be `0`)
3. `len_bits` (`4 bits`)
4. `payload_len_bytes` (`len_bits bits`)
5. `payload[payload_len_bytes]`

Confirmed service ids from the parser table:

- `0 = lobby browser`
- `1 = lobby`
- `2 = channel`
- `3 = connection`
- `4 = game_session`
- `5 = game`
- `6 = authentication`
- `7 = eac`
- `8 = eos_eac`
- `9 = voip`

Best current synthetic `0x13` browser-payload model:

- `service_id = 0` (`lobby browser`)
- `reserved/version = 0`
- `payload_len_bytes = 9`
- payload bytes:
  - `byte 0 = 0x27` (`browser event 0x13` token)
  - `bytes 1..8 = u64 lobby_id / descriptor key`

Semantics of that `u64` field:

- it is a real `lobby_id`, not a peer id and not an arbitrary browser-only token
- the browser result table uses it as the main entry key
- later connect/join flow expects it to remain consistent with the discovered browser entry and associated state

### Remaining uncertain packet

- `0x18` is no longer considered unresolved; it is most likely a reject/control packet, not `lobby_request_join`

## Relevant log strings

### Lobby state / flow

- `creating`
- `joining`
- `joined`
- `failed`
- `request_join sent`

### Transport / parser logs

- `LobbyTransport::update`
- `Lobby %llx was joined`
- `Lobby join to %llx was denied.`
- `Broken channel packet from %llx detected when parsing lobby data`
- `Broken channel packet from %llx detected when parsing lobby member data`
- `Broken packet from %llx detected when parsing lobby request join reply`
- `Broken packet from %llx detected when parsing create channel message`
- `Broken packet from %llx detected when parsing connection reply`
- `Broken packet from %llx detected when parsing remote connection id`
- `Wait for lobby data timeout!`
- `Broken packet from %llx detected when parsing request connection reply`
- `Broken connectionless lobby packet from %llx detected when parsing lobby id`
- `Missing lobby on channel %u from %llx`
- `Sending connectionless connect request`
- `Failed to connectionless connect request`
- `Sending lobby discovery on port %d`
- `Failed to send lobby discovery message`
- `Added %llx as known peer %s`
- `Broken packet from %llx detected when parsing lobby host`
- `LanClient::update`
- `LanTransport::update`
- `Established a connection %u to %llx`
- `Aborting handshake with %llx. Reason: %s`

## Current best hypothesis

- Retail native code still contains the LAN host reply/data flow.
- The missing piece is not the existence of host logic; it is a reachable constructor/bootstrap path for a host-side `LanLobby` object.
- The strongest remaining possibilities are:
  1. a native-only host constructor path exists but does not stamp `0x140f44500` via an obvious immediate in the same way as `join_lan_lobby`
  2. the old host create path was removed, but enough lower-level LAN transport remains that a custom bootstrap may still be synthesizeable from the surviving helpers

Stronger negative evidence gathered later:

- no convincing receive-side handler for incoming outer `0x12` (`discover_lobby`) was found in the active LAN cluster
- the active connectionless parser `0x14056f5d0` clearly accepts:
  - `0x04`
  - `0x11`
  - `0x13`
- outer `0x12` appears to be sent, but not handled on receive in the inspected LAN cluster
- no second LAN-side receive parser handling incoming outer `0x10` or `0x12` was found in the adjacent LAN/lobby/transport cluster either
- client bootstrap sends inner packet `0x1c`, but no active receive-side handler for `0x1c` was found in the inspected LAN/lobby cluster or elsewhere in the binary
- no stock outbound sender for inner `0x17` (positive join reply) was found in the active LAN/lobby sender cluster
- no stock outbound sender/builder for outer `discover_lobby_reply (0x13)` was found anywhere in the binary, even outside the active LAN cluster
- if this interpretation is correct, the retail binary may no longer contain a complete stock discovery-reply host path, even though the rest of the LAN/join machinery survives

Additional global-scan result:

- a stock inner `0x17` join-reply builder/emitter helper still exists at `0x1406c56f0`
- however, no separate global stock sender for outer `0x13` was found

## GameSession host state

- `GameSession +0xd0` is the live `_game_session_host` peer-id field
- `GameSession +0x28` is very likely the local/self peer id
- host checks throughout the session code compare peer ids directly against `+0xd0`
- strongest current interpretation:
  - `make_game_session_host` promotes an existing peer entry to host state
  - it does not appear to create a separate standalone host object/handle

Important consumers of `_game_session_host`:

- `0x1403370e8` `remove_peer` asserts `peer_id != _game_session_host`
- `0x140337323` compares local peer vs `_game_session_host` for host-local branching
- `0x140337e51`, `0x14033b13d`, `0x14033d78b` gate privileged operations by comparing current/local peer id against `_game_session_host`

## Immediate next targets

- find another `LanLobby` initialization path that does not hardcode `state = 1`
- resolve packet id `0x18`
- determine how `discover_lobby_reply` is emitted on the host side
- identify whether host advertisement is driven by `LanClient`, `LanTransport`, or a separate native owner object
