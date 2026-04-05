# VT2 Player-Hosted Notes

This note captures why Vermintide 2 is relevant to the Darktide player-host investigation, and what looks reusable conceptually.

## Main takeaway

Vermintide 2 does appear to ship a real non-LAN player-hosted flow, but the reusable part is the **matchmaking/session pattern**, not a simple exposed "connect directly to peer_id" API.

The strongest Darktide direction suggested by VT2 is:

1. keep the current party/strike-team state alive
2. preserve the current peer identities and party membership
3. transition into a player-hosted session/game state
4. carry the other party members through slot reservation / join / transition

This is more promising than treating Darktide purely as a LAN-browser problem.

## VT2 evidence

### 1. Starting a player-hosted game is a matchmaking flow

Source:
- `scripts/ui/dlc_versus/views/start_game_view/windows/start_game_window_versus_custom_game.lua`

Key path:
- `_play()` at lines `225-247`

Important details:
- builds a `search_config` with:
  - `matchmaking_start_state = "MatchmakingStatePlayerHostedGame"`
  - `player_hosted = true`
  - `party_lobby_host = lobby`
- then calls `Managers.matchmaking:find_game(search_config)`

Interpretation:
- VT2 player-hosting is not a raw peer dial.
- It is an ordinary matchmaking state machine entered with a config that says “this game is player-hosted”.

### 2. The player-hosted state explicitly marks the game as player-hosted

Source:
- `scripts/managers/matchmaking/matchmaking_state_player_hosted_game.lua`

Key path:
- `on_enter()` at lines `21-29`

Important details:
- sets `self._search_config.is_player_hosted = true`
- calls `_start_hosting_game()` immediately

Then in `_start_hosting_game()` at lines `72-93`:
- sets difficulty
- sets the leader from `self._lobby:lobby_host()`
- sets matchmaking and privacy data

Interpretation:
- The host is still a normal lobby participant.
- The system promotes the current lobby into a player-hosted game state.

### 3. Search uses hosted-lobby metadata, not direct peer dialing

Source:
- `scripts/managers/matchmaking/matchmaking_state_search_player_hosted_lobby.lua`

Key paths:
- `_initialize_search()` at `27-70`
- `_search_for_game()` / `_find_suitable_lobby()` at `111-187`

Important details:
- hosted lobbies are searched through the standard lobby finder/search system
- filtering is done on lobby metadata like mission, difficulty, mechanism, EAC state, number of players, etc.

Interpretation:
- VT2’s player-hosted internet path still looks like “search/select lobby metadata”, not “use peer id as an internet route”.

### 4. Slot reservation and party carry-through are explicit

Source:
- `scripts/managers/matchmaking/matchmaking_state_reserve_slots_player_hosted.lua`

Key paths:
- `on_enter()` at `23-43`
- `_update_states()` at `61-160`
- `_join_game_success()` at `163-176`

Important details:
- joins the player-hosted lobby first
- waits for connection
- requests slot reservation from the host with `rpc_matchmaking_request_reserve_slots`
- uses the current party lobby members from `party_lobby_host:members()` at `128-131`

Interpretation:
- VT2 preserves the party and moves it into the hosted game through reservation, not ad-hoc direct connects.

### 5. Waiting state keeps the party in the hosted lobby until transition

Source:
- `scripts/managers/matchmaking/matchmaking_state_wait_join_player_hosted.lua`

Key paths:
- `on_enter()` at `18-38`
- `rpc_matchmaking_join_game()` at `101-112`

Interpretation:
- There is still a normal lobby/session transition after reservation succeeds.
- Again, this is an orchestrated host/session flow, not a raw peer-id transport shortcut.

## What VT2 does *not* show

From the source review, there does **not** appear to be a clean public API equivalent to:

- `connect_to_peer_id(peer_id)`
- `join_player_host_by_peer_id(peer_id)`

`peer_id` is used everywhere, but mostly:
- after a session already exists
- for RPC routing
- for ownership, slots, reservations, or chat/session state

So `peer_id` in VT2 still looks like a **session identity**, not a direct internet rendezvous primitive.

## Darktide parallels

Darktide already has the session-join side of a very similar pattern.

### 1. Existing party/session boot paths

Source:
- `scripts/managers/multiplayer/multiplayer_session_manager.lua`

Relevant functions:
- `party_immaterium_hot_join_hub_server()` at `146-153`
- `party_immaterium_join_server(matched_game_session_id, party_id)` at `156-163`

Interpretation:
- Darktide already has explicit session boot objects for carrying an existing party into hub/mission sessions.

### 2. Darktide mission boot already uses WAN browser -> lobby -> connection client

Source:
- `scripts/multiplayer/party_immaterium_mission_session_boot.lua`

Relevant flow:
- construct WAN browser at `34-36`
- start handshake at `126-127`
- `browser:establish_connection_to_server(...)` at `183`
- use `browser:lobby(1)` and `Network.join_lan_lobby(lobby_data.id)` at `202-206`
- create `ConnectionClient` at `145`

Interpretation:
- Darktide already has a non-LAN/browser-driven mission join flow.
- The current research question is whether a player-hosted front door can be grafted into this existing pattern.

### 3. Darktide hub boot uses the same general shape

Source:
- `scripts/multiplayer/party_immaterium_hub_session_boot.lua`

Relevant flow:
- construct WAN browser at `24-26`
- handshake at `127-128`
- connect at `199-200`
- `Network.join_lan_lobby(lobby_data.id)` at `218-222`
- create `ConnectionClient` at `146`

Interpretation:
- Darktide already treats browser/lobby/join/client creation as a normal session-boot pipeline.

## What looks reusable for Darktide

### Reusable concept: existing strike team as carrier

The strongest reusable idea from VT2 is:

- keep the existing party/strike team alive
- preserve its peer identities and membership
- move from “party in hub/queue” to “player-hosted mission/session” through a structured transition

This is much more plausible than inventing a pure peer-id direct-connect path from scratch.

### Reusable concept: reservation/transition after join

VT2 suggests the hosted path is not just:
- discover host
- connect

It is also:
- reserve slots for the party
- only then transition into the game/session state

That implies Darktide may need a similar reservation/bootstrap layer if we want a stable strike-team-hosted path.

## Non-LAN conclusion

If Darktide already has the party and its peers in the same strike team, then the best non-LAN path is probably:

1. reuse existing strike-team/session state
2. reuse existing peer/channel/session identities
3. promote one player into host authority
4. carry the others through the same general boot/transition process

Not:

1. use raw `peer_id` as a standalone internet address
2. bypass the existing party/session mechanisms entirely

## Practical research direction from this note

The highest-value next investigation is:

- how to repurpose Darktide’s **existing party/session boot** to lead into a player-hosted mission session

Specifically:
- inspect current strike-team peer/channel state
- inspect where `party_id` and `matched_game_session_id` are consumed
- inspect whether a local player-hosted target can stand in for the usual dedicated mission target

This should proceed in parallel with the existing LAN/browser research, not replace it.
