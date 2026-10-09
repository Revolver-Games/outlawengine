-- Copyright (c) 2026 Revolver Games contributors.
-- SPDX-License-Identifier: Apache-2.0 OR MIT
-- Optional bridge when the separately reviewed CineScript Gem is present.
local NarrativeCinematic = {
    Properties = {
        CinematicEntity = { default = EntityId() },
        AllowedScene = { default = "mercy_delivery" },
    },
}
function NarrativeCinematic:OnActivate()
    self.handler = WantedNotificationBus.Connect(self)
end
function NarrativeCinematic:OnNarrativeCompleted(scene, creditsAdded)
    -- Credits already belong to the saved narrative state. Never grant them again here.
    if scene ~= self.Properties.AllowedScene or not self.Properties.CinematicEntity:IsValid() then
        return
    end
    if CineScriptRequestBus == nil then
        Debug.Log("Narrative completed; CineScript is not enabled in this build.")
        return
    end
    if not CineScriptRequestBus.Event.Play(self.Properties.CinematicEntity) then
        Debug.Warning(CineScriptRequestBus.Event.GetLastError(self.Properties.CinematicEntity))
    end
end
function NarrativeCinematic:OnDeactivate()
    if self.handler then
        self.handler:Disconnect()
        self.handler = nil
    end
end
return NarrativeCinematic
