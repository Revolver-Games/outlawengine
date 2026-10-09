/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/O3DE/ScriptMotionComponent.h>
#include "ScriptMotionPlayback.h"
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Wanted::ScriptMotion::O3DE
{
    ScriptMotionComponent::ScriptMotionComponent()
        : m_playback(AZStd::make_unique<ScriptMotionPlayback>())
    {
    }

    ScriptMotionComponent::ScriptMotionComponent(const ScriptMotionConfiguration& configuration)
        : m_configuration(configuration)
        , m_playback(AZStd::make_unique<ScriptMotionPlayback>())
    {
    }

    ScriptMotionComponent::~ScriptMotionComponent() = default;

    void ScriptMotionComponent::Reflect(AZ::ReflectContext* context)
    {
        ScriptMotionConfiguration::Reflect(context);
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<ScriptMotionComponent, AZ::Component>()
                ->Version(1)
                ->Field("Configuration", &ScriptMotionComponent::m_configuration);
        }
        if (auto* behavior = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behavior->EBus<ScriptMotionRequestBus>("ScriptMotionRequestBus")
                ->Event("Play", &ScriptMotionRequests::Play)
                ->Event("Stop", &ScriptMotionRequests::Stop)
                ->Event("SetPlaybackSpeed", &ScriptMotionRequests::SetPlaybackSpeed)
                ->Event("Seek", &ScriptMotionRequests::Seek)
                ->Event("GetDuration", &ScriptMotionRequests::GetDuration)
                ->Event("GetLastError", &ScriptMotionRequests::GetLastError);
            behavior->Class<ScriptMotionComponent>()->RequestBus("ScriptMotionRequestBus");
        }
    }

    void ScriptMotionComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services)
    {
        services.push_back(AZ_CRC_CE("WantedScriptMotionService"));
    }

    void ScriptMotionComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services)
    {
        services.push_back(AZ_CRC_CE("EMotionFXActorService"));
    }

    void ScriptMotionComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services)
    {
        services.push_back(AZ_CRC_CE("WantedScriptMotionService"));
        services.push_back(AZ_CRC_CE("EMotionFXAnimGraphService"));
        services.push_back(AZ_CRC_CE("EMotionFXSimpleMotionService"));
        services.push_back(AZ_CRC_CE("NonUniformScaleService"));
    }

    void ScriptMotionComponent::Activate()
    {
        ScriptMotionRequestBus::Handler::BusConnect(GetEntityId());
        // O3DE's notification connection policy immediately reports an already-created actor.
        EMotionFX::Integration::ActorComponentNotificationBus::Handler::BusConnect(GetEntityId());
    }

    void ScriptMotionComponent::Deactivate()
    {
        EMotionFX::Integration::ActorComponentNotificationBus::Handler::BusDisconnect();
        ScriptMotionRequestBus::Handler::BusDisconnect();
        m_playback->SetActor(nullptr);
    }

    void ScriptMotionComponent::OnActorInstanceCreated(EMotionFX::ActorInstance* actorInstance)
    {
        m_playback->SetActor(actorInstance);
        if (m_configuration.m_playOnActivation)
        {
            Play();
        }
    }

    void ScriptMotionComponent::OnActorInstanceDestroyed([[maybe_unused]] EMotionFX::ActorInstance* actorInstance)
    {
        // This notification runs before the actor is destroyed; release our motion instance first.
        m_playback->SetActor(nullptr);
    }

    bool ScriptMotionComponent::Play()
    {
        const bool success = m_playback->Play(m_configuration);
        AZ_Warning("ScriptMotion", success, "%s", m_playback->GetLastError().c_str());
        return success;
    }

    void ScriptMotionComponent::Stop()
    {
        m_playback->Stop();
    }

    bool ScriptMotionComponent::SetPlaybackSpeed(float speed)
    {
        if (!m_playback->SetPlaybackSpeed(speed))
        {
            return false;
        }
        m_configuration.m_playbackSpeed = speed;
        return true;
    }

    bool ScriptMotionComponent::Seek(float timeSeconds)
    {
        return m_playback->Seek(timeSeconds);
    }

    float ScriptMotionComponent::GetDuration() const
    {
        return m_playback->GetDuration();
    }

    AZStd::string ScriptMotionComponent::GetLastError() const
    {
        return m_playback->GetLastError();
    }
}
