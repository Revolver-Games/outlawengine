/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "ScriptMotionPlayback.h"

#include <ScriptMotion/ScriptMotion.h>
#include <ScriptMotion/MotionBake.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/Motion.h>
#include <EMotionFX/Source/MotionData/NonUniformMotionData.h>
#include <EMotionFX/Source/MotionEventTable.h>
#include <EMotionFX/Source/MotionEventTrack.h>
#include <EMotionFX/Source/MotionInstance.h>
#include <EMotionFX/Source/MotionSystem.h>
#include <EMotionFX/Source/Node.h>
#include <EMotionFX/Source/PlayBackInfo.h>
#include <EMotionFX/Source/Pose.h>
#include <EMotionFX/Source/Skeleton.h>
#include <EMotionFX/Source/TransformData.h>
#include <EMotionFX/Source/TwoStringEventData.h>

#include <cmath>
#include <string>
#include <utility>

namespace Wanted::ScriptMotion::O3DE
{
    namespace
    {
        Result<std::string> ReadJson(const AZStd::string& path)
        {
            if (path.empty())
            {
                return {{}, "A skeleton JSON path and animation JSON path are required."};
            }
            if (!AZ::IO::FileIOBase::GetInstance())
            {
                return {{}, "O3DE FileIO is unavailable."};
            }
            AZ::IO::FileIOStream input(path.c_str(), AZ::IO::OpenMode::ModeRead);
            if (!input.IsOpen())
            {
                return {{}, std::string("Cannot read ScriptMotion source: ") + path.c_str()};
            }
            const auto length = input.GetLength();
            if (length == 0 || length > MaxJsonBytes)
            {
                return {{}, "ScriptMotion JSON must be nonempty and at most 8 MiB."};
            }
            std::string json(static_cast<std::size_t>(length), '\0');
            if (input.Read(length, json.data()) != length)
            {
                return {{}, "ScriptMotion JSON could not be read completely."};
            }
            return {std::move(json), {}};
        }

        EMotionFX::Transform ToNative(const LocalTransform& local)
        {
            EMotionFX::Transform result = EMotionFX::Transform::CreateIdentity();
            result.m_position = AZ::Vector3(static_cast<float>(local.translation.x),
                static_cast<float>(local.translation.y), static_cast<float>(local.translation.z));
            result.m_rotation = AZ::Quaternion(static_cast<float>(local.rotation.x),
                static_cast<float>(local.rotation.y), static_cast<float>(local.rotation.z),
                static_cast<float>(local.rotation.w));
            return result;
        }

        std::string ValidateActor(const Skeleton& source, const EMotionFX::ActorInstance& actor)
        {
            const EMotionFX::Skeleton* target = actor.GetActor()->GetSkeleton();
            const EMotionFX::Pose* bindPose = actor.GetTransformData()->GetBindPose();
            // Matching names alone is unsafe: different parent spaces or bind poses deform the actor.
            if (target->GetNumNodes() != source.bones.size())
            {
                return "Actor and ScriptMotion skeleton have different bone counts; retargeting is not enabled.";
            }
            for (const Bone& bone : source.bones)
            {
                const EMotionFX::Node* node = target->FindNodeByName(bone.name.c_str());
                if (!node)
                {
                    return "Actor is missing bone: " + bone.name;
                }
                const std::size_t parent = node->GetParentIndex();
                if (bone.parent < 0)
                {
                    if (parent != InvalidIndex)
                    {
                        return "Actor root-parent mismatch at bone: " + bone.name;
                    }
                }
                else if (parent == InvalidIndex || source.bones[static_cast<std::size_t>(bone.parent)].name != target->GetNode(parent)->GetName())
                {
                    return "Actor parent mismatch at bone: " + bone.name;
                }
                const EMotionFX::Transform& targetBind = bindPose->GetLocalSpaceTransform(node->GetNodeIndex());
                const EMotionFX::Transform authoredBind = ToNative(bone.bindPose);
                if (!targetBind.m_position.IsClose(authoredBind.m_position, 0.001f) ||
                    std::abs(std::abs(targetBind.m_rotation.Dot(authoredBind.m_rotation)) - 1.0f) > 0.0001f)
                {
                    return "Actor bind-pose mismatch at bone: " + bone.name;
                }
#ifndef EMFX_SCALE_DISABLED
                if (!targetBind.m_scale.IsClose(AZ::Vector3::CreateOne(), 0.0001f))
                {
                    return "Scaled bind poses are not supported in this ScriptMotion adapter: " + bone.name;
                }
#endif
            }
            return {};
        }

