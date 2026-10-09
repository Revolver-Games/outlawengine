/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <ScriptMotion/O3DE/ScriptMotionBus.h>
#include <ScriptMotion/O3DE/ScriptMotionConfiguration.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <Integration/ActorComponentBus.h>

namespace Wanted::ScriptMotion::O3DE
{
    class ScriptMotionPlayback;

    class ScriptMotionComponent final
        : public AZ::Component
        , private EMotionFX::Integration::ActorComponentNotificationBus::Handler
        , private ScriptMotionRequestBus::Handler
    {
    public:
        AZ_COMPONENT(ScriptMotionComponent, "{63365497-4B3A-433A-B889-651D941E9153}");
        ScriptMotionComponent();
        explicit ScriptMotionComponent(const ScriptMotionConfiguration& configuration);
        ~ScriptMotionComponent() override;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services);

        void Activate() override;
        void Deactivate() override;
        bool Play() override;
        void Stop() override;
        bool SetPlaybackSpeed(float speed) override;
        bool Seek(float timeSeconds) override;
        float GetDuration() const override;
        AZStd::string GetLastError() const override;

    private:
        void OnActorInstanceCreated(EMotionFX::ActorInstance* actorInstance) override;
        void OnActorInstanceDestroyed(EMotionFX::ActorInstance* actorInstance) override;

        ScriptMotionConfiguration m_configuration;
        AZStd::unique_ptr<ScriptMotionPlayback> m_playback;
    };
}
