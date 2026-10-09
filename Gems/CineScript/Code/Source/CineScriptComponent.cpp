/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include "CineScriptComponent.h"
#include <ScriptMotion/O3DE/ScriptMotionBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/CameraBus.h>
#include <DebugDraw/DebugDrawBus.h>
#include <unordered_set>
#include <cmath>

namespace Wanted::CineScript
{
    namespace
    {
        using MotionBus = ScriptMotion::O3DE::ScriptMotionRequestBus;
        AZ::EntityId activeDirector; // Main-thread, one camera/control owner across scene entities.
        AZStd::string Native(const std::string& s) { return {s.data(), s.size()}; }
        // Commands triggered by notification callbacks must be deferred by the caller.
        struct DispatchGuard { bool& flag; explicit DispatchGuard(bool& f) : flag(f) { flag = true; } ~DispatchGuard() { flag = false; } };
        class NotificationHandler : public NotificationBus::Handler, public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(NotificationHandler, "{C9451D71-D61D-4D95-894C-85A2129FC13E}", AZ::SystemAllocator,
                OnControlLockChanged, OnSubtitle, OnCue, OnCompleted, OnError);
            void OnControlLockChanged(bool locked) override { Call(FN_OnControlLockChanged, locked); }
            void OnSubtitle(const AZStd::string& speaker, const AZStd::string& text) override { Call(FN_OnSubtitle, speaker, text); }
            void OnCue(const AZStd::string& kind, const AZStd::string& alias) override { Call(FN_OnCue, kind, alias); }
            void OnCompleted(bool skipped) override { Call(FN_OnCompleted, skipped); }
            void OnError(const AZStd::string& error) override { Call(FN_OnError, error); }
        };
    }
    void Binding::Reflect(AZ::ReflectContext* context)
    {
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(context))
        {
            s->Class<Binding>()->Version(1)->Field("Name", &Binding::m_name)->Field("Entity", &Binding::m_entity)
                ->Field("MotionAlias", &Binding::m_motionAlias);
            if (auto* e = s->GetEditContext())
            {
                e->Class<Binding>("Scene binding", "Bind an authored name to a game entity")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Binding::m_name, "Name", "Authored target identifier")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Binding::m_entity, "Entity", "Actor or Camera entity")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Binding::m_motionAlias, "Motion alias", "Alias of the entity's configured ScriptMotion clip");
            }
        }
    }
    void CineScriptComponent::Reflect(AZ::ReflectContext* context)
    {
        Binding::Reflect(context);
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(context))
        {
            s->Class<CineScriptComponent, AZ::Component>()->Version(1)
                ->Field("Source", &CineScriptComponent::m_sourcePath)->Field("Bindings", &CineScriptComponent::m_bindings)
                ->Field("TrueFlags", &CineScriptComponent::m_trueFlags)->Field("PlayOnActivation", &CineScriptComponent::m_playOnActivation)
                ->Field("DebugSubtitles", &CineScriptComponent::m_debugSubtitles);
            if (auto* e = s->GetEditContext())
            {
                e->Class<CineScriptComponent>("CineScript", "Text-authored cinematic playback")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Outlaw Engine")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CineScriptComponent::m_sourcePath, "Scene JSON", "FileIO source path, e.g. @projectroot@/Assets/CineScript/mercy_delivery.cinescript.json")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CineScriptComponent::m_bindings, "Bindings", "Unique actor/camera names and entities")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CineScriptComponent::m_trueFlags, "True flags", "Conditions captured when playback starts")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CineScriptComponent::m_playOnActivation, "Play on activation", "Attempt play on first game tick")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CineScriptComponent::m_debugSubtitles, "Debug subtitles", "Draw temporary development subtitles through DebugDraw");
            }
        }
        if (auto* b = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            b->EBus<RequestBus>("CineScriptRequestBus")
                ->Event("Play", &Requests::Play)->Event("Cancel", &Requests::Cancel)->Event("Seek", &Requests::Seek)
                ->Event("Skip", &Requests::Skip)->Event("SetPaused", &Requests::SetPaused)->Event("IsPlaying", &Requests::IsPlaying)
                ->Event("GetLastError", &Requests::GetLastError)->Event("SaveCheckpoint", &Requests::SaveCheckpoint)
                ->Event("RestoreCheckpoint", &Requests::RestoreCheckpoint);
            b->EBus<NotificationBus>("CineScriptNotificationBus")->Handler<NotificationHandler>();
        }
    }
    void CineScriptComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("CineScriptService")); }
    void CineScriptComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("CineScriptService")); }
    void CineScriptComponent::Activate()
    {
        RequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        m_pendingPlay = m_playOnActivation;
    }
    void CineScriptComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        RequestBus::Handler::BusDisconnect();
        DispatchGuard guard(m_dispatching);
        m_timeline.Stop(); Release(true); m_pendingPlay = false;
    }
    const Binding* CineScriptComponent::Find(std::string_view name) const
    {
        for (const auto& b : m_bindings) { if (name == std::string_view(b.m_name.data(), b.m_name.size())) { return &b; } }
        return nullptr;
    }
    bool CineScriptComponent::Fail(const std::string& error)
    {
        m_error = Native(error);
        AZ_Warning("CineScript", false, "%s", m_error.c_str());
        NotificationBus::Event(GetEntityId(), &Notifications::OnError, m_error);
        return false;
    }
    bool CineScriptComponent::ValidateBindings(const Scene& scene)
    {
        if (m_bindings.size() != scene.targets.size()) { return Fail("Bind every scene target exactly once"); }
        std::unordered_set<std::string> names;
        for (std::size_t i = 0; i < m_bindings.size(); ++i)
        {
            const auto& binding = m_bindings[i];
            if (!binding.m_entity.IsValid() || !names.insert(binding.m_name.c_str()).second) { return Fail("Invalid or duplicate binding"); }
            for (std::size_t j = 0; j < i; ++j)
            { if (binding.m_entity == m_bindings[j].m_entity) { return Fail("Each target requires a distinct entity"); } }
        }
        for (const auto& target : scene.targets)
        {
            const auto* b = Find(target.id);
            if (!b || !AZ::TransformBus::FindFirstHandler(b->m_entity)) { return Fail("Missing active Transform: " + target.id); }
            if (target.kind == TargetKind::Camera && !Camera::CameraRequestBus::FindFirstHandler(b->m_entity))
            { return Fail("Missing active Camera: " + target.id); }
        }
        for (const auto& event : scene.events)
        {
            if (event.kind != EventKind::Animation) { continue; }
            const auto* b = Find(event.target);
            if (!b || event.value != b->m_motionAlias.c_str() || !MotionBus::FindFirstHandler(b->m_entity))
            { return Fail("Unbound ScriptMotion animation alias: " + event.value); }
        }
        return true;
    }
    bool CineScriptComponent::Load(Timeline& timeline)
    {
        if (m_sourcePath.empty() || !AZ::IO::FileIOBase::GetInstance()) { return Fail("Scene path or FileIO unavailable"); }
        AZ::IO::FileIOStream input(m_sourcePath.c_str(), AZ::IO::OpenMode::ModeRead);
        if (!input.IsOpen() || input.GetLength() == 0 || input.GetLength() > 1024 * 1024) { return Fail("Scene must be a readable JSON file of at most 1 MiB"); }
        std::string text(static_cast<std::size_t>(input.GetLength()), '\0');
        if (input.Read(text.size(), text.data()) != text.size()) { return Fail("Scene read failed"); }
        auto scene = ParseScene(text);
        if (!scene) { return Fail(scene.error); }
        if (!ValidateBindings(scene.value)) { return false; }
        std::vector<std::string> flags;
        for (const auto& flag : m_trueFlags) { flags.emplace_back(flag.c_str()); }
        if (auto error = timeline.Load(std::move(scene.value), std::move(flags)); !error.empty()) { return Fail(error); }
        return true;
    }
    bool CineScriptComponent::Acquire()
    {
        if (activeDirector.IsValid()) { return Fail("Another CineScript scene owns the camera"); }
        m_previousCamera = {};
        Camera::CameraSystemRequestBus::BroadcastResult(m_previousCamera, &Camera::CameraSystemRequests::GetActiveCamera);
        // Require a camera we can restore; otherwise a cut could strand the viewport.
        if (!m_previousCamera.IsValid() || !Camera::CameraRequestBus::FindFirstHandler(m_previousCamera))
        { return Fail("An active gameplay Camera is required before a cutscene starts"); }
        m_originalTransforms.clear();
        for (const auto& target : m_timeline.GetScene().targets)
        {
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(transform, Find(target.id)->m_entity, &AZ::TransformInterface::GetWorldTM);
            m_originalTransforms.emplace(target.id, transform);
        }
        m_owned = true;
        activeDirector = GetEntityId();
        NotificationBus::Event(GetEntityId(), &Notifications::OnControlLockChanged, true);
        return true;
    }
    void CineScriptComponent::Release(bool restoreActors)
    {
        // Restore configured playback speed while instances still exist, then stop them.
        for (const auto& [target, speed] : m_originalSpeeds)
        { if (const auto* b = Find(target)) { MotionBus::Event(b->m_entity, &MotionBus::Events::SetPlaybackSpeed, speed); } }
        m_originalSpeeds.clear();
        for (const auto& [target, alias] : m_activeAnimations)
        { (void)alias; if (const auto* b = Find(target)) { MotionBus::Event(b->m_entity, &MotionBus::Events::Stop); } }
        m_activeAnimations.clear();
        if (!m_owned) { return; }
        for (const auto& target : m_timeline.GetScene().targets)
        {
            if (restoreActors || target.kind == TargetKind::Camera)
            {
                const auto original = m_originalTransforms.find(target.id);
                if (const auto* b = Find(target.id); b && original != m_originalTransforms.end())
                { AZ::TransformBus::Event(b->m_entity, &AZ::TransformInterface::SetWorldTM, original->second); }
            }
        }
        Camera::CameraRequestBus::Event(m_previousCamera, &Camera::CameraComponentRequests::MakeActiveView);
        m_owned = false; m_originalTransforms.clear(); m_subtitle.clear();
        if (activeDirector == GetEntityId()) { activeDirector = {}; }
        NotificationBus::Event(GetEntityId(), &Notifications::OnSubtitle, AZStd::string{}, AZStd::string{});
        NotificationBus::Event(GetEntityId(), &Notifications::OnControlLockChanged, false);
    }
    bool CineScriptComponent::Apply(const Frame& frame, bool skipped)
    {
        if (!ValidateBindings(m_timeline.GetScene())) { m_timeline.Stop(); Release(true); return false; }
        for (const auto& p : frame.placements)
        {
            const auto& r = p.transform.rotation; const auto& v = p.transform.translation;
            const auto transform = AZ::Transform::CreateFromQuaternionAndTranslation(
                AZ::Quaternion(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z), static_cast<float>(r.w)),
                AZ::Vector3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)));
            AZ::TransformBus::Event(Find(p.target)->m_entity, &AZ::TransformInterface::SetWorldTM, transform);
        }
        if (const auto* camera = Find(frame.camera)) { Camera::CameraRequestBus::Event(camera->m_entity, &Camera::CameraComponentRequests::MakeActiveView); }
        std::map<std::string, std::string> active;
        for (const auto& animation : frame.animations)
        {
            const auto entity = Find(animation.target)->m_entity;
            bool ok = true;
            const auto old = m_activeAnimations.find(animation.target);
            if (old == m_activeAnimations.end() || old->second != animation.clip)
            {
                if (!m_originalSpeeds.contains(animation.target))
                {
                    float speed = 1.0f;
                    MotionBus::EventResult(speed, entity, &MotionBus::Events::GetPlaybackSpeed);
                    m_originalSpeeds[animation.target] = speed;
                }
                ok = false; MotionBus::EventResult(ok, entity, &MotionBus::Events::Play);
                if (ok) { m_activeAnimations[animation.target] = animation.clip; }
            }
            bool paused = false, sought = false;
            if (ok) { MotionBus::EventResult(paused, entity, &MotionBus::Events::SetPlaybackSpeed, 0.0f); }
            if (paused) { MotionBus::EventResult(sought, entity, &MotionBus::Events::Seek, static_cast<float>(animation.time)); }
            if (!sought) { m_timeline.Stop(); Release(true); return Fail("ScriptMotion playback/seek failed: " + animation.target); }
            active[animation.target] = animation.clip;
        }
        for (const auto& [target, alias] : m_activeAnimations)
        {
            (void)alias;
            if (!active.contains(target))
            {
                const auto entity = Find(target)->m_entity;
                if (const auto speed = m_originalSpeeds.find(target); speed != m_originalSpeeds.end())
                { MotionBus::Event(entity, &MotionBus::Events::SetPlaybackSpeed, speed->second); m_originalSpeeds.erase(speed); }
                MotionBus::Event(entity, &MotionBus::Events::Stop);
            }
        }
        m_activeAnimations = std::move(active);
        if (m_subtitle != frame.subtitle)
        {
            m_subtitle = frame.subtitle;
            NotificationBus::Event(GetEntityId(), &Notifications::OnSubtitle, Native(frame.speaker), Native(frame.subtitle));
        }
        if (m_debugSubtitles && !frame.subtitle.empty())
        {
            DebugDraw::DebugDrawRequestBus::Broadcast(&DebugDraw::DebugDrawRequests::DrawTextOnScreen,
                Native(frame.speaker + ": " + frame.subtitle), AZ::Color::CreateOne(), 0.0f);
        }
        for (const auto& cue : frame.cues)
        {
            const AZStd::string type = cue.kind == EventKind::Signal ? "signal" : (cue.kind == EventKind::Audio ? "audio" : "music");
            NotificationBus::Event(GetEntityId(), &Notifications::OnCue, type, Native(cue.value));
        }
        if (!frame.gameplayLocked) { Release(false); }
        if (frame.completed) { NotificationBus::Event(GetEntityId(), &Notifications::OnCompleted, skipped); }
        return true;
    }
    bool CineScriptComponent::Play()
    {
        if (m_dispatching || m_timeline.IsPlaying()) { return false; }
        DispatchGuard guard(m_dispatching);
        Timeline loaded;
        if (!Load(loaded)) { return false; }
        m_timeline = std::move(loaded);
        if (!Acquire()) { return false; }
        auto frame = m_timeline.Start();
        if (!frame) { Release(true); return Fail(frame.error); }
        m_error.clear();
        return Apply(frame.value);
    }
    void CineScriptComponent::Cancel()
    {
        if (m_dispatching) { return; }
        DispatchGuard guard(m_dispatching);
        m_timeline.Stop(); Release(true);
    }
    bool CineScriptComponent::Seek(float seconds)
    {
        if (m_dispatching || !m_owned) { return false; }
        DispatchGuard guard(m_dispatching);
        auto frame = m_timeline.Seek(seconds);
        return frame ? Apply(frame.value) : Fail(frame.error);
    }
    bool CineScriptComponent::Skip()
    {
        if (m_dispatching) { return false; }
        DispatchGuard guard(m_dispatching);
        auto frame = m_timeline.Skip();
        return frame ? Apply(frame.value, true) : Fail(frame.error);
    }
    void CineScriptComponent::SetPaused(bool paused) { if (!m_dispatching) { m_timeline.Pause(paused); } }
    AZStd::string CineScriptComponent::SaveCheckpoint() const { return Native(m_timeline.SaveCheckpoint()); }
    bool CineScriptComponent::RestoreCheckpoint(const AZStd::string& json)
    {
        if (m_dispatching || m_owned) { return false; }
        DispatchGuard guard(m_dispatching);
        Timeline loaded;
        if (!Load(loaded)) { return false; }
        if (auto error = loaded.RestoreCheckpoint(std::string_view(json.data(), json.size())); !error.empty()) { return Fail(error); }
        m_timeline = std::move(loaded);
        if (!m_timeline.IsPlaying()) { return true; }
        if (!Acquire()) { m_timeline.Stop(); return false; }
        auto frame = m_timeline.Sample();
        if (!frame) { m_timeline.Stop(); Release(true); return Fail(frame.error); }
        return Apply(frame.value);
    }
    void CineScriptComponent::OnTick(float dt, AZ::ScriptTimePoint)
    {
        if (m_pendingPlay) { m_pendingPlay = false; Play(); return; }
        if (!m_timeline.IsPlaying() || m_dispatching) { return; }
        DispatchGuard guard(m_dispatching);
        auto frame = m_timeline.Advance(dt);
        if (!frame) { m_timeline.Stop(); Release(true); Fail(frame.error); return; }
        Apply(frame.value);
    }
}
