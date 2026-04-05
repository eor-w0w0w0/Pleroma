local ExperimentalConnectionHost = require("scripts/mods/SoloPlay/experimental/connection_host")
local SessionBootBase = require("scripts/multiplayer/session_boot_base")

local STATES = table.enum("idle", "ready", "failed")

local ExperimentalPlayerHostedSessionBoot = class("ExperimentalPlayerHostedSessionBoot", "SessionBootBase")

ExperimentalPlayerHostedSessionBoot.init = function (self, event_object, engine_lobby, host_type, tick_rate, max_members, optional_session_id)
    ExperimentalPlayerHostedSessionBoot.super.init(self, STATES, event_object)

    self:_set_state(STATES.idle)

    if not engine_lobby then
        event_object:failed_to_boot(true, "game", "missing_native_host_lobby")
        self:_set_state(STATES.failed)

        return
    end

    local connection_manager = Managers.connection
    local event_delegate = connection_manager:network_event_delegate()
    local approve_channel_delegate = connection_manager:approve_channel_delegate()

    self._connection_host = ExperimentalConnectionHost:new(
        event_delegate,
        approve_channel_delegate,
        engine_lobby,
        host_type,
        tick_rate,
        max_members,
        optional_session_id
    )

    self:_set_state(STATES.ready)
end

ExperimentalPlayerHostedSessionBoot.result = function (self)
    local connection_host = self._connection_host

    self._connection_host = nil

    return connection_host
end

ExperimentalPlayerHostedSessionBoot.destroy = function (self)
    if self._connection_host then
        self._connection_host:delete()
        self._connection_host = nil
    end
end

implements(ExperimentalPlayerHostedSessionBoot, SessionBootBase.INTERFACE)

return ExperimentalPlayerHostedSessionBoot
