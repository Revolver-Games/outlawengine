-- Copyright (c) 2026 Revolver Games contributors.
-- SPDX-License-Identifier: Apache-2.0 OR MIT
-- Attach to the mission controller and assign the entity containing CineScript.
local DeliveryCinematic = {
    Properties = {
        CinematicEntity = { default = EntityId(), description = "Mercy Crossing CineScript entity" },
    },
}

function DeliveryCinematic:OnActivate()
    self.started = false
    self.missionHandler = WantedNotificationBus.Connect(self)
end

function DeliveryCinematic:OnFirstErrandCompleted()
    if self.started then
        return
    end
    if not self.Properties.CinematicEntity:IsValid() then
        Debug.Warning("DeliveryCinematic requires a CineScript entity")
        return
    end
    self.started = CineScriptRequestBus.Event.Play(self.Properties.CinematicEntity)
    if not self.started then
        Debug.Warning(CineScriptRequestBus.Event.GetLastError(self.Properties.CinematicEntity))
    end
end

function DeliveryCinematic:OnDeactivate()
    if self.missionHandler then
        self.missionHandler:Disconnect()
        self.missionHandler = nil
    end
end

return DeliveryCinematic
