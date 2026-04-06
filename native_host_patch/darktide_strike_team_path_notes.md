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
