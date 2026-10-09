/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>
#include <AzCore/IO/FileIO.h>

#include "WantedSystemComponent.h"

#include <Wanted/WantedTypeIds.h>

namespace Wanted
{
    class WantedNotificationBusHandler
        : public WantedNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(WantedNotificationBusHandler,
            "{579AD8DD-CAFB-4D2D-9AF6-D2C4653DE592}", AZ::SystemAllocator,
            OnFirstErrandObjectiveChanged, OnFirstErrandCompleted, OnNarrativeChanged, OnNarrativeCompleted);

        void OnFirstErrandObjectiveChanged(const AZStd::string& objective) override
        {
            Call(FN_OnFirstErrandObjectiveChanged, objective);
        }

        void OnFirstErrandCompleted() override
        {
            Call(FN_OnFirstErrandCompleted);
        }

        void OnNarrativeChanged(const AZStd::string& objective, const AZStd::string& dialogue) override
        { Call(FN_OnNarrativeChanged, objective, dialogue); }
        void OnNarrativeCompleted(const AZStd::string& cutscene, int credits) override
        { Call(FN_OnNarrativeCompleted, cutscene, credits); }
    };

    AZ_COMPONENT_IMPL(WantedSystemComponent, "WantedSystemComponent",
        WantedSystemComponentTypeId);

    void WantedSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<WantedSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<WantedRequestBus>("WantedRequestBus")
                ->Attribute(AZ::Script::Attributes::Category, "Wanted")
                ->Event("StartFirstErrand", &WantedRequestBus::Events::StartFirstErrand)
                ->Event("ResetFirstErrand", &WantedRequestBus::Events::ResetFirstErrand)
                ->Event("InteractWithMissionTarget", &WantedRequestBus::Events::InteractWithMissionTarget)
                ->Event("GetFirstErrandStage", &WantedRequestBus::Events::GetFirstErrandStage)
                ->Event("GetFirstErrandObjective", &WantedRequestBus::Events::GetFirstErrandObjective)
                ->Event("LoadNarrative", &WantedRequests::LoadNarrative)
                ->Event("StartNarrative", &WantedRequests::StartNarrative)
                ->Event("SendNarrativeEvent", &WantedRequests::SendNarrativeEvent)
                ->Event("BeginNarrativeDialogue", &WantedRequests::BeginNarrativeDialogue)
                ->Event("ChooseNarrativeDialogue", &WantedRequests::ChooseNarrativeDialogue)
                ->Event("CloseNarrativeDialogue", &WantedRequests::CloseNarrativeDialogue)
                ->Event("FailNarrative", &WantedRequests::FailNarrative)
                ->Event("GetNarrativeObjective", &WantedRequests::GetNarrativeObjective)
                ->Event("GetNarrativeDialogue", &WantedRequests::GetNarrativeDialogue)
                ->Event("GetNarrativeError", &WantedRequests::GetNarrativeError)
                ->Event("HasNarrativeFlag", &WantedRequests::HasNarrativeFlag)
                ->Event("GetNarrativeReputation", &WantedRequests::GetNarrativeReputation)
                ->Event("GetNarrativeCredits", &WantedRequests::GetNarrativeCredits)
                ->Event("SaveNarrative", &WantedRequests::SaveNarrative)
                ->Event("RestoreNarrative", &WantedRequests::RestoreNarrative);

            behaviorContext->EBus<WantedNotificationBus>("WantedNotificationBus")
                ->Attribute(AZ::Script::Attributes::Category, "Wanted")
                ->Handler<WantedNotificationBusHandler>();
        }
    }

    void WantedSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("WantedService"));
    }

    void WantedSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("WantedService"));
    }

    void WantedSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void WantedSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    WantedSystemComponent::WantedSystemComponent()
    {
        if (WantedInterface::Get() == nullptr)
        {
            WantedInterface::Register(this);
        }
    }

    WantedSystemComponent::~WantedSystemComponent()
    {
        if (WantedInterface::Get() == this)
        {
            WantedInterface::Unregister(this);
        }
    }

    void WantedSystemComponent::Init()
    {
    }

    void WantedSystemComponent::Activate()
    {
        WantedRequestBus::Handler::BusConnect();
    }

    void WantedSystemComponent::Deactivate()
    {
        WantedRequestBus::Handler::BusDisconnect();
    }

    bool WantedSystemComponent::StartFirstErrand()
    {
        if (!m_firstErrand.Start())
        {
            return false;
        }
        BroadcastObjective();
        return true;
    }

    void WantedSystemComponent::ResetFirstErrand()
    {
        m_firstErrand.Reset();
        BroadcastObjective();
    }

    bool WantedSystemComponent::InteractWithMissionTarget(const AZStd::string& target)
    {
        const auto result = m_firstErrand.Interact(std::string_view(target.data(), target.size()));
        if (result == FirstErrand::Transition::Ignored)
        {
            return false;
        }
        BroadcastObjective();
        if (result == FirstErrand::Transition::Completed)
        {
            WantedNotificationBus::Broadcast(&WantedNotificationBus::Events::OnFirstErrandCompleted);
        }
        return true;
    }

    AZ::u32 WantedSystemComponent::GetFirstErrandStage() const
    {
        return static_cast<AZ::u32>(m_firstErrand.GetStage());
    }

    AZStd::string WantedSystemComponent::GetFirstErrandObjective() const
    {
        const auto objective = m_firstErrand.GetObjective();
        return AZStd::string(objective.data(), objective.size());
    }

    void WantedSystemComponent::BroadcastObjective()
    {
        WantedNotificationBus::Broadcast(
            &WantedNotificationBus::Events::OnFirstErrandObjectiveChanged,
            GetFirstErrandObjective());
    }

    bool WantedSystemComponent::LoadNarrative(const AZStd::string& path)
    {
        if (m_narrativeDispatch) { return false; }
        if (path.empty() || !AZ::IO::FileIOBase::GetInstance()) { m_narrativeError = "Narrative path or FileIO unavailable"; return false; }
        AZ::IO::FileIOStream input(path.c_str(), AZ::IO::OpenMode::ModeRead);
        if (!input.IsOpen() || input.GetLength() == 0 || input.GetLength() > 1024 * 1024)
        { m_narrativeError = "Narrative source must be readable and at most 1 MiB"; return false; }
        std::string text(static_cast<std::size_t>(input.GetLength()), '\0');
        if (input.Read(text.size(), text.data()) != text.size()) { m_narrativeError = "Narrative source read failed"; return false; }
        const auto error = m_narrative.Load(text);
        m_narrativeError.assign(error.data(), error.size());
        if (!error.empty()) { return false; }
        BroadcastNarrative();
        return true;
    }
    bool WantedSystemComponent::FinishNarrativeCommand(Narrative::Result<Narrative::Change> change)
    {
        m_narrativeError.assign(change.error.data(), change.error.size());
        if (!change || !change.value.accepted) { return false; }
        // Read queries are allowed in callbacks; further mutation must wait until the next tick.
        m_narrativeDispatch = true;
        WantedNotificationBus::Broadcast(&WantedNotifications::OnNarrativeChanged, GetNarrativeObjective(), GetNarrativeDialogue());
        if (change.value.completed)
        {
            const AZStd::string scene(change.value.cutscene.data(), change.value.cutscene.size());
            WantedNotificationBus::Broadcast(&WantedNotifications::OnNarrativeCompleted, scene, change.value.creditsAdded);
        }
        m_narrativeDispatch = false;
        return true;
    }
    void WantedSystemComponent::BroadcastNarrative()
    {
        m_narrativeDispatch = true;
        WantedNotificationBus::Broadcast(&WantedNotifications::OnNarrativeChanged, GetNarrativeObjective(), GetNarrativeDialogue());
        m_narrativeDispatch = false;
    }
    bool WantedSystemComponent::StartNarrative()
    { return !m_narrativeDispatch && FinishNarrativeCommand(m_narrative.Start()); }
    bool WantedSystemComponent::SendNarrativeEvent(const AZStd::string& kind, const AZStd::string& target)
    { return !m_narrativeDispatch && FinishNarrativeCommand(m_narrative.Event(std::string_view(kind.data(), kind.size()), std::string_view(target.data(), target.size()))); }
    bool WantedSystemComponent::BeginNarrativeDialogue(const AZStd::string& node)
    { return !m_narrativeDispatch && FinishNarrativeCommand(m_narrative.BeginDialogue(std::string_view(node.data(), node.size()))); }
    bool WantedSystemComponent::ChooseNarrativeDialogue(const AZStd::string& choice)
    { return !m_narrativeDispatch && FinishNarrativeCommand(m_narrative.Choose(std::string_view(choice.data(), choice.size()))); }
    bool WantedSystemComponent::CloseNarrativeDialogue()
    { return !m_narrativeDispatch && FinishNarrativeCommand(m_narrative.CloseDialogue()); }
    bool WantedSystemComponent::FailNarrative()
    { return !m_narrativeDispatch && FinishNarrativeCommand(m_narrative.Fail()); }
    AZStd::string WantedSystemComponent::GetNarrativeObjective() const
    { const auto text = m_narrative.ObjectiveText(); return {text.data(), text.size()}; }
    AZStd::string WantedSystemComponent::GetNarrativeDialogue() const
    { const auto text = m_narrative.DialogueJson(); return {text.data(), text.size()}; }
    bool WantedSystemComponent::HasNarrativeFlag(const AZStd::string& flag) const
    { return m_narrative.Story().flags.contains(std::string(flag.data(), flag.size())); }
    int WantedSystemComponent::GetNarrativeReputation(const AZStd::string& character) const
    {
        const auto found = m_narrative.Story().reputation.find(std::string(character.data(), character.size()));
        return found == m_narrative.Story().reputation.end() ? 0 : found->second;
    }
    AZStd::string WantedSystemComponent::SaveNarrative() const
    { const auto text = m_narrative.Save(); return {text.data(), text.size()}; }
    bool WantedSystemComponent::RestoreNarrative(const AZStd::string& json)
    {
        if (m_narrativeDispatch) { return false; }
        const auto error = m_narrative.Restore(std::string_view(json.data(), json.size()));
        m_narrativeError.assign(error.data(), error.size());
        if (!error.empty()) { return false; }
        BroadcastNarrative(); // Rebuild UI only: no completion/cinematic/reward notifications on replay.
        return true;
    }
}
