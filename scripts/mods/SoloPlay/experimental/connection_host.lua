local MasterItems = require("scripts/backend/master_items")
local MatchmakingConstants = require("scripts/settings/network/matchmaking_constants")
local ProfileSynchronizerHost = require("scripts/loading/profile_synchronizer_host")

local HOST_TYPES = MatchmakingConstants.HOST_TYPES
local HOST_RPCS = {
    "rpc_check_version",
    "rpc_connection_booted",
    "rpc_request_master_items_version",
    "rpc_request_host_type",
    "rpc_check_mechanism",
    "rpc_sync_local_players",
    "rpc_reserve_slots",
    "rpc_claim_slot",
    "rpc_request_eac_approval",
    "rpc_ready_to_receive_tick_rate",
    "rpc_ready_to_receive_local_profiles",
    "rpc_dlc_verification_client_done",
    "rpc_ready_to_receive_data",
    "rpc_sync_stat_version",
    "rpc_check_connected",
    "rpc_client_entered_connected_state",
}

local ExperimentalConnectionHost = class("ExperimentalConnectionHost")

local function shallow_copy(tbl)
    local out = {}

    for key, value in pairs(tbl or {}) do
        out[key] = value
    end

    return out
end

ExperimentalConnectionHost.init = function (self, event_delegate, approve_channel_delegate, engine_lobby, host_type, tick_rate, max_members, optional_session_id)
    self._event_delegate = event_delegate
    self._approve_channel_delegate = approve_channel_delegate
    self._engine_lobby = engine_lobby
    self._host_type = host_type or HOST_TYPES.player
    self._tick_rate = tick_rate or 30
    self._max_members = max_members or 4
    self._session_id = optional_session_id
    self._session_seed = math.random_seed()
    self._peer_id = Network.peer_id()
    self._profile_synchronizer_host = ProfileSynchronizerHost:new(event_delegate)
    self._events = {}
    self._connecting = {}
    self._members = {}
    self._pending_sync = {}
    self._ticket_parts = {}

    if engine_lobby and engine_lobby.set_max_members then
        engine_lobby:set_max_members(self._max_members)
    end

    approve_channel_delegate:register(engine_lobby:id(), "connection", self)
end

ExperimentalConnectionHost.delete = function (self)
    self._approve_channel_delegate:unregister(self._engine_lobby:id(), "connection")

    for channel_id, _ in pairs(self._connecting) do
        self._event_delegate:unregister_channel_events(channel_id, unpack(HOST_RPCS))
    end

    for channel_id, _ in pairs(self._members) do
        self._event_delegate:unregister_channel_events(channel_id, unpack(HOST_RPCS))
    end

    self._profile_synchronizer_host:delete()
end

ExperimentalConnectionHost.register_profile_synchronizer = function (self)
    Managers.profile_synchronization:set_profile_synchroniser_host(self._profile_synchronizer_host)
end

ExperimentalConnectionHost.unregister_profile_synchronizer = function (self)
    if Managers.profile_synchronization then
        Managers.profile_synchronization:set_profile_synchroniser_host(nil)
    end
end

ExperimentalConnectionHost.approve_channel = function (self, channel_id, peer_id, lobby_id, message)
    if message ~= "connection" then
        return false
    end

    self._connecting[channel_id] = {
        peer_id = peer_id,
    }
    self._pending_sync[channel_id] = {
        local_player_id_array = {},
        is_human_controlled_array = {},
        account_id_array = {},
        player_session_id_array = {},
        slot_array = {},
        player_instance_id_array = {},
        profile_chunks_array = {},
    }

    self._event_delegate:register_connection_channel_events(self, channel_id, unpack(HOST_RPCS))

    self._events[#self._events + 1] = {
        name = "connecting",
        parameters = {
            channel_id = channel_id,
            peer_id = peer_id,
        },
    }

    return true
end

ExperimentalConnectionHost.allocation_state = function (self)
    return table.size(self._members) + 1, self._max_members
end

ExperimentalConnectionHost.num_connections = function (self)
    return table.size(self._members) + table.size(self._connecting)
end

ExperimentalConnectionHost.max_members = function (self)
    return self._max_members
end

ExperimentalConnectionHost.tick_rate = function (self)
    return self._tick_rate
end

ExperimentalConnectionHost.host_type = function (self)
    return self._host_type
end

ExperimentalConnectionHost.host_is_dedicated_server = function (self)
    return false
end

ExperimentalConnectionHost.server_name = function (self)
    if self._engine_lobby.server_name then
        return self._engine_lobby:server_name()
    end

    return self._engine_lobby:data("server_name") or "Player Hosted"
end

ExperimentalConnectionHost.engine_lobby = function (self)
    return self._engine_lobby
end

ExperimentalConnectionHost.engine_lobby_id = function (self)
    return self._engine_lobby:id()
end

ExperimentalConnectionHost.session_id = function (self)
    return self._session_id
end

ExperimentalConnectionHost.session_seed = function (self)
    return self._session_seed
end

