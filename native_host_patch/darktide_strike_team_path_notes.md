# Darktide Strike-Team / Session Path Notes

This note tracks the strongest source-level evidence for reusing Darktide’s existing
strike-team / party / session machinery instead of relying purely on the LAN browser path.

## Main takeaway

Darktide already has a real backend-driven flow that carries an existing party into:

- a hub server session
- a mission server session

That flow already preserves:

- current `party_id`
- current `game_session_id`
- current peer/channel/session identities

So the best non-LAN alternative to pure LAN discovery is likely to reuse this path and replace only the final “target dedicated session” with a player-hosted target.

## Core source path

### 1. Party manager tracks the active game session id

Source:
- `scripts/managers/party_immaterium/party_immaterium_manager.lua`

Key functions:

- `game_session_in_progress()` at `1175-1179`
  - true when party game-state status is `GAME_SESSION_IN_PROGRESS`

- `current_game_session_id()` at `1193-1200`
  - returns `game_state.game_session.game_session_id`

- `join_game_session()` at `1185-1191`
  - calls `Managers.multiplayer_session:party_immaterium_join_server(game_session_id, party_id)`

Interpretation:
- the party manager already knows when the current strike team is supposed to be in a session
- the actual mission join path is already keyed by the current `game_session_id` and `party_id`

### 2. The actual mission boot object is explicit

Source:
- `scripts/managers/multiplayer/multiplayer_session_manager.lua`

Key function:

- `party_immaterium_join_server(matched_game_session_id, party_id)` at `156-163`
  - creates `MultiplayerSession:new()`
  - sets `_session_boot = PartyImmateriumMissionSessionBoot:new(new_session, matched_game_session_id, party_id)`

Interpretation:
- there is already a dedicated “boot this party into the mission session” object
- if we ever want to repurpose the strike-team path, this is one of the highest-value interception points

### 2.5. `matched_game_session_id` and `initial_party_id` matter after join

Relevant sources:

- `scripts/multiplayer/connection/connection_client.lua`
- `scripts/managers/mechanism/mechanisms/mechanism_adventure.lua`
- `scripts/managers/mechanism/mechanisms/mechanism_expedition.lua`

Observed behavior:

- `ConnectionClient` only stores/exposes:
  - `_matched_game_session_id`
  - `_initial_party_id`
- but the surrounding mission mechanisms read them from `Managers.connection` at mechanism init and use them as the baseline for later party/session decisions

Practical effect:

- when the live `party_id` changes, the mechanism compares:
  - current `party_id`
  - cached initial `party_id`
  - current `game_session_id`
  - cached `matched_game_session_id`
- and then decides whether to:
  - stay in current mission
  - join another party game session
  - leave back to hub

Interpretation:

- these fields are passive inside `ConnectionClient`
- but they are definitely not “just debug metadata” in the wider session flow
- they are part of the mechanism-level follow-party contract

### 3. Mission session boot already uses browser -> lobby -> connection client

Source:
- `scripts/multiplayer/party_immaterium_mission_session_boot.lua`

Key flow:

- constructs WAN browser at `34-36`
- fetches server details using backend `matched_game_session_id` at `39-65`
- starts browser handshake at `126-127`
- after handshake success, calls `browser:establish_connection_to_server(...)` at `183`
- once lobby search succeeds, joins lobby with `Network.join_lan_lobby(lobby_data.id)` at `202-206`
- creates `ConnectionClient` at `145`

Interpretation:
- Darktide already has a complete join pipeline built around:
  - browser
  - handshake
  - lobby
  - connection client
- the question is whether a player-hosted target can be injected into this same pattern

### 4. Hub session boot is the same pattern

Source:
- `scripts/multiplayer/party_immaterium_hub_session_boot.lua`

Relevant flow:
- constructs WAN browser at `24-26`
- handshake at `127-128`
- `browser:establish_connection_to_server(...)` at `199-200`
- `Network.join_lan_lobby(lobby_data.id)` at `218-222`
- creates `ConnectionClient` at `146`

Interpretation:
- the browser/lobby/client model is not mission-only
- this increases confidence that a player-hosted target can be treated as “just another server target” if we can supply the right handshake/lobby/session state

## Mechanism-level auto-join logic

### Hub

Source:
- `scripts/managers/mechanism/mechanisms/mechanism_hub.lua`

Important lines:
- `269-283`
- `_retry_join()` at `321-326`

