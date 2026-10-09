/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "ScriptMotionEditorComponent.h"
#include "ScriptMotionPlayback.h"
#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace Wanted::ScriptMotion::O3DE
{
    ScriptMotionEditorComponent::ScriptMotionEditorComponent()
        : m_playback(AZStd::make_unique<ScriptMotionPlayback>())
    {
    }

    ScriptMotionEditorComponent::~ScriptMotionEditorComponent() = default;

    void ScriptMotionEditorComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<ScriptMotionEditorComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(1)
                ->Field("Configuration", &ScriptMotionEditorComponent::m_configuration)
                ->Field("Preview", &ScriptMotionEditorComponent::m_preview);
            if (AZ::EditContext* edit = serialize->GetEditContext())
            {
                edit->Class<ScriptMotionEditorComponent>("ScriptMotion", "Preview and play a text-authored skeletal motion on an EMotionFX Actor")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Wanted")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionEditorComponent::m_configuration,
                        "Configuration", "Validated sources and playback configuration")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ScriptMotionEditorComponent::OnSettingsChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionEditorComponent::m_preview,
                        "Preview in editor", "Play through the native EMotionFX motion system")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ScriptMotionEditorComponent::OnSettingsChanged)
                    ->UIElement(AZ::Edit::UIHandlers::Button, "", "Reload edited JSON files and restart preview")
                    ->Attribute(AZ::Edit::Attributes::ButtonText, "Reload and preview")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ScriptMotionEditorComponent::OnReloadPreview)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionEditorComponent::m_paused,
                        "Pause preview", "Pause or resume without reloading the sources or resetting the playhead")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ScriptMotionEditorComponent::OnPauseChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionEditorComponent::m_seekTime,
                        "Seek seconds", "Seek without emitting events from the skipped interval; enable Pause preview to hold")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ScriptMotionEditorComponent::OnSeekChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionEditorComponent::m_status,
                        "Preview status", "Last load or validation result")
                    ->Attribute(AZ::Edit::Attributes::ReadOnly, true);
            }
        }
    }

    void ScriptMotionEditorComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        EMotionFX::Integration::ActorComponentNotificationBus::Handler::BusConnect(GetEntityId());
    }

    void ScriptMotionEditorComponent::Deactivate()
    {
        EMotionFX::Integration::ActorComponentNotificationBus::Handler::BusDisconnect();
        m_playback->SetActor(nullptr);
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void ScriptMotionEditorComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<ScriptMotionComponent>(m_configuration);
    }

    void ScriptMotionEditorComponent::OnActorInstanceCreated(EMotionFX::ActorInstance* actorInstance)
    {
        m_playback->SetActor(actorInstance);
        OnSettingsChanged();
    }

    void ScriptMotionEditorComponent::OnActorInstanceDestroyed([[maybe_unused]] EMotionFX::ActorInstance* actorInstance)
    {
        m_playback->SetActor(nullptr);
        m_status = "Waiting for Actor";
    }

    AZ::u32 ScriptMotionEditorComponent::OnSettingsChanged()
    {
        if (m_preview)
        {
            const bool success = m_playback->Play(m_configuration);
            if (success && m_paused)
            {
                m_playback->SetPlaybackSpeed(0.0f);
            }
            m_status = success ? (m_paused ? "Preview paused" : "Playing native motion") : m_playback->GetLastError();
            AZ_Warning("ScriptMotion", success, "%s", m_status.c_str());
        }
        else
        {
            m_playback->Stop();
            m_status = "Preview stopped";
        }
        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZ::u32 ScriptMotionEditorComponent::OnReloadPreview()
    {
        m_preview = true;
        return OnSettingsChanged();
    }

    AZ::u32 ScriptMotionEditorComponent::OnSeekChanged()
    {
        m_status = m_playback->Seek(m_seekTime) ? "Seek applied" : m_playback->GetLastError();
        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZ::u32 ScriptMotionEditorComponent::OnPauseChanged()
    {
        const bool success = m_playback->SetPlaybackSpeed(m_paused ? 0.0f : m_configuration.m_playbackSpeed);
        m_status = success ? (m_paused ? "Preview paused" : "Preview resumed") : m_playback->GetLastError();
        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }
}