        Result<Skeleton> CaptureActorSkeleton(const EMotionFX::ActorInstance& actor, const AZStd::string& name)
        {
            Skeleton result;
            result.name = name.c_str();
            const auto* native = actor.GetActor()->GetSkeleton();
            const auto* bind = actor.GetTransformData()->GetBindPose();
            if (native->GetNumNodes() == 0 || native->GetNumNodes() > MaxBones)
            {
                return {{}, "Actor must have between 1 and 1024 joints."};
            }
            for (std::size_t index = 0; index < native->GetNumNodes(); ++index)
            {
                const auto* node = native->GetNode(index);
                const auto& transform = bind->GetLocalSpaceTransform(index);
#ifndef EMFX_SCALE_DISABLED
                if (!transform.m_scale.IsClose(AZ::Vector3::CreateOne(), 0.0001f))
                {
                    return {{}, "Scaled Actor bind poses are not supported."};
                }
#endif
                Bone bone;
                bone.name = node->GetName();
                bone.parent = node->GetParentIndex() == InvalidIndex ? -1 : static_cast<int>(node->GetParentIndex());
                bone.bindPose.translation = {transform.m_position.GetX(), transform.m_position.GetY(), transform.m_position.GetZ()};
                bone.bindPose.rotation = {transform.m_rotation.GetX(), transform.m_rotation.GetY(),
                    transform.m_rotation.GetZ(), transform.m_rotation.GetW()};
                result.bones.push_back(std::move(bone));
            }
            if (const std::string error = ValidateSkeleton(result); !error.empty())
            {
                return {{}, error};
            }
            return {std::move(result), {}};
        }

        Result<AZStd::unique_ptr<EMotionFX::NonUniformMotionData>> Bake(const Clip& clip,
            const Skeleton& skeleton, float sampleRate)
        {
            const auto baked = BakeMotion(clip, skeleton, sampleRate);
            if (!baked)
            {
                return {{}, baked.error};
            }
            auto native = AZStd::make_unique<EMotionFX::NonUniformMotionData>();
            native->SetSampleRate(sampleRate);
            for (std::size_t joint = 0; joint < skeleton.bones.size(); ++joint)
            {
                const Bone& bone = skeleton.bones[joint];
                const auto bind = ToNative(bone.bindPose);
                native->AddJoint(AZStd::string(bone.name.c_str()), bind, bind);
                native->AllocateJointPositionSamples(joint, baked.value.times.size());
                native->AllocateJointRotationSamples(joint, baked.value.times.size());
                for (std::size_t sample = 0; sample < baked.value.times.size(); ++sample)
                {
                    const auto value = ToNative(baked.value.poses[sample].localTransforms[joint]);
                    native->SetJointPositionSample(joint, sample, {baked.value.times[sample], value.m_position});
                    native->SetJointRotationSample(joint, sample, {baked.value.times[sample], value.m_rotation});
                }
            }
            native->SetDuration(baked.value.duration);
            if (!native->VerifyIntegrity())
            {
                return {{}, "EMotionFX rejected the baked motion's keyframe integrity."};
            }
            return {AZStd::move(native), {}};
        }
    }

    ScriptMotionPlayback::~ScriptMotionPlayback()
    {
        Stop();
    }

    bool ScriptMotionPlayback::Fail(const char* error)
    {
        m_error = error;
        return false;
    }

    void ScriptMotionPlayback::SetActor(EMotionFX::ActorInstance* actor)
    {
        Stop();
        m_actor = actor;
    }

    EMotionFX::MotionInstance* ScriptMotionPlayback::GetLiveInstance() const
    {
        if (!m_actor || !m_actor->GetMotionSystem() || !m_instance)
        {
            return nullptr;
        }
        const EMotionFX::MotionSystem* system = m_actor->GetMotionSystem();
        for (std::size_t index = 0; index < system->GetNumMotionInstances(); ++index)
        {
            EMotionFX::MotionInstance* candidate = system->GetMotionInstance(index);
            // Never dereference a previously deleted MotionInstance just to test its validity.
            if (candidate == m_instance && candidate->GetMotion() == m_motion)
            {
                return candidate;
            }
        }
        return nullptr;
    }