Behavior:
- if `party_immaterium:game_session_in_progress()` is true
- and the current game session id changes
- mechanism hub calls `Managers.party_immaterium:join_game_session()` automatically

Interpretation:
- the hub already trusts the party manager as the source of truth for “my party has a session I should join now”

### Adventure / Mission

Source:
- `scripts/managers/mechanism/mechanisms/mechanism_adventure.lua`

Important lines:
- `316-341`
- retry path at `469-489`

Behavior:
- if the player’s party changes and the new party is in another `game_session_id`
- the mechanism automatically calls `Managers.party_immaterium:join_game_session()`
- otherwise it leaves back to hub or stays in current session

Interpretation:
- Darktide already has robust “follow my party into its current session” behavior

## Party connection as state carrier

Source:
- `scripts/managers/party_immaterium/party_immaterium_connection.lua`

Key pieces:
- `_game_state` / `_game_state_version` at `26-27`
- `party_game_state()` at `96-98`
- `_handle_party_game_state_update_event()` at `240-249`

Interpretation:
- party session intent is carried over the party gRPC stream as structured state updates
- the local game doesn’t need to infer session intent; it is told what the current game session is

## Most promising reuse points

### Best source-level interception points

1. `PartyImmateriumManager.join_game_session()`
   - current entrypoint from party state into mission session boot

2. `MultiplayerSessionManager.party_immaterium_join_server(...)`
   - the explicit place where the game chooses `PartyImmateriumMissionSessionBoot`

3. `PartyImmateriumMissionSessionBoot._fetch_server_details()`
   - current place where dedicated server details are fetched from backend
   - a player-hosted path could replace or augment this step

4. `PartyImmateriumMissionSessionBoot._start_handshaking()`
   - current browser/handshake setup path

### Strongest non-LAN hypothesis

If the strike team is already together, then the player-hosted path may not need browser discovery first.

Instead, a plausible flow is:

1. party already exists
2. party state says there is a target session to join
3. mission boot object is given player-hosted target details instead of dedicated-server details
4. existing connection/lobby/session host machinery does the rest

## Open question this note motivates

The highest-value next research question is:

- what is the smallest replacement for “backend server details” that `PartyImmateriumMissionSessionBoot` needs in order to target a player-hosted session instead of a dedicated one?

That is likely the bridge between the strike-team path and the LAN/browser host bootstrap work.

## Strongest current strike-team bridge hypothesis

After deeper source review, the most plausible player-hosted strike-team bridge is:

1. keep using the existing party game-state as the source of truth
2. keep using `PartyImmateriumManager.join_game_session()` as the high-level entrypoint
3. intercept or replace the dedicated-server-details phase inside `PartyImmateriumMissionSessionBoot`
4. provide a player-hosted target description instead of backend dedicated-server details
5. let the existing browser/lobby/client/session boot continue from there as far as possible

Why this is promising:

- the party manager already distributes `game_session_id`
- the mission mechanisms already auto-follow the party into that session
- `ConnectionClient` already carries `matched_game_session_id` and `initial_party_id` forward for mechanism-level follow-party logic
- the session/join protocol (`rpc_request_join_game_session`, slot reservation, claim slot, sync) already exists after the connection is made

So the best strike-team path currently looks like a **session-boot redirection problem**, not a full session-stack reinvention problem.

## Experimental host compatibility patch now added

The first minimal post-connect compatibility set has now been wired into
`mods/SoloPlay/scripts/mods/SoloPlay/experimental/connection_host.lua`.

Added on `rpc_ready_to_receive_local_profiles(...)` / `rpc_client_entered_connected_state(...)`:

1. `ProfileSynchronizerHost.register_rpcs(channel_id)`
2. `ProfileSynchronizerHost.peer_connected(peer_id, channel_id)`
3. initial host profile sync to the joining peer via `sync_player_profile(...)`
4. `package_synchronization:synchronizer_host():add_peer(peer_id)`
5. `rpc_sync_host_local_players(...)` to the joining peer
6. `rpc_player_connected(...)` fanout between the joining peer and already-connected peers
7. `rpc_peer_joined_session(...)` fanout between the joining peer and already-connected peers

Added on disconnect / removal side:

