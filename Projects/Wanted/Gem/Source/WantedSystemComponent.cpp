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
            OnFirstErrandObjectiveChanged, OnFirstErrandCompleted);

        void OnFirstErrandObjectiveChanged(const AZStd::string& objective) override
        {
            Call(FN_OnFirstErrandObjectiveChanged, objective);
        }

        void OnFirstErrandCompleted() override
        {
            Call(FN_OnFirstErrandCompleted);
        }
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
                ->Event("GetFirstErrandObjective", &WantedRequestBus::Events::GetFirstErrandObjective);

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
}