    bool ScriptMotionPlayback::Play(const ScriptMotionConfiguration& configuration)
    {
        if (!m_actor || !m_actor->GetMotionSystem())
        {
            return Fail("The entity's EMotionFX Actor is not ready.");
        }
        if (m_actor->GetAnimGraphInstance())
        {
            return Fail("ScriptMotion simple playback cannot drive an Actor with an active Animation Graph.");
        }
        if (!std::isfinite(configuration.m_playbackSpeed) || configuration.m_playbackSpeed < 0.0f || configuration.m_playbackSpeed > 10.0f ||
            !std::isfinite(configuration.m_blendInSeconds) || configuration.m_blendInSeconds < 0.0f || configuration.m_blendInSeconds > 10.0f ||
            !std::isfinite(configuration.m_sampleRate) || configuration.m_sampleRate < 30.0f || configuration.m_sampleRate > 240.0f)
        {
            return Fail("Playback requires finite speed/blend values in [0,10] and sample rate in [30,240].");
        }
        Result<Skeleton> skeleton;
        if (configuration.m_useActorBindPose)
        {
            skeleton = CaptureActorSkeleton(*m_actor, configuration.m_actorSkeletonName);
        }
        else
        {
            const auto skeletonJson = ReadJson(configuration.m_skeletonPath);
            if (!skeletonJson)
            {
                return Fail(skeletonJson.error.c_str());
            }
            skeleton = ParseSkeleton(skeletonJson.value);
        }
        if (!skeleton)
        {
            return Fail(skeleton.error.c_str());
        }
        const std::string compatibility = ValidateActor(skeleton.value, *m_actor);
        if (!compatibility.empty())
        {
            return Fail(compatibility.c_str());
        }
        const auto clipJson = ReadJson(configuration.m_clipPath);
        if (!clipJson)
        {
            return Fail(clipJson.error.c_str());
        }
        const auto clip = ParseClip(clipJson.value, skeleton.value);
        if (!clip)
        {
            return Fail(clip.error.c_str());
        }
        auto baked = Bake(clip.value, skeleton.value, configuration.m_sampleRate);
        if (!baked)
        {
            return Fail(baked.error.c_str());
        }

        // Invalid edits above leave the currently playing motion intact.
        Stop();
        m_motion = aznew EMotionFX::Motion(clip.value.name.c_str());
        m_motion->SetIsOwnedByRuntime(true);
        m_motion->SetFileName(configuration.m_clipPath.c_str());
        m_motion->SetMotionData(baked.value.release());
        m_motion->UpdateDuration();
        if (!clip.value.events.empty())
        {
            auto* track = EMotionFX::MotionEventTrack::Create("ScriptMotion", m_motion);
            m_motion->GetEventTable()->AddTrack(track);
            for (const AnimationEvent& event : clip.value.events)
            {
                EMotionFX::EventDataPtr data = AZStd::make_shared<EMotionFX::TwoStringEventData>(
                    AZStd::string(event.name.c_str()), AZStd::string(event.payloadJson.c_str()));
                track->AddEvent(static_cast<float>(event.time), AZStd::move(data));
            }
        }
        EMotionFX::PlayBackInfo playback;
        playback.m_numLoops = configuration.m_loop ? EMFX_LOOPFOREVER : 1;
        playback.m_playSpeed = configuration.m_playbackSpeed;
        // A paused initial preview must have visible weight; EMotion FX advances blending with motion time.
        playback.m_blendInTime = configuration.m_playbackSpeed == 0.0f ? 0.0f : configuration.m_blendInSeconds;
        playback.m_playNow = true;
        playback.m_deleteOnZeroWeight = false;
        playback.m_canOverwrite = false;
        playback.m_freezeAtLastFrame = true;
        playback.m_motionExtractionEnabled = false;
        playback.m_retarget = false;
        playback.m_enableMotionEvents = true;
        m_instance = m_actor->GetMotionSystem()->PlayMotion(m_motion, &playback);
        if (!m_instance)
        {
            Stop();
            return Fail("EMotionFX could not create a motion instance.");
        }
        m_error.clear();
        return true;
    }

    void ScriptMotionPlayback::Stop()
    {
        if (EMotionFX::MotionInstance* live = GetLiveInstance())
        {
            m_actor->GetMotionSystem()->RemoveMotionInstance(live);
        }
        m_instance = nullptr;
        if (m_motion)
        {
            m_motion->Destroy();
            m_motion = nullptr;
        }
    }

    bool ScriptMotionPlayback::SetPlaybackSpeed(float speed)
    {
        if (!std::isfinite(speed) || speed < 0.0f || speed > 10.0f)
        {
            return Fail("Playback speed must be finite and in [0,10].");
        }
        if (EMotionFX::MotionInstance* live = GetLiveInstance())
        {
            live->SetPlaySpeed(speed);
            if (speed == 0.0f)
            {
                live->SetWeight(1.0f, 0.0f);
                m_actor->UpdateTransformations(0.0f, true, true);
            }
            m_error.clear();
            return true;
        }
        return Fail("No ScriptMotion clip is playing.");
    }

    bool ScriptMotionPlayback::Seek(float timeSeconds)
    {
        if (!std::isfinite(timeSeconds) || timeSeconds < 0.0f || timeSeconds > GetDuration())
        {
            return Fail("Seek time must be finite and within the current clip.");
        }
        if (EMotionFX::MotionInstance* live = GetLiveInstance())
        {
            // Clear the finished loop/frozen state as well as last time. A finished non-looping
            // motion otherwise stays frozen when scrubbed back. Skipped events are not emitted.
            live->ResetTimes();
            live->SetCurrentTime(timeSeconds, true);
            live->SetWeight(1.0f, 0.0f);
            m_actor->UpdateTransformations(0.0f, true, true);
            m_error.clear();
            return true;
        }
        return Fail("No ScriptMotion clip is playing.");
    }

    float ScriptMotionPlayback::GetDuration() const
    {
        const EMotionFX::MotionInstance* live = GetLiveInstance();
        return live ? live->GetDuration() : 0.0f;
    }
}
