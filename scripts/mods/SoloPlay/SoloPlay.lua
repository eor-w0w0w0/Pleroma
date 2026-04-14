local mod = get_mod("SoloPlay")
local Promise = require("scripts/foundation/utilities/promise")
local MissionTemplates = require("scripts/settings/mission/mission_templates")
local DangerSettings = require("scripts/settings/difficulty/danger_settings")
local MatchmakingConstants = require("scripts/settings/network/matchmaking_constants")
local DifficultyManager = require("scripts/managers/difficulty/difficulty_manager")
local GameModeSurvival = require("scripts/managers/game_mode/game_modes/game_mode_survival")
local GameModeManager = require("scripts/managers/game_mode/game_mode_manager")
local PacingManager = require("scripts/managers/pacing/pacing_manager")
local PacingTemplates = require("scripts/managers/pacing/pacing_templates")
local RoamerPacing = require("scripts/managers/pacing/roamer_pacing/roamer_pacing")
local PickupSystem = require("scripts/extension_systems/pickups/pickup_system")
local PickupSettings = require("scripts/settings/pickup/pickup_settings")
local UISoundEvents = require("scripts/settings/ui/ui_sound_events")
local WwiseGameSyncSettings = require("scripts/settings/wwise_game_sync/wwise_game_sync_settings")
local MultiplayerSession = require("scripts/managers/multiplayer/multiplayer_session")
local SoloPlaySettings = mod:io_dofile("SoloPlay/scripts/mods/SoloPlay/SoloPlaySettings")
local ExperimentalPlayerHostedSessionBoot = mod:io_dofile("SoloPlay/scripts/mods/SoloPlay/experimental/player_hosted_session_boot")
mod:io_dofile("SoloPlay/scripts/mods/SoloPlay/havoc")
mod:io_dofile("SoloPlay/scripts/mods/SoloPlay/workaround")

local HOST_TYPES = MatchmakingConstants.HOST_TYPES
local DISTRIBUTION_TYPES = PickupSettings.distribution_types

mod.on_all_mods_loaded = function ()
	mod.load_package("packages/ui/views/end_player_view/end_player_view")
end

mod.is_soloplay = function ()
	if not Managers.state.game_mode then
		return false
	end
	local game_mode_name = Managers.state.game_mode:game_mode_name()
	if game_mode_name == "training_grounds" or game_mode_name == "shooting_range" then
		return false
	end
	if not Managers.multiplayer_session then
		return false
	end
	local host_type = Managers.multiplayer_session:host_type()
	return host_type == HOST_TYPES.singleplay
end

mod.load_package = function (package_name)
	if not Managers.package:is_loading(package_name) and not Managers.package:has_loaded(package_name) then
		Managers.package:load(package_name, "solo_play", nil, true)
	end
end

local raw_io = Mods and Mods.lua and Mods.lua.io or io
local raw_os = Mods and Mods.lua and Mods.lua.os or os

local function current_engine_lobby()
	local connection_manager = Managers.connection

	if not connection_manager then
		return nil
	end

	if connection_manager.engine_lobby then
		local ok, lobby = pcall(connection_manager.engine_lobby, connection_manager)
		if ok then
			return lobby
		end
	end

	if connection_manager.lobby then
		local ok, lobby = pcall(connection_manager.lobby, connection_manager)
		if ok then
			return lobby
		end
	end

	return nil
end

local function current_wan_client()
	local connection_manager = Managers.connection

	if not connection_manager or not connection_manager.client then
		return nil
	end

	local ok, client = pcall(connection_manager.client, connection_manager)

	if ok then
		return client
	end

	return nil
end

