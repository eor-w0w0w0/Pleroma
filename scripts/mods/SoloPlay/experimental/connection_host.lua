local MasterItems = require("scripts/backend/master_items")
local MatchmakingConstants = require("scripts/settings/network/matchmaking_constants")
local PlayerManager = require("scripts/foundation/managers/player/player_manager")
local ProfileSynchronizerHost = require("scripts/loading/profile_synchronizer_host")
local ProfileUtils = require("scripts/utilities/profile_utils")
local VALID_REMOTE_ARCHETYPE = {
	veteran = true,
	zealot = true,
	psyker = true,
	ogryn = true,
}

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

local function collect_local_player_sync_data(include_profile_chunks)
	local player_manager = Managers.player

	if not player_manager or not player_manager.create_sync_data then
		return {
			local_player_id_array = {},
			is_human_controlled_array = {},
			account_id_array = {},
			character_id_array = {},
			profile_chunks_array = include_profile_chunks and {} or nil,
			player_session_id_array = {},
			slot_array = {},
			player_instance_id_array = {},
			last_mission_id = nil,
		}
	end

	return player_manager:create_sync_data(Network.peer_id(), include_profile_chunks)
end

local function stable_remote_instance_id(peer_id, local_player_id)
	return string.format("remote-%s-%s", tostring(peer_id), tostring(local_player_id))
end

local function split_profile_into_chunks(profile)
	local ok_pack, profile_json = pcall(ProfileUtils.pack_profile, profile)
	if not ok_pack or not profile_json then
		return nil
	end

	local chunks = {}
	ProfileUtils.split_for_network(profile_json, chunks)

	return chunks
end

local function log_remote_profile_source(source_name, account_id)
	Log.info("ExperimentalConnectionHost", "remote profile source=%s account_id=%s", tostring(source_name), tostring(account_id))
end

local function cached_remote_profile(account_id)
	local party_manager = Managers.party_immaterium
	if party_manager and party_manager.all_members then
		local ok_members, members = pcall(party_manager.all_members, party_manager)
		if ok_members and type(members) == "table" then
			for i = 1, #members do
				local member = members[i]
				if member and member.account_id and member.profile then
					local ok_account, member_account_id = pcall(member.account_id, member)
					if ok_account and member_account_id == account_id then
					local ok_profile, profile = pcall(member.profile, member)
					if ok_profile and profile and profile.archetype and profile.loadout_item_ids and profile.talents then
						log_remote_profile_source("party_member", account_id)
						return profile
					end
					end
				end
			end
		end
	end

	local presence_manager = Managers.presence
	if not presence_manager or not presence_manager.get_presence then
		return nil
	end

	local presence = presence_manager:get_presence(account_id)
	if not presence or not presence.character_profile then
		return nil
	end

	local ok, profile = pcall(presence.character_profile, presence)
	if ok and profile and profile.archetype and profile.loadout_item_ids and profile.talents then
		log_remote_profile_source("presence", account_id)
		return profile
	end

	return nil
end

local function send_host_local_players(channel_id)
	local sync_data = collect_local_player_sync_data(false)

	RPC.rpc_sync_host_local_players(
		channel_id,
		sync_data.local_player_id_array,
		sync_data.is_human_controlled_array,
		sync_data.account_id_array,
		sync_data.player_session_id_array,
		sync_data.slot_array,
		sync_data.player_instance_id_array
	)
end

local function sync_host_profiles_to_peer(profile_synchronizer_host, channel_id)
	local sync_data = collect_local_player_sync_data(true)

	for i = 1, #(sync_data.local_player_id_array or {}) do
		profile_synchronizer_host:sync_player_profile(
			channel_id,
			Network.peer_id(),
			sync_data.local_player_id_array[i],
			sync_data.profile_chunks_array[i]
		)
	end
end

local function best_remote_profile_chunks(account_id, character_id, optional_archetype_name)
	local cached_profile = cached_remote_profile(account_id)
	if cached_profile then
		local chunks = split_profile_into_chunks(cached_profile)
		if chunks then
			return chunks
		end
	end

	local archetype_name = VALID_REMOTE_ARCHETYPE[optional_archetype_name] and optional_archetype_name or "veteran"
	local profile = {
		archetype = {
			name = archetype_name,
		},
		loadout_item_ids = {},
		talents = {},
	}

	log_remote_profile_source("stub", account_id)

	return split_profile_into_chunks(profile) or { "{}" }
end

local function notify_player_disconnected(members, leaving_channel_id, leaving_peer_id)
	for existing_channel_id, _ in pairs(members) do
		if existing_channel_id ~= leaving_channel_id then
			RPC.rpc_player_disconnected(existing_channel_id, leaving_peer_id)
		end
	end
end

