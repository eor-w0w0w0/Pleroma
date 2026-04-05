# SoloPlay Native Host Patch

Initial C++ scaffold for a native/player-host patch.

Current goal:
- expose or recreate the missing player-host bootstrap layer that retail Darktide no longer exposes to Lua
- keep rewards/progression out of scope

Confirmed from source/runtime:
- retail Lua has client join support for LAN-style lobbies
- retail Lua does not expose host-side lobby creation
- retail message definitions already exist for the player-host bootstrap RPCs
- session-layer host logic already exists in Lua via `SessionHost` and `SessionRemoteStateMachine`

Most likely native responsibilities:
1. Create and advertise a real host LAN lobby userdata.
2. Expose or bridge the missing host-side connection bootstrap used by `ConnectionHost`.
3. Hand Lua a real `engine_lobby` so existing session/game sync can take over.

Known runtime surfaces:
- `Network.join_lan_lobby`
- `Network.leave_lan_lobby`
- `LanClient.create_lobby_browser`
- joined `engine_lobby` methods include `set_max_members`, `set_game_session_host`, `open_channel`, `close_channel`, `kick`, `members`, `set_member_data`

Known missing Lua pieces:
- `ConnectionHost`
- `PlayerHostedSessionBoot`
- host-side LAN lobby creation/bootstrap

Build:
```sh
cmake -S . -B build
cmake --build build
```

Expected output:
- `build/SoloPlayNativeHostPatch.dll`
- `build/XINPUT9_1_0.dll`
- `build/dxva2.dll`

Planned injection path:
- Darktide imports `XINPUT9_1_0.dll` and only requests `XInputGetState` + `XInputSetState`
- the game does not ship a local `XINPUT9_1_0.dll`
- this makes an xinput proxy DLL a practical no-injector native entry point
- proxy strategy:
  1. place `XINPUT9_1_0.dll` next to `Darktide.exe`
  2. proxy loads the real system xinput DLL from System32
  3. proxy forwards the xinput exports and runs our patch initialization in-process
  4. proxy attempts to load `SoloPlayNativeHostPatch.dll` from the same directory

Current diagnostics:
- the xinput proxy writes `SoloPlayNativeHostPatch.log` next to itself
- expected early log lines include:
  - `xinput proxy initialized`
  - `system xinput exports resolved`
  - `companion patch loaded`

Alternate proxy path:
- Darktide also imports `dxva2.dll`
- the main EXE only imports two dxva2 functions:
  - `GetNumberOfPhysicalMonitorsFromHMONITOR`
  - `GetPhysicalMonitorsFromHMONITOR`
- this makes a `dxva2.dll` proxy a cleaner fallback native entry point if `XINPUT9_1_0.dll` override destabilizes startup
- current runtime-validation behavior:
  - the `dxva2` proxy now schedules a delayed companion load automatically
  - current delay is `20s`
  - once loaded, the companion patch starts a validation worker that waits for `LanClient`, resolves the deep transport interface, and attempts a harmless outer callback registration
  - packet metadata from that callback is written to `SoloPlayNativeHostPatch.log`
  - opt-in marker files in `binaries/`:
    - `SoloPlayNativeHostPatch.enable_browser_callback`
    - `SoloPlayNativeHostPatch.enable_discovery_reply`
  - both are off by default

Reverse-engineering artifacts:
- `reverse_engineering.md` tracks recovered LAN/lobby addresses, packet ids, and open questions
- `src/darktide_lan_symbols.h` contains code-side RVAs and field offsets for the recovered LAN subsystem pieces
- `src/darktide_transport_api.h` contains the inferred transport virtual offsets and callback RVAs/signatures
- `src/darktide_packet_api.h` contains inferred bitstream helper RVAs and send-path conventions
- `src/darktide_browser_api.h` contains the inferred `LanLobbyBrowser` and result-entry layouts
- `src/darktide_gamesession_api.h` contains the inferred `GameSession` host/peer field layout
- `src/darktide_wan_client_api.h` contains the inferred script-facing `LanClient` wrapper and underlying `LanTransport` backend references
- `src/darktide_connectionless_api.h` contains inferred connectionless envelope constants, endpoint layout, and token encoding helpers
- `implementation_strategy.md` summarizes the current synthetic-host direction implied by the reversing work

Immediate reversing targets:
1. hidden host-side lobby creation function
2. hidden server-side handshake/listen bootstrap
3. native bridge or reimplementation of the `ConnectionHost` interface expected by `ConnectionManager`

Runtime facts already confirmed:
- retail message definitions for the player-host bootstrap RPCs already exist
- client join flow already exists in Lua
- session host flow already exists in Lua
- strongest missing piece is host-side lobby/bootstrap creation, not mission/session sync itself