local function get_method_names(value)
	local methods = {}
	local mt = getmetatable(value)
	local index = mt and mt.__index

	if type(index) == "table" then
		for key, callback in pairs(index) do
			if type(callback) == "function" then
				methods[#methods + 1] = key
			end
		end
	elseif type(mt) == "table" then
		for key, callback in pairs(mt) do
			if type(callback) == "function" and string.sub(key, 1, 2) ~= "__" then
				methods[#methods + 1] = key
			end
		end
	end

	table.sort(methods)

	return methods
end

local function get_party_player_count()
	local party_manager = Managers.party_immaterium

	if not party_manager then
		return 1
	end

	local ok_num, other_members = pcall(party_manager.num_other_members, party_manager)
	if ok_num and type(other_members) == "number" then
		return math.max(1, other_members + 1)
	end

	local ok_members, all_members = pcall(party_manager.all_members, party_manager)
	if ok_members and type(all_members) == "table" then
		return math.max(1, #all_members)
	end

	return 1
end

local function get_prefered_mission_region()
	local region_latency = Managers.data_service and Managers.data_service.region_latency

	if not region_latency or not region_latency.get_prefered_mission_region then
		return nil
	end

	local ok, region = pcall(region_latency.get_prefered_mission_region, region_latency)

	if ok then
		return region
	end

	return nil
end

local function value_matches(selected, actual)
	if not selected or selected == "" or selected == "default" then
		return true
	end

	return selected == actual
end

local function resolve_live_mission_id(mission_context, missions)
	if not missions or table.is_empty(missions) then
		return nil, "msg_private_no_live_mission"
	end

	local best_score = -1
	local best_mission_id = nil

	for i = 1, #missions do
		local mission = missions[i]

		if mission and mission.id and mission.map == mission_context.mission_name and mission.challenge == mission_context.challenge and mission.resistance == mission_context.resistance then
			local score = 0

			if value_matches(mission_context.side_mission, mission.sideMission or mission.side_mission) then
				score = score + 1
			elseif mission_context.side_mission ~= "default" then
				score = -1
			end

			if score >= 0 then
				if value_matches(mission_context.circumstance_name, mission.circumstance or mission.circumstance_name) then
					score = score + 1
				elseif mission_context.circumstance_name ~= "default" then
					score = -1
				end
			end

			if score >= 0 then
				local wanted_giver = mission_context.mission_giver_vo_override or "default"
				if value_matches(wanted_giver, mission.missionGiver or mission.mission_giver) then
					score = score + 1
				elseif wanted_giver ~= "default" then
					score = -1
				end
			end

			if score > best_score then
				best_score = score
				best_mission_id = mission.id
			end
		end
	end

	if best_mission_id then
		return best_mission_id
	end

	return nil, "msg_private_no_live_mission"
end

local function can_start_private_normal_game()
	local party_manager = Managers.party_immaterium

	if not party_manager or get_party_player_count() <= 1 then
		return false, "msg_private_requires_party"
	end

	if party_manager.are_all_members_in_hub then
		local ok, all_available = pcall(party_manager.are_all_members_in_hub, party_manager)
		if ok and not all_available then
			return false, "msg_private_team_not_available"
		end
	end

	return true
end

local function should_use_experimental_party_hosted_launch()
	local status = native_feature_status()

	return status.host_test_enabled == true and get_party_player_count() > 1
end

local function start_experimental_party_hosted_session(mission_context)
	set_native_flag("SoloPlayNativeHostPatch.host_test_enabled", true)

	local browser = ensure_validation_browser()
	if browser then
		call_browser_method(browser, "refresh")
	end

	local engine_lobby = current_engine_lobby()
	local multiplayer_session_manager = Managers.multiplayer_session
	local party_manager = Managers.party_immaterium
	local native_status = native_feature_status()

	if not engine_lobby or not multiplayer_session_manager then
		return false, "msg_party_host_missing_lobby"
	end

	mod:dump({
		action = "start_experimental_party_hosted_session",
		native_status = native_status,
		browser_status = collect_browser_status(browser),
		mission_context = mission_context,
		engine_lobby = tostring(engine_lobby),
		engine_lobby_id = engine_lobby.id and engine_lobby:id() or nil,
		party_id = party_manager and party_manager.party_id and party_manager:party_id() or nil,
		party_members = party_manager and party_manager.all_members and party_manager:all_members() or nil,
	}, "solo_party_hosted_start", 3)

	multiplayer_session_manager:clear_session_boot()

	local new_session = MultiplayerSession:new()
	local tick_rate = GameParameters.tick_rate or 30
	local max_members = math.max(4, get_party_player_count())

	multiplayer_session_manager._session_boot = ExperimentalPlayerHostedSessionBoot:new(
		new_session,
		engine_lobby,
		HOST_TYPES.player,
		tick_rate,
		max_members,
		engine_lobby:id()
	)

	local mechanism_manager = Managers.mechanism
	local mission_settings = MissionTemplates[mission_context.mission_name]
	local mechanism_name = mission_settings and mission_settings.mechanism_name or "adventure"
	local mechanism_context = table.clone(mission_context)
	local loading_manager = Managers.loading

	mechanism_manager:change_mechanism(mechanism_name, mechanism_context)

	mod:dump({
		action = "start_experimental_party_hosted_session_post_boot",
		session_boot = tostring(multiplayer_session_manager._session_boot),
		session_boot_class = multiplayer_session_manager._session_boot and multiplayer_session_manager._session_boot.__class_name or nil,
		session_boot_state = multiplayer_session_manager._session_boot and multiplayer_session_manager._session_boot.state and multiplayer_session_manager._session_boot:state() or nil,
		session = tostring(multiplayer_session_manager._session),
		loading_is_alive = loading_manager and loading_manager.is_alive and loading_manager:is_alive() or nil,
		loading_is_host = loading_manager and loading_manager.is_host and loading_manager:is_host() or nil,
		loading_host = loading_manager and tostring(loading_manager._loading_host) or nil,
	}, "solo_party_hosted_post_boot", 3)

	mod:echo("experimental party-hosted boot started: " .. tostring(mechanism_name))
	return true
end

local function destroy_validation_browser()
	local browser = mod._validation_lobby_browser
	local wan_client = mod._validation_browser_client

	mod._validation_lobby_browser = nil
	mod._validation_browser_client = nil

	if browser and wan_client and LanClient and LanClient.destroy_lobby_browser then
		pcall(LanClient.destroy_lobby_browser, wan_client, browser)
	end
end

local function ensure_validation_browser()
	local browser = mod._validation_lobby_browser
	local wan_client = mod._validation_browser_client

	if browser and wan_client then
		return browser, wan_client
	end

	wan_client = current_wan_client()

	if not wan_client or not LanClient or not LanClient.create_lobby_browser then
		return nil, wan_client
	end

	local ok, new_browser = pcall(LanClient.create_lobby_browser, wan_client)
	if not ok or not new_browser then
		return nil, wan_client
	end

	mod._validation_lobby_browser = new_browser
	mod._validation_browser_client = wan_client

	return new_browser, wan_client
end

local function call_browser_method(browser, method_name, ...)
	if not browser then
		return false, "missing_browser"
	end

	local method = browser[method_name]

	if type(method) ~= "function" then
		return false, "missing_method"
	end

	return pcall(method, browser, ...)
end

local function collect_browser_status(browser)
	if not browser then
		return {
			has_browser = false,
		}
	end

	local status = {
		browser = tostring(browser),
		has_browser = true,
	}

	local ok_refreshing, refreshing = call_browser_method(browser, "is_refreshing")
	status.is_refreshing_ok = ok_refreshing
	status.is_refreshing = ok_refreshing and refreshing or tostring(refreshing)

	local ok_num, num_lobbies = call_browser_method(browser, "num_lobbies")
	status.num_lobbies_ok = ok_num
	status.num_lobbies = ok_num and num_lobbies or tostring(num_lobbies)

	local ok_handshake, handshake_status = call_browser_method(browser, "client_handshake_status")
	status.client_handshake_status_ok = ok_handshake
	status.client_handshake_status = ok_handshake and handshake_status or tostring(handshake_status)

	return status
end

local function get_binaries_path()
	if not raw_io or not raw_io.popen then
		return nil
	end

	local handle = raw_io.popen("cd")
	if not handle then
		return nil
	end

	local path = handle:read("*a")
	handle:close()

	if not path then
		return nil
	end

	path = string.gsub(path, "[\r\n]+", "")

	if path == "" then
		return nil
	end

	return path
end

local function native_flag_path(file_name)
	local binaries_path = get_binaries_path()

	if not binaries_path then
		return nil
	end

	return binaries_path .. "\\" .. file_name
end

local function native_flag_exists(file_name)
	local path = native_flag_path(file_name)

	if not path or not raw_io or not raw_io.open then
		return false
	end

	local handle = raw_io.open(path, "r")
	if not handle then
		return false
	end

	handle:close()
	return true
end

local function set_native_flag(file_name, enabled)
	local path = native_flag_path(file_name)

	if not path or not raw_io or not raw_io.open then
		return false
	end

	if enabled then
		local handle = raw_io.open(path, "w+")
		if not handle then
			return false
		end

		handle:write("enabled\n")
		handle:close()
		return true
	end

	if raw_os and raw_os.remove then
		raw_os.remove(path)
	end

	return not native_flag_exists(file_name)
end

local function disable_all_native_host_flags()
	local ok_host = set_native_flag("SoloPlayNativeHostPatch.host_test_enabled", false)
	local ok_browser = set_native_flag("SoloPlayNativeHostPatch.enable_browser_callback", false)
	local ok_reply = set_native_flag("SoloPlayNativeHostPatch.enable_discovery_reply", false)
	local ok_capture = set_native_flag("SoloPlayNativeHostPatch.enable_registration_capture", false)

	return ok_host and ok_browser and ok_reply and ok_capture
end

local function native_feature_status()
	local host_test_enabled = native_flag_exists("SoloPlayNativeHostPatch.host_test_enabled")

	return {
		host_test_enabled = host_test_enabled,
		browser_callback = host_test_enabled or native_flag_exists("SoloPlayNativeHostPatch.enable_browser_callback"),
		discovery_reply = host_test_enabled or native_flag_exists("SoloPlayNativeHostPatch.enable_discovery_reply"),
		registration_capture = host_test_enabled or native_flag_exists("SoloPlayNativeHostPatch.enable_registration_capture"),
		path = get_binaries_path(),
	}
end

local function dump_hosted_runtime_state(label)
	local multiplayer_session_manager = Managers.multiplayer_session
	local session_boot = multiplayer_session_manager and multiplayer_session_manager._session_boot
	local session = multiplayer_session_manager and multiplayer_session_manager._session
	local loading_manager = Managers.loading
	local loading_host = loading_manager and loading_manager._loading_host

	mod:dump({
		label = label,
		session_boot = tostring(session_boot),
		session_boot_class = session_boot and session_boot.__class_name or nil,
		session_boot_state = session_boot and session_boot.state and session_boot:state() or nil,
		session = tostring(session),
		host_type = multiplayer_session_manager and multiplayer_session_manager.host_type and multiplayer_session_manager:host_type() or nil,
		loading_is_alive = loading_manager and loading_manager.is_alive and loading_manager:is_alive() or nil,
		loading_is_host = loading_manager and loading_manager.is_host and loading_manager:is_host() or nil,
		loading_host = tostring(loading_host),
		loading_host_state = loading_host and loading_host._state or nil,
		loading_host_clients = loading_host and loading_host._clients or nil,
	}, label, 3)
end

local function schedule_hosted_runtime_dumps()
	mod._hosted_runtime_dump_schedule = {
		launched_at = Application and Application.time_since_launch and Application.time_since_launch() or 0,
		next_index = 1,
		offsets = { 1, 3, 6 },
	}
end

local function update_hosted_runtime_dumps()
	local schedule = mod._hosted_runtime_dump_schedule
	if not schedule then
		return
	end

	local now = Application and Application.time_since_launch and Application.time_since_launch() or 0
	local index = schedule.next_index
	local offset_t = schedule.offsets[index]

	if not offset_t then
		mod._hosted_runtime_dump_schedule = nil
		return
	end

	if now >= schedule.launched_at + offset_t then
		dump_hosted_runtime_state("solo_party_hosted_runtime_" .. tostring(index))
		schedule.next_index = index + 1
	end
end

local function maybe_prime_native_browser_transport()
	local status = native_feature_status()

	if not status.browser_callback then
		return
	end

	if not Managers.connection then
		return
	end

	local now = Application and Application.time_since_launch and Application.time_since_launch() or 0
	local next_allowed_t = mod._next_native_browser_prime_t or 0

	if now < next_allowed_t then
		return
	end

	mod._next_native_browser_prime_t = now + 5

	local browser = ensure_validation_browser()

	if not browser then
		return
	end

	local ok_refreshing, is_refreshing = call_browser_method(browser, "is_refreshing")
	if ok_refreshing and is_refreshing then
		return
	end

	call_browser_method(browser, "refresh")
end

mod.parse_mission_params = function (mission_name_value)
	local parts = string.split(mission_name_value, "|")
	if #parts < 2 then
		return parts[1], ""
	end
	return parts[1], parts[2]
end

mod.gen_normal_mission_context = function ()
	local mission_name, params = mod.parse_mission_params(mod:get("choose_mission"))
	local difficulty = DangerSettings[mod:get("choose_difficulty")]
	local mission_context = {
		mission_name = mission_name,
		challenge = difficulty.challenge,
		resistance = difficulty.resistance,
		circumstance_name = mod:get("choose_circumstance"),
		side_mission = mod:get("choose_side_mission"),
		custom_params = params,
	}
	local mission_giver = mod:get("choose_mission_giver")
	if mission_giver ~= "default" then
		mission_context.mission_giver_vo_override = mission_giver
	end
	for map_name, override in pairs(SoloPlaySettings.context_override) do
		if mission_context.mission_name == map_name then
			for key, settings in pairs(override) do
				mission_context[key] = settings.value
			end
		end
	end
	return mission_context
end

mod.gen_havoc_mission_context = function ()
	local rank = mod:get("havoc_difficulty")
	local challenge, resistance
	if rank <= 10 then
		challenge = 3
		resistance = 3
	elseif rank >= 11 and rank <= 20 then
		challenge = 4
		resistance = 4
	elseif rank >= 21 and rank <= 30 then
		challenge = 5
		resistance = 4
	else
		challenge = 5
		resistance = 5
	end

	local mission, params = mod.parse_mission_params(mod:get("havoc_mission"))
	local faction = mod:get("havoc_faction")
	local circumstance1 = mod:get("havoc_circumstance1")
	local circumstance2 = mod:get("havoc_circumstance2")
	local theme_circumstance = mod:get("havoc_theme_circumstance")
	local difficulty_circumstance = mod:get("havoc_difficulty_circumstance")
	local theme = SoloPlaySettings.lookup.theme_of_circumstances[theme_circumstance]

	local chosen_circumstances_table = {}
	if circumstance1 ~= "default" then
		chosen_circumstances_table[#chosen_circumstances_table+1] = circumstance1
	end
	if circumstance2 ~= "default" and circumstance2 ~= circumstance1 then
		chosen_circumstances_table[#chosen_circumstances_table+1] = circumstance2
	end
	if theme_circumstance ~= "default" then
		chosen_circumstances_table[#chosen_circumstances_table+1] = theme_circumstance
	end
	if difficulty_circumstance ~= "default" then
		chosen_circumstances_table[#chosen_circumstances_table+1] = difficulty_circumstance
	end
	local chosen_circumstances = table.concat(chosen_circumstances_table, ":")

	local chosen_modifiers_table = {}
	for modifier_name in pairs(SoloPlaySettings.lookup.havoc_modifiers_max_level) do
		local modifier_id = NetworkLookup.havoc_modifiers[modifier_name]
		local level = mod:get("havoc_modifier_" .. modifier_name) or 0
		if level > 0 then
			chosen_modifiers_table[#chosen_modifiers_table+1] = string.format("%d.%d", modifier_id, level)
		end
	end
	local chosen_modifiers = table.concat(chosen_modifiers_table, ":")

	local data = string.format("%s;%d;%s;%s;%s;%s;%s;%s", mission, rank, theme, faction, chosen_circumstances, chosen_modifiers, challenge, resistance)

	local mission_context = {
		mission_name = mission,
		challenge = challenge,
		resistance = resistance,
		havoc_data = data,
		custom_params = params,
	}
	local mission_giver = mod:get("havoc_mission_giver")
	if mission_giver ~= "default" then
		mission_context.mission_giver_vo_override = mission_giver
	end
	return mission_context
end

mod:hook_require("scripts/ui/views/system_view/system_view_content_list", function (instance)
	local leave_mission_occur = 1
	for _, item in ipairs(instance.default) do
		if item.text == "loc_exit_to_main_menu_display_name" then
			item.validation_function = function ()
				local game_mode_manager = Managers.state.game_mode
				if not game_mode_manager then
					return false
				end

				local game_mode_name = game_mode_manager:game_mode_name()
				local is_onboarding = game_mode_name == "prologue" or game_mode_name == "prologue_hub"
				local is_hub = game_mode_name == "hub"
				local is_training_grounds = game_mode_name == "training_grounds" or game_mode_name == "shooting_range"

				local host_type = Managers.multiplayer_session:host_type()
				local can_show = is_onboarding or is_hub or is_training_grounds or host_type == HOST_TYPES.singleplay
				local is_leaving_game = game_mode_manager:game_mode_state() == "leaving_game"
				local is_in_matchmaking = Managers.data_service.social:is_in_matchmaking()
				local is_disabled = is_leaving_game or is_in_matchmaking

				return can_show, is_disabled
			end
		elseif item.text == "loc_leave_mission_display_name" and leave_mission_occur == 1 then
			item.validation_function = function ()
				local game_mode_manager = Managers.state.game_mode
				local is_training_grounds = false
				if game_mode_manager then
					local game_mode_name = game_mode_manager:game_mode_name()
					is_training_grounds = game_mode_name == "training_grounds" or game_mode_name == "shooting_range"
				end

				local host_type = Managers.multiplayer_session:host_type()
				local mechanism = Managers.mechanism:current_mechanism()
				local mechanism_data = mechanism and mechanism:mechanism_data()

				if is_training_grounds then
					return false
				end
				if host_type == HOST_TYPES.singleplay then
					return true
				end
				if host_type == HOST_TYPES.mission_server then
					return mechanism_data and not mechanism_data.havoc_data
				end
				return false
			end
			leave_mission_occur = leave_mission_occur + 1
		elseif item.text == "loc_leave_mission_display_name" and leave_mission_occur == 2 then
			item.validation_function = function ()
				local game_mode_manager = Managers.state.game_mode
				local is_training_grounds = false
				if game_mode_manager then
					local game_mode_name = game_mode_manager:game_mode_name()
					is_training_grounds = game_mode_name == "training_grounds" or game_mode_name == "shooting_range"
				end

				local host_type = Managers.multiplayer_session:host_type()
				local mechanism = Managers.mechanism:current_mechanism()
				local mechanism_data = mechanism and mechanism:mechanism_data()

				if is_training_grounds then
					return false
				end
				if host_type == HOST_TYPES.singleplay then
					return false
				end
				if host_type == HOST_TYPES.mission_server then
					return mechanism_data and mechanism_data.havoc_data
				end
				return false
			end
			leave_mission_occur = leave_mission_occur + 1
		end
	end
end)

mod:hook(DifficultyManager, "friendly_fire_enabled", function (func, self, target_is_player, target_is_minion)
	local ret = func(self, target_is_player, target_is_minion)
	if mod.is_soloplay() and mod:get("friendly_fire_enabled") then
		return true
	end
	return ret
end)

mod:hook(GameModeSurvival, "select_random_island", function (func, self)
	local selected_island_name = mod:get("_horde_selected_island")
	if not selected_island_name or selected_island_name == "" then
		return func(self)
	end

	if not self._is_server then
		return
	end

	self._island_selected = selected_island_name
	Log.info("GameModeSurvival", "Server selected Island to play: %s", selected_island_name)

	local level = Managers.state.mission:mission_level()
	local game_mode_name = Managers.state.game_mode and Managers.state.game_mode:game_mode_name()

	if game_mode_name and level then
		local island_selected_level_event_name = "horde_mode_" .. selected_island_name .. "_selected"
		Level.trigger_event(level, island_selected_level_event_name)
	end

	local island_name_id = NetworkLookup.hordes_island_names[selected_island_name]
	Managers.state.game_session:send_rpc_clients("rpc_client_hordes_set_selected_island", island_name_id)
end)

mod:hook(PacingManager, "init", function (func, self, world, nav_world, level_seed, pacing_control)
	func(self, world, nav_world, level_seed, pacing_control)
	local is_havoc = Managers.state.difficulty:get_parsed_havoc_data()
	if not is_havoc then
		if Managers.mechanism and Managers.mechanism._mechanism and Managers.mechanism._mechanism._mechanism_data then
			local mechanism_data = Managers.mechanism._mechanism._mechanism_data
			local pacing_override = SoloPlaySettings.pacing_override[mechanism_data.mission_name]
			if pacing_override and PacingTemplates[pacing_override] then
				local template = PacingTemplates[pacing_override]
				local game_mode_settings = Managers.state.game_mode:settings()
				local side_sub_faction_types = game_mode_settings.side_sub_faction_types
				local sub_faction_types = side_sub_faction_types[self._side_name]
				self._template = template
				self._roamer_pacing = RoamerPacing:new(nav_world, template.roamer_pacing_template, level_seed, sub_faction_types)
			end
		end
	end
end)

mod:hook(PickupSystem, "spawn_spread_pickups", function (func, self, distribution_type, pickup_pool, seed)
	if distribution_type == DISTRIBUTION_TYPES.side_mission and mod:get("random_side_mission_seed") then
		self._seed = func(self, distribution_type, pickup_pool, self._seed)
		return self._seed
	end
	return func(self, distribution_type, pickup_pool, seed)
end)

local main_menu_want_solo_play = nil

mod:hook(CLASS.StateMainMenu, "update", function (func, self, main_dt, main_t)
	update_hosted_runtime_dumps()

	if self._continue and not self:_waiting_for_synchronizations() then
		if main_menu_want_solo_play then
			local mode = main_menu_want_solo_play
			main_menu_want_solo_play = nil

			if mode == "normal_multiplayer" or mode == "havoc_multiplayer" then
				local mission_context = mode == "havoc_multiplayer" and mod.gen_havoc_mission_context() or mod.gen_normal_mission_context()
				local started_hosted, hosted_error_loc_key = start_experimental_party_hosted_session(mission_context)
				if started_hosted then
					local next_state, state_context = Managers.mechanism:wanted_transition()
					return next_state, state_context
				end

				mod:notify(mod:localize(hosted_error_loc_key))
				return func(self, main_dt, main_t)
			end

			local mission_context = {}
			if mode == "havoc" then
				mission_context = mod.gen_havoc_mission_context()
			else
				mission_context = mod.gen_normal_mission_context()
			end
			local mission_settings = MissionTemplates[mission_context.mission_name]
			local mechanism_name = mission_settings.mechanism_name

			if mission_context.custom_params and SoloPlaySettings.custom_params_handlers[mission_context.mission_name] then
				SoloPlaySettings.custom_params_handlers[mission_context.mission_name](mission_context.mission_name, mission_context.custom_params)
			end

			disable_all_native_host_flags()
			destroy_validation_browser()
			Managers.multiplayer_session:boot_singleplayer_session()
			Managers.mechanism:change_mechanism(mechanism_name, mission_context)
			Managers.mechanism:trigger_event("all_players_ready")

			local next_state, state_context = Managers.mechanism:wanted_transition()
			return next_state, state_context
		end
	elseif self._continue then
		self._continue = false
	end

	local next_state, next_state_params = func(self, main_dt, main_t)
	return next_state, next_state_params
end)

mod:hook(CLASS.StateGame, "update", function (func, self, dt)
	update_hosted_runtime_dumps()
	return func(self, dt)
end)

local function in_hub_or_psykhanium()
	if not Managers.state or not Managers.state.game_mode then
		return false
	end
	local game_mode_name = Managers.state.game_mode:game_mode_name()
	return (game_mode_name == "hub" or game_mode_name == "prologue_hub" or game_mode_name == "shooting_range")
end

local function on_main_menu()
	if not Managers.ui then
		return false
	end
	return Managers.ui:view_active("main_menu_view")
end

mod.can_start_game = function ()
	if in_hub_or_psykhanium() or mod.is_soloplay() or on_main_menu() then
		return true
	end
	return false
end

mod.start_private_normal_game = function ()
	local can_start, error_loc_key = can_start_private_normal_game()
	if not can_start then
		return false, error_loc_key
	end

	local mission_context = mod.gen_normal_mission_context()
	local mission_board = Managers.data_service and Managers.data_service.mission_board

	if not mission_board or not mission_board.fetch then
		return false, "msg_private_no_live_mission"
	end

	mission_board:fetch(nil, 1):next(function (missions_data)
		local mission_id, mission_error_loc_key = resolve_live_mission_id(mission_context, missions_data and missions_data.missions)
		if not mission_id then
			mod:notify(mod:localize(mission_error_loc_key))
			return
		end

		local preferred_mission_region = get_prefered_mission_region()
		local party_manager = Managers.party_immaterium
		local ok = pcall(party_manager.wanted_mission_selected, party_manager, mission_id, true, preferred_mission_region)

		if not ok then
			mod:notify(mod:localize("msg_private_no_live_mission"))
		end
	end):catch(function (error)
		mod:dump(error, "solo_private_mission_fetch_error", 3)
		mod:notify(mod:localize("msg_private_no_live_mission"))
	end)

	return true
end

mod.start_game = function (mode)
	if not mod.can_start_game() then
		mod:notify(mod:localize("msg_not_in_hub_or_mission"))
		return
	end

	if mode == "normal_multiplayer" then
		if get_party_player_count() <= 1 then
			mod:notify(mod:localize("msg_private_requires_party"))
			return
		end

		local started_hosted, hosted_error_loc_key = start_experimental_party_hosted_session(mod.gen_normal_mission_context())
		if started_hosted then
			return
		end

		mod:notify(mod:localize(hosted_error_loc_key))
		return
	end

	if mode == "havoc_multiplayer" then
		if get_party_player_count() <= 1 then
			mod:notify(mod:localize("msg_private_requires_party"))
			return
		end

		local started_hosted, hosted_error_loc_key = start_experimental_party_hosted_session(mod.gen_havoc_mission_context())
		if started_hosted then
			return
		end

		mod:notify(mod:localize(hosted_error_loc_key))
		return
	end

	if mode == "normal" and not on_main_menu() then
		if get_party_player_count() > 1 then
			local started_private, private_error_loc_key = mod.start_private_normal_game()
			if started_private then
				return
			end

			mod:notify(mod:localize(private_error_loc_key))
			return
		end
	elseif mode == "havoc" and get_party_player_count() > 1 then
		mod:notify(mod:localize("msg_havoc_strike_team_unavailable"))
		return
	end

	if on_main_menu() then
		main_menu_want_solo_play = mode
		Managers.event:trigger("event_state_main_menu_continue")
		return
	end

	local mission_context
	if mode == "havoc" then
		mission_context = mod.gen_havoc_mission_context()
	else
		mission_context = mod.gen_normal_mission_context()
	end
	local mission_settings = MissionTemplates[mission_context.mission_name]
	local mechanism_name = mission_settings.mechanism_name

	if mission_context.custom_params and SoloPlaySettings.custom_params_handlers[mission_context.mission_name] then
		SoloPlaySettings.custom_params_handlers[mission_context.mission_name](mission_context.mission_name, mission_context.custom_params)
	end

	disable_all_native_host_flags()
	destroy_validation_browser()
	Managers.multiplayer_session:reset("Hosting SoloPlay session")
	Managers.multiplayer_session:boot_singleplayer_session()

	Promise.until_true(function ()
		if not Managers.multiplayer_session._session_boot or not Managers.multiplayer_session._session_boot.leaving_game_session then
			return false
		end
		return true
	end):next(function ()
		Managers.mechanism:change_mechanism(mechanism_name, mission_context)
		Managers.mechanism:trigger_event("all_players_ready")
	end)
end

mod:add_require_path("SoloPlay/scripts/mods/SoloPlay/soloplay_mod_view/soloplay_mod_view")
mod:register_view({
	view_name = "soloplay_mod_view",
	view_settings = {
		init_view_function = function (ingame_ui_context)
			return true
		end,
		state_bound = true,
		path = "SoloPlay/scripts/mods/SoloPlay/soloplay_mod_view/soloplay_mod_view",
		class = "SoloPlayModView",
		disable_game_world = false,
		load_always = true,
		load_in_hub = true,
		game_world_blur = 1.1,
		enter_sound_events = {
			UISoundEvents.system_menu_enter,
		},
		exit_sound_events = {
			UISoundEvents.system_menu_exit,
		},
		wwise_states = {
			options = WwiseGameSyncSettings.state_groups.options.ingame_menu,
		},
		context = {
			use_item_categories = false,
		},
	},
	view_transitions = {},
	view_options = {
		close_all = false,
		close_previous = false,
		close_transition_time = nil,
		transition_time = nil,
	},
})

mod.open_solo_view = function ()
	if not Managers.ui:view_instance("soloplay_mod_view") then
		Managers.ui:open_view("soloplay_mod_view", nil, nil, nil, nil, {})
	end
end

mod.keybind_open_solo_view = function ()
	if not Managers.ui:chat_using_input() then
		mod.open_solo_view()
	end
end

mod.keybind_open_inventory = function()
	if Managers.ui:chat_using_input() then
		return
	end
	if not mod.is_soloplay() then
		return
	end
	local active = false
	local active_views = Managers.ui:active_views()
	for _, active_view in pairs(active_views) do
		if active_view == "inventory_background_view" then
			active = true
		end
	end
	if active then
		return
	end
	Managers.ui:open_view("inventory_background_view", nil, nil, nil, nil, nil)
end

mod:command("solo", mod:localize("solo_command_desc"), function ()
	mod.open_solo_view()
end)

mod:command("solo_netdump", "Dump LAN/runtime network surfaces for SoloPlay native validation", function ()
	local browser = mod._validation_lobby_browser
	local engine_lobby = current_engine_lobby()

	mod:dump({
		Network = get_method_names(Network),
		LanClient = get_method_names(LanClient),
		validation_browser = browser and get_method_names(browser) or {},
		engine_lobby = engine_lobby and get_method_names(engine_lobby) or {},
	}, "solo_netdump", 3)

	mod:notify(mod:localize("msg_netdump_done"))
end)

mod:command("solo_rpcdump", "Dump known host/bootstrap RPC metadata", function ()
	local known_rpcs = {
		"rpc_request_host_type",
		"rpc_request_host_type_reply",
		"rpc_sync_local_players",
		"rpc_sync_local_players_reply",
		"rpc_sync_host_local_players",
		"rpc_claim_slot",
		"rpc_claim_slot_reply",
		"rpc_reserve_slots",
		"rpc_reserve_slots_reply",
		"rpc_client_entered_connected_state",
		"rpc_player_connected",
		"rpc_player_disconnected",
		"rpc_peer_joined_session",
		"rpc_peer_left_session",
	}

	local rpc_info = {}
	local message_info = Network and Network.message_info or {}

	for i = 1, #known_rpcs do
		local rpc_name = known_rpcs[i]
		local info = message_info and message_info[rpc_name]
		local entry = {
			exists = info ~= nil,
		}

		if info then
			entry.reliable = info.reliable
			entry.session_bound = info.session_bound
			entry.arguments = info.arguments
		end

		rpc_info[rpc_name] = entry
	end

	mod:dump(rpc_info, "solo_rpcdump", 3)
	mod:notify(mod:localize("msg_rpcdump_done"))
end)

mod:command("solo_partydump", "Dump current strike-team / session state", function ()
	local party_manager = Managers.party_immaterium
	local connection_manager = Managers.connection
	local multiplayer_session = Managers.multiplayer_session
	local connection_client = connection_manager and connection_manager._connection_client
	local connection_host = connection_manager and connection_manager._connection_host
	local party_game_state = party_manager and party_manager.party_game_state and party_manager:party_game_state() or nil
	local all_members = party_manager and party_manager.all_members and party_manager:all_members() or nil

	mod:dump({
		party_id = party_manager and party_manager.party_id and party_manager:party_id() or nil,
		current_game_session_id = party_manager and party_manager.current_game_session_id and party_manager:current_game_session_id() or nil,
		current_game_session_mission_data = party_manager and party_manager.current_game_session_mission_data and party_manager:current_game_session_mission_data() or nil,
		game_session_in_progress = party_manager and party_manager.game_session_in_progress and party_manager:game_session_in_progress() or false,
		have_received_game_state = party_manager and party_manager.have_recieved_game_state and party_manager:have_recieved_game_state() or false,
		party_game_state = party_game_state,
		all_members = all_members,
		num_other_members = party_manager and party_manager.num_other_members and party_manager:num_other_members() or 0,
		connection_client = tostring(connection_client),
		connection_host = tostring(connection_host),
		connection_client_host_type = connection_client and connection_client.host_type and connection_client:host_type() or nil,
		connection_client_matched_game_session_id = connection_client and connection_client.matched_game_session_id and connection_client:matched_game_session_id() or nil,
		connection_client_initial_party_id = connection_client and connection_client.initial_party_id and connection_client:initial_party_id() or nil,
		multiplayer_session_host_type = multiplayer_session and multiplayer_session.host_type and multiplayer_session:host_type() or nil,
		multiplayer_session_has_joined_host = multiplayer_session and multiplayer_session.has_joined_host and multiplayer_session:has_joined_host() or nil,
	}, "solo_partydump", 3)

	mod:notify(mod:localize("msg_partydump_done"))
end)

mod:command("solo_sessionbootdump", "Dump the current multiplayer session boot object", function ()
	local multiplayer_session_manager = Managers.multiplayer_session
	local session_boot = multiplayer_session_manager and multiplayer_session_manager._session_boot
	local session = multiplayer_session_manager and multiplayer_session_manager._session

	mod:dump({
		session_boot = tostring(session_boot),
		session_boot_class = session_boot and session_boot.__class_name or nil,
		session_boot_state = session_boot and session_boot.state and session_boot:state() or nil,
		session = tostring(session),
		session_class = session and session.__class_name or nil,
		host_type = multiplayer_session_manager and multiplayer_session_manager.host_type and multiplayer_session_manager:host_type() or nil,
		is_booting = multiplayer_session_manager and multiplayer_session_manager.is_booting_session and multiplayer_session_manager:is_booting_session() or nil,
		is_ready = multiplayer_session_manager and multiplayer_session_manager.is_ready and multiplayer_session_manager:is_ready() or nil,
	}, "solo_sessionbootdump", 3)

	mod:notify(mod:localize("msg_sessionbootdump_done"))
end)

mod:command("solo_loadingdump", "Dump the current loading host/client state", function ()
	local loading_manager = Managers.loading
	local loading_host = loading_manager and loading_manager._loading_host
	local loading_client = loading_manager and loading_manager._loading_client

	mod:dump({
		is_alive = loading_manager and loading_manager.is_alive and loading_manager:is_alive() or nil,
		is_host = loading_manager and loading_manager.is_host and loading_manager:is_host() or nil,
		is_client = loading_manager and loading_manager.is_client and loading_manager:is_client() or nil,
		loading_host = tostring(loading_host),
		loading_client = tostring(loading_client),
		loading_host_state = loading_host and loading_host._state or nil,
		loading_host_spawn_groups = loading_host and loading_host._spawn_groups or nil,
		loading_host_spawned_peers = loading_host and loading_host._spawned_peers or nil,
		loading_host_package_sync_enabled_peers = loading_host and loading_host._package_sync_enabled_peers or nil,
		loading_host_clients = loading_host and loading_host._clients or nil,
		loading_client_state = loading_client and loading_client:state() or nil,
		spawn_group_id = loading_manager and loading_manager.spawn_group_id and loading_manager:spawn_group_id() or nil,
	}, "solo_loadingdump", 3)

	mod:notify(mod:localize("msg_loadingdump_done"))
end)

mod:command("solo_lan_browser_refresh", "Create/refresh a persistent LAN browser for native validation", function ()
	local browser = ensure_validation_browser()

	if not browser then
		mod:notify(mod:localize("msg_browser_create_failed"))
		return
	end

	local ok, result = call_browser_method(browser, "refresh")

	mod:dump({
		action = "solo_lan_browser_refresh",
		refresh_ok = ok,
		refresh_result = ok and true or tostring(result),
		status = collect_browser_status(browser),
	}, "solo_lan_browser_refresh", 3)

	mod:notify(mod:localize(ok and "msg_browser_refreshed" or "msg_browser_refresh_failed"))
end)

mod:command("solo_lan_browser_status", "Dump the persistent LAN browser status", function ()
	mod:dump({
		action = "solo_lan_browser_status",
		status = collect_browser_status(mod._validation_lobby_browser),
		wan_client = tostring(mod._validation_browser_client),
	}, "solo_lan_browser_status", 3)

	if mod._validation_lobby_browser then
		mod:notify(mod:localize("msg_browser_status_dumped"))
	else
		mod:notify(mod:localize("msg_browser_missing"))
	end
end)

mod:command("solo_lan_browser_clear", "Destroy the persistent LAN browser used for native validation", function ()
	destroy_validation_browser()
	mod:notify(mod:localize("msg_browser_cleared"))
end)

mod:command("solo_native_host_test_enable", "Enable browser callback + synthetic discovery reply for next host launch", function ()
	local ok_host_test = set_native_flag("SoloPlayNativeHostPatch.host_test_enabled", true)

	mod:dump(native_feature_status(), "solo_native_host_test_enable", 3)
	mod:notify(mod:localize(ok_host_test and "msg_native_host_test_enabled" or "msg_native_flag_error"))
end)

mod:command("solo_native_host_test_disable", "Disable browser callback + synthetic discovery reply marker files", function ()
	local ok_host_test = set_native_flag("SoloPlayNativeHostPatch.host_test_enabled", false)
	local ok_browser = set_native_flag("SoloPlayNativeHostPatch.enable_browser_callback", false)
	local ok_reply = set_native_flag("SoloPlayNativeHostPatch.enable_discovery_reply", false)
	local ok_capture = set_native_flag("SoloPlayNativeHostPatch.enable_registration_capture", false)

	mod:dump(native_feature_status(), "solo_native_host_test_disable", 3)
	mod:notify(mod:localize(ok_host_test and ok_browser and ok_reply and ok_capture and "msg_native_host_test_disabled" or "msg_native_flag_error"))
end)

mod:command("solo_native_registration_capture_enable", "Enable only the native registration-capture hook for next launch", function ()
	local ok_capture = set_native_flag("SoloPlayNativeHostPatch.enable_registration_capture", true)
	mod:dump(native_feature_status(), "solo_native_registration_capture_enable", 3)
	mod:notify(mod:localize(ok_capture and "msg_native_registration_capture_enabled" or "msg_native_flag_error"))
end)

mod:command("solo_native_registration_capture_disable", "Disable the native registration-capture hook marker file", function ()
	local ok_capture = set_native_flag("SoloPlayNativeHostPatch.enable_registration_capture", false)
	mod:dump(native_feature_status(), "solo_native_registration_capture_disable", 3)
	mod:notify(mod:localize(ok_capture and "msg_native_registration_capture_disabled" or "msg_native_flag_error"))
end)

mod:command("solo_native_host_test_status", "Show native host discovery marker-file status", function ()
	mod:dump(native_feature_status(), "solo_native_host_test_status", 3)
	mod:notify(mod:localize("msg_native_host_test_status"))
end)

mod:command("solo_native_host_prime", "Force-create and refresh the LAN browser on the current client", function ()
	local browser = ensure_validation_browser()

	if not browser then
		mod:notify(mod:localize("msg_browser_create_failed"))
		return
	end

	call_browser_method(browser, "refresh")
	mod:dump({
		action = "solo_native_host_prime",
		native = native_feature_status(),
		status = collect_browser_status(browser),
	}, "solo_native_host_prime", 3)
	mod:notify(mod:localize("msg_browser_refreshed"))
end)
