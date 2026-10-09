-- Copyright (c) Contributors to the Wanted Engine Project.
-- SPDX-License-Identifier: Apache-2.0 OR MIT
-- Source integration only: bind the properties and Interact input in the editor.
-- O3DE editor execution has not yet been verified. This is single-player logic.
local FirstErrandInteraction =
{
    Properties =
    {
        Player = { default = EntityId() },
        Ada = { default = EntityId() },
        Satchel = { default = EntityId() },
        InteractionDistance = { default = 2.5, min = 0.1 },
        InputEvent = { default = "Interact" },
    },
}

function FirstErrandInteraction:OnActivate()
    self.notifications = WantedNotificationBus.Connect(self)
    self.input = InputEventNotificationBus.Connect(self,
        InputEventNotificationId(self.Properties.InputEvent))
    -- An explicitly fresh prototype session. Save/checkpoint support is not implemented.
    WantedRequestBus.Broadcast.ResetFirstErrand()
    WantedRequestBus.Broadcast.StartFirstErrand()
end

function FirstErrandInteraction:OnPressed(value)
    local stage = WantedRequestBus.Broadcast.GetFirstErrandStage()
    local target
    local targetName
    if stage == 1 or stage == 3 then
        target = self.Properties.Ada
        targetName = "ada_mercer"
    elseif stage == 2 then
        target = self.Properties.Satchel
        targetName = "mail_satchel"
    else
        return
    end

    if not self.Properties.Player:IsValid() or not target:IsValid() then
        Debug.Log("WANTED: assign Player, Ada and Satchel entity references in the editor.")
        return
    end
    local playerPosition = TransformBus.Event.GetWorldTranslation(self.Properties.Player)
    local targetPosition = TransformBus.Event.GetWorldTranslation(target)
    if playerPosition == nil or targetPosition == nil then
        Debug.Log("WANTED: interaction entities require Transform components.")
        return
    end
    if (playerPosition - targetPosition):GetLength() <= self.Properties.InteractionDistance then
        WantedRequestBus.Broadcast.InteractWithMissionTarget(targetName)
    end
end

function FirstErrandInteraction:OnFirstErrandObjectiveChanged(objective)
    Debug.Log("WANTED objective: " .. objective)
end

function FirstErrandInteraction:OnFirstErrandCompleted()
    Debug.Log("Ada Mercer: You brought back more than the mail. Take a look at that postmark.")
end

function FirstErrandInteraction:OnDeactivate()
    if self.input then
        self.input:Disconnect()
    end
    if self.notifications then
        self.notifications:Disconnect()
    end
end

return FirstErrandInteraction
