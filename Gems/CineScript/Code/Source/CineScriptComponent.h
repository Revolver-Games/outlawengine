/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#pragma once
#include <CineScript/Timeline.h>
#include <CineScript/CineScriptBus.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/vector.h>
#include <map>

namespace Wanted::CineScript
{
    struct Binding
    {
        AZ_TYPE_INFO(Binding, "{D077F990-FC5D-4408-B1E3-8235197C03FC}");
        AZ_CLASS_ALLOCATOR(Binding, AZ::SystemAllocator);
        AZStd::string m_name;
        AZ::EntityId m_entity;
        AZStd::string m_motionAlias; // Whitelisted alias of the entity's configured ScriptMotion clip.
        static void Reflect(AZ::ReflectContext* context);
    };
    class CineScriptComponent final : public AZ::Component, private AZ::TickBus::Handler, private RequestBus::Handler
    {
    public:
        AZ_COMPONENT(CineScriptComponent, "{CA7B2462-D19D-4B05-A45D-61B7700D7401}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        void Activate() override;
        void Deactivate() override;
        bool Play() override;
        void Cancel() override;
        bool Seek(float seconds) override;
        bool Skip() override;
        void SetPaused(bool paused) override;
        bool IsPlaying() const override { return m_timeline.IsPlaying(); }
        AZStd::string GetLastError() const override { return m_error; }
        AZStd::string SaveCheckpoint() const override;
        bool RestoreCheckpoint(const AZStd::string& json) override;
    private:
        void OnTick(float dt, AZ::ScriptTimePoint) override;
        int GetTickOrder() override { return AZ::TICK_ANIMATION + 1; }
        const Binding* Find(std::string_view name) const;
        bool Load(Timeline& timeline);
        bool ValidateBindings(const Scene& scene);
        bool Acquire();
        void Release(bool restoreActors);
        bool Apply(const Frame& frame, bool skipped = false);
        bool Fail(const std::string& error);
        AZStd::string m_sourcePath;
        AZStd::vector<Binding> m_bindings;
        AZStd::vector<AZStd::string> m_trueFlags;
        bool m_playOnActivation = false;
        bool m_debugSubtitles = true;
        bool m_pendingPlay = false;
        bool m_dispatching = false;
        bool m_owned = false;
        Timeline m_timeline;
        AZ::EntityId m_previousCamera;
        std::map<std::string, AZ::Transform> m_originalTransforms;
        std::map<std::string, std::string> m_activeAnimations;
        std::map<std::string, float> m_originalSpeeds;
        AZStd::string m_error;
        std::string m_subtitle;
    };
}