1. queued `"disconnected"` events for host-initiated kick/disconnect paths
2. `package_synchronization:synchronizer_host():remove_peer(peer_id)`
3. `ProfileSynchronizerHost.peer_disconnected(peer_id, channel_id)`
4. `rpc_player_disconnected(...)` fanout to remaining peers
5. `rpc_peer_left_session(...)` fanout to remaining peers

Additional host-loop compatibility fix:

6. `ExperimentalConnectionHost.update(dt)` now ticks `ProfileSynchronizerHost:update(dt)` instead of being a pure no-op

Rationale:

- stock `ProfileSynchronizerHost` relies on its per-frame `update()` to flush RPC queues and finalize profile sync state
- without ticking it, the earlier profile-sync compatibility additions would not progress in practice

Intent:

- satisfy the smallest host-side expectations the stock `ConnectionClient`,
  `ConnectionManager`, `ProfileSynchronizerClient`, and `MultiplayerSession.client_joined`
  paths appear to have after a peer reaches connected state

This is still not a full stock-quality host implementation, but it removes several
obvious missing integration steps from the experimental strike-team host path.

## Reserve / claim replies are already good enough

Client-side source review confirms:

- `rpc_reserve_slots_reply(channel_id, success)` only gates a boolean transition
- `rpc_claim_slot_reply(channel_id, success)` only gates a boolean transition
- neither state validates slot counts, slot ids, or reservation tokens

So the current experimental host replies of:

- `rpc_reserve_slots_reply(channel_id, true)`
- `rpc_claim_slot_reply(channel_id, true)`

are already sufficient for the immediate client-side contract.

Practical implication:

- reserve/claim is not the high-value area to refine right now
- the bigger value is still in:
  - `rpc_sync_local_players_reply` quality
  - profile sync content
  - package sync completion

## Boot-time host promotion now mirrored

`experimental/player_hosted_session_boot.lua` now mirrors the two clearest stock host-promotion calls from `SessionHost.init(...)`:

1. `GameSession.make_game_session_host(engine_gamesession)` when a game session object is present
2. `engine_lobby:set_game_session_host(Network.peer_id())`

Rationale:

- this is the lowest-risk stock host promotion signal in the codebase
- it should make the experimental player-hosted boot look more like a real host session to later loading/mechanism/session code

## Minimal host event contract confirmed

From `ConnectionManager._update_host(...)`, the experimental host only needs to emit:

- `"connecting"` with:
  - `channel_id`
  - `peer_id`

- `"connected"` with:
  - `channel_id`
  - `peer_id`
  - `player_sync_data`

- `"disconnected"` with:
  - `channel_id`
  - `peer_id`
  - optional `game_reason`
  - optional `engine_reason`

That is enough for `ConnectionManager` to drive:

- `_add_client(...)`
- `MultiplayerSession.client_joined(...)`
- `_remove_client(...)`
- `MultiplayerSession.client_disconnected(...)`

So the experimental host path does not need a new top-level manager contract; it needs more complete stock-compatible data and RPC fanout behind those existing event names.

## Loading/package sync gate confirmed

Source review of `LoadingHost`, `PackageSynchronizerHost`, and `ProfileSynchronizerHost` confirms:

- `MultiplayerSession.client_joined(...)` is not enough by itself
- the remote peer only progresses once `LoadingHost` sees:
  - level load complete for the spawn group
  - `profile_sync_host:profiles_synced(...) == true`
  - `package_sync_host:peers_synced(...) == true`

So the current experimental compatibility patch is pointed at the correct gate:

- `ProfileSynchronizerHost.peer_connected(...)`
- host profile sync via `sync_player_profile(...)`
- `rpc_sync_host_local_players(...)`
- `PackageSynchronizerHost.add_peer(peer_id)`

The remaining uncertainty is mainly the *quality* of the temporary sync data we provide
(slots/profile chunks/player data), not which managers or RPC families are required.

## Loading-host gate is now mapped

The stock host loading path for a remote peer is now clear:

1. `Managers.loading:add_client(channel_id)` creates a `LoadingRemoteStateMachine`
2. client requests a spawn group
3. host places the peer into `SpawnQueue`
4. host spawn group enters `wait_for_level`
5. both host and remote must report `rpc_finished_loading_level`
6. host moves the group to `wait_for_sync`
7. host enables package sync for that peer
8. host waits for:
   - `profiles_synced(...)`
   - `peers_synced(...)`
9. for the initial group, host waits until `Managers.loading:end_load()` releases that spawn group
10. remote loading state then adds the peer to game session and sends `rpc_group_loaded`