ExperimentalConnectionHost.connecting_peers = function (self)
    local peers = {}

    for _, data in pairs(self._connecting) do
        peers[#peers + 1] = data.peer_id
    end

    return peers
end

ExperimentalConnectionHost.has_connecting_peers = function (self)
    return not table.is_empty(self._connecting)
end

ExperimentalConnectionHost.host = function (self)
    return self._peer_id
end

ExperimentalConnectionHost.update = function (self, dt)
    return
end

ExperimentalConnectionHost.next_event = function (self)
    if table.is_empty(self._events) then
        return nil
    end

    local event = self._events[1]

    table.remove(self._events, 1)

    return event.name, event.parameters
end

ExperimentalConnectionHost.kick = function (self, channel_id, reason, option_details)
    RPC.rpc_kicked(channel_id, reason, option_details)
    self._engine_lobby:close_channel(channel_id)
end

ExperimentalConnectionHost.remove = function (self, channel_id)
    self._event_delegate:unregister_channel_events(channel_id, unpack(HOST_RPCS))
    self._connecting[channel_id] = nil
    self._members[channel_id] = nil
    self._pending_sync[channel_id] = nil
    self._ticket_parts[channel_id] = nil
end

ExperimentalConnectionHost.disconnect = function (self, channel_id)
    self._engine_lobby:close_channel(channel_id)
end

ExperimentalConnectionHost.rpc_check_version = function (self, channel_id, network_hash)
    RPC.rpc_check_version_reply(channel_id, network_hash == Managers.connection.combined_hash)
end

ExperimentalConnectionHost.rpc_connection_booted = function (self, channel_id)
    RPC.rpc_connection_booted_reply(channel_id)
end

ExperimentalConnectionHost.rpc_request_master_items_version = function (self, channel_id)
    local metadata = MasterItems.get_cached_metadata() or {}

    RPC.rpc_master_items_version_reply(channel_id, tostring(MasterItems.get_cached_version()), metadata.url)
end

ExperimentalConnectionHost.rpc_request_host_type = function (self, channel_id)
    RPC.rpc_request_host_type_reply(channel_id, false, self._max_members)
end

ExperimentalConnectionHost.rpc_check_mechanism = function (self, channel_id, ticket_part, is_last_part)
    local parts = self._ticket_parts[channel_id] or {}

    parts[#parts + 1] = ticket_part
    self._ticket_parts[channel_id] = parts

    if is_last_part then
        -- TODO: decode the JWT ticket and validate mechanism / mission / host type.
        RPC.rpc_check_mechanism_reply(channel_id, true, nil)
    end
end

ExperimentalConnectionHost.rpc_sync_local_players = function (self, channel_id, local_player_id_array, is_human_controlled_array, account_id_array, character_id_array, player_session_id_array, last_mission_id)
    local pending_sync = self._pending_sync[channel_id]

    pending_sync.local_player_id_array = shallow_copy(local_player_id_array)
    pending_sync.is_human_controlled_array = shallow_copy(is_human_controlled_array)
    pending_sync.account_id_array = shallow_copy(account_id_array)
    pending_sync.character_id_array = shallow_copy(character_id_array)
    pending_sync.player_session_id_array = shallow_copy(player_session_id_array)
    pending_sync.last_mission_id = last_mission_id

    -- TODO: assign real slots and profile chunks for remote players before using this live.
    for i = 1, #local_player_id_array do
        pending_sync.slot_array[i] = i
        pending_sync.player_instance_id_array[i] = Application.guid()
        pending_sync.profile_chunks_array[i] = pending_sync.profile_chunks_array[i] or { "" }
    end

    RPC.rpc_sync_local_players_reply(channel_id, pending_sync.local_player_id_array, pending_sync.slot_array, pending_sync.player_instance_id_array)
end

ExperimentalConnectionHost.rpc_reserve_slots = function (self, channel_id, slots_to_reserve)
    RPC.rpc_reserve_slots_reply(channel_id, true)
end

ExperimentalConnectionHost.rpc_claim_slot = function (self, channel_id)
    RPC.rpc_claim_slot_reply(channel_id, true)
end

ExperimentalConnectionHost.rpc_request_eac_approval = function (self, channel_id)
    RPC.rpc_request_eac_approval_reply(channel_id, true)
end

ExperimentalConnectionHost.rpc_ready_to_receive_tick_rate = function (self, channel_id)
    RPC.rpc_sync_tick_rate(channel_id, self._tick_rate)
end

ExperimentalConnectionHost.rpc_ready_to_receive_local_profiles = function (self, channel_id)
    -- TODO: integrate ProfileSynchronizerHost initial sync for remote player profiles.
    RPC.rpc_sync_local_profiles_reply(channel_id)
end

ExperimentalConnectionHost.rpc_dlc_verification_client_done = function (self, channel_id, platform)
    RPC.rpc_dlc_verification_host_response(channel_id, nil)
end

ExperimentalConnectionHost.rpc_ready_to_receive_data = function (self, channel_id)
    RPC.rpc_session_seed_sync(channel_id, self._session_seed)
    RPC.rpc_data_sync_done(channel_id)
end

ExperimentalConnectionHost.rpc_sync_stat_version = function (self, channel_id, local_player_id, version)
    RPC.rpc_stat_version_matched(channel_id)
end

ExperimentalConnectionHost.rpc_check_connected = function (self, channel_id)
    RPC.rpc_check_connected_reply(channel_id)
end

ExperimentalConnectionHost.rpc_client_entered_connected_state = function (self, channel_id)
    local connecting = self._connecting[channel_id]
    local pending_sync = self._pending_sync[channel_id]

    if not connecting or not pending_sync then
        return
    end

    self._connecting[channel_id] = nil
    self._members[channel_id] = connecting

    self._events[#self._events + 1] = {
        name = "connected",
        parameters = {
            channel_id = channel_id,
            peer_id = connecting.peer_id,
            player_sync_data = pending_sync,
        },
    }

    -- TODO: once a native lobby/bootstrap exists, integrate these with stock sync managers:
    --   Managers.package_synchronization:synchronizer_host():add_peer(connecting.peer_id)
    --   self._profile_synchronizer_host:peer_connected(connecting.peer_id, channel_id)
    --   RPC.rpc_sync_host_local_players(...)
    --   RPC.rpc_player_connected(...) to already connected peers
end

return ExperimentalConnectionHost
