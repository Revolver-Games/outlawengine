/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <ScriptMotion/O3DE/ScriptMotionComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Wanted::ScriptMotion::O3DE
{
    class ScriptMotionEditorComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private EMotionFX::Integration::ActorComponentNotificationBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(ScriptMotionEditorComponent, "{971CF3E0-6F41-4125-9339-979629BEA84D}");
        ScriptMotionEditorComponent();
        ~ScriptMotionEditorComponent() override;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        { ScriptMotionComponent::GetProvidedServices(services); }
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        { ScriptMotionComponent::GetRequiredServices(services); }
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        { ScriptMotionComponent::GetIncompatibleServices(services); }

        void Activate() override;
        void Deactivate() override;
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        void OnActorInstanceCreated(EMotionFX::ActorInstance* actorInstance) override;
        void OnActorInstanceDestroyed(EMotionFX::ActorInstance* actorInstance) override;
        AZ::u32 OnSettingsChanged();
        AZ::u32 OnReloadPreview();
        AZ::u32 OnSeekChanged();

        ScriptMotionConfiguration m_configuration;
        bool m_preview = false;
        float m_seekTime = 0.0f;
        AZStd::string m_status;
        AZStd::unique_ptr<ScriptMotionPlayback> m_playback;
    };
}