Practical implication:

- even with a good experimental host connection/session path, the peer will still stall unless the hosted path really reaches stock `LoadingHost` remote-state behavior
- this is one of the next likely runtime bottlenecks after connection/session sync becomes good enough

## Important loading-host creation result

Stock source confirms `LoadingHost` creation is already on the exact `ConnectionHost` branch in:

- `scripts/managers/multiplayer/multiplayer_session_manager.lua:333-345`

Specifically:

- if `session_boot:state() == "ready"`
- and `connection_class_name == "ConnectionHost"`

then `MultiplayerSessionManager.update()` itself creates:

- `LoadingHost:new(...)`
- `Managers.loading:set_host(loading_host)`

Implication:

- the experimental strike-team host path should not need a manual `Managers.loading:set_host(...)` patch
- the more important runtime question is whether the new hosted path actually reaches that stock `ready -> ConnectionHost` branch cleanly

## Important narrowing result

Source review now strongly suggests the experimental host’s `"connected"` event path is already sufficient to create the stock `LoadingRemoteStateMachine` through:

- `ConnectionManager._update_host(...)`
- `_add_client(...)`
- `MultiplayerSession.client_joined(...)`
- `Managers.loading:add_client(channel_id)`

So the next likely blockers are later in the pipeline, not at the event-contract boundary:

- real lobby/channel scaffolding
- promoted host/session-host state
- loading/profile/package sync completion

## Current host-side contract focus

Source/binary review now suggests the strike-team hosted path is already aiming at the correct layer:

- early host promotion via:
  - `GameSession.make_game_session_host(...)`
  - `engine_lobby:set_game_session_host(Network.peer_id())`

- then the full connection-channel handshake through:
  - version / boot / host type / mechanism / local player sync / slot reserve / slot claim / EAC / tick rate / profile sync / data sync / stats / connected check
  - culminating in `rpc_client_entered_connected_state`

Interpretation:

- the biggest remaining work on the strike-team path is not high-level lobby state
- it is making the experimental connection-channel handshake stock-compatible enough that the peer reaches `rpc_client_entered_connected_state` cleanly and the host can then hand off to loading/session stock paths

## Sync-data quality improvements now applied

In `experimental/connection_host.lua`, the worst placeholder fields in
`rpc_sync_local_players(...)` have now been improved:

Now more stock-like:

- `slot_array`
  - uses `Managers.player:claim_slot()` when available instead of naive `1..n`

- `player_instance_id_array`
  - uses stable per-peer/per-local-player string ids instead of a fresh `Application.guid()` every time

Still provisional:

- `profile_chunks_array`
  - now uses a minimal stock-like profile stub when no better source is available:
    - valid player archetype name
    - empty `loadout_item_ids`
    - empty `talents`
  - this should round-trip through stock `pack_profile` / `unpack_profile` much better than raw `{}`

Current code-side direction:

- `ExperimentalConnectionHost` now routes remote profile chunk generation through a dedicated helper (`best_remote_profile_chunks(...)`)
- `best_remote_profile_chunks(...)` now prefers cached current strike-team member profiles first:
  - `Managers.party_immaterium:all_members()`
  - `member:account_id()` match
  - `member:profile()`
- then falls back to cached presence/profile data from `Managers.presence:get_presence(account_id):character_profile()` when available
- fallback remains a minimal stock-like stub when no cached profile is available
- intended next improvement is to substitute a cached real profile source there without rewriting the host event/session contract again
- runtime observability now logs which path was used:
  - `party_member`
  - `presence`
  - `stub`

## Package-sync implication of better remote profiles

Source review now suggests:

- package sync resolves from the reconstructed runtime profile created from `profile_chunks_array`
- especially from `profile.visual_loadout` and related archetype/talent-derived fields

So better remote profile chunks should improve package sync automatically.

But this does **not** replace the need for stock package-sync seeding:

- `package_synchronizer_host:add_peer(peer_id)`
- later `LoadingHost` -> `package_sync_host:enable_peers(...)`

Therefore the right priority remains:

1. keep stock package-sync seeding/enablement in place
2. improve remote profile realism
3. avoid package-specific ad-hoc hacks unless profile-based improvement proves insufficient

Pass-through from the joining client and already close to stock:

- `account_id_array`
- `character_id_array`
- `player_session_id_array`
- `last_mission_id`