ExperimentalConnectionHost.init = function (self, event_delegate, approve_channel_delegate, engine_lobby, host_type, tick_rate, max_members, optional_session_id)
	self.__class_name = "ConnectionHost"

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
	if self._profile_synchronizer_host and self._profile_synchronizer_host.update then
		self._profile_synchronizer_host:update(dt)
	end
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
	local member = self._connecting[channel_id] or self._members[channel_id]
	if member then
		self._events[#self._events + 1] = {
			name = "disconnected",
			parameters = {
				channel_id = channel_id,
				peer_id = member.peer_id,
				game_reason = reason,
				engine_reason = "remote_disconnected",
			},
		}
	end

    RPC.rpc_kicked(channel_id, reason, option_details)
    self._engine_lobby:close_channel(channel_id)
end

ExperimentalConnectionHost.remove = function (self, channel_id)
	local member = self._members[channel_id]
	if member then
		local peer_id = member.peer_id
		local package_synchronizer_host = Managers.package_synchronization and Managers.package_synchronization:synchronizer_host()

		if package_synchronizer_host and package_synchronizer_host.remove_peer then
			package_synchronizer_host:remove_peer(peer_id)
		end

		if self._profile_synchronizer_host and self._profile_synchronizer_host.peer_disconnected then
			self._profile_synchronizer_host:peer_disconnected(peer_id, channel_id)
		end

		for existing_channel_id, _ in pairs(self._members) do
			if existing_channel_id ~= channel_id then
				RPC.rpc_peer_left_session(existing_channel_id, peer_id)
			end
		end

		notify_player_disconnected(self._members, channel_id, peer_id)
	end

    self._event_delegate:unregister_channel_events(channel_id, unpack(HOST_RPCS))
    self._connecting[channel_id] = nil
    self._members[channel_id] = nil
    self._pending_sync[channel_id] = nil
    self._ticket_parts[channel_id] = nil
end

ExperimentalConnectionHost.disconnect = function (self, channel_id)
	local member = self._connecting[channel_id] or self._members[channel_id]
	if member then
		self._events[#self._events + 1] = {
			name = "disconnected",
			parameters = {
				channel_id = channel_id,
				peer_id = member.peer_id,
				engine_reason = "remote_disconnected",
			},
		}
	end

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

    local player_manager = Managers.player

    for i = 1, #local_player_id_array do
		if player_manager and player_manager.claim_slot then
			pending_sync.slot_array[i] = player_manager:claim_slot()
		else
			pending_sync.slot_array[i] = i
		end

		pending_sync.player_instance_id_array[i] = stable_remote_instance_id(self._connecting[channel_id].peer_id, local_player_id_array[i])
		pending_sync.profile_chunks_array[i] = pending_sync.profile_chunks_array[i]
			or best_remote_profile_chunks(pending_sync.account_id_array[i], pending_sync.character_id_array[i], "veteran")
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
	self._profile_synchronizer_host:register_rpcs(channel_id)
	self._profile_synchronizer_host:peer_connected(self._connecting[channel_id] and self._connecting[channel_id].peer_id or self._members[channel_id] and self._members[channel_id].peer_id, channel_id)
	sync_host_profiles_to_peer(self._profile_synchronizer_host, channel_id)
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

	local peer_id = connecting.peer_id
	local host_sync_data = collect_local_player_sync_data(false)
	local package_synchronizer_host = Managers.package_synchronization and Managers.package_synchronization:synchronizer_host()

	if package_synchronizer_host and package_synchronizer_host.add_peer then
		package_synchronizer_host:add_peer(peer_id)
	end

	RPC.rpc_sync_host_local_players(
		channel_id,
		host_sync_data.local_player_id_array,
		host_sync_data.is_human_controlled_array,
		host_sync_data.account_id_array,
		host_sync_data.player_session_id_array,
		host_sync_data.slot_array,
		host_sync_data.player_instance_id_array
	)

	for existing_channel_id, existing_member in pairs(self._members) do
		if existing_channel_id ~= channel_id then
			RPC.rpc_peer_joined_session(existing_channel_id, peer_id)
			RPC.rpc_peer_joined_session(channel_id, existing_member.peer_id)

			RPC.rpc_player_connected(
				existing_channel_id,
				peer_id,
				pending_sync.local_player_id_array,
				pending_sync.is_human_controlled_array,
				pending_sync.account_id_array,
				pending_sync.player_session_id_array,
				pending_sync.slot_array,
				pending_sync.player_instance_id_array
			)

			RPC.rpc_player_connected(
				channel_id,
				existing_member.peer_id,
				host_sync_data.local_player_id_array,
				host_sync_data.is_human_controlled_array,
				host_sync_data.account_id_array,
				host_sync_data.player_session_id_array,
				host_sync_data.slot_array,
				host_sync_data.player_instance_id_array
			)
		end
	end

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
