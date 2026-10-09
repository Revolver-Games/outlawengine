/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "ScriptMotionPlayback.h"

#include <ScriptMotion/ScriptMotion.h>
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

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Wanted::ScriptMotion::O3DE
{
    namespace
    {
        constexpr std::size_t MaxBakedTransformSamples = 2000000;
        constexpr std::size_t MaxBakeValidationWork = 20000000;
        constexpr double MaxBakedDurationSeconds = 600.0;

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

        // Bake the validated evaluator output into the engine's native motion representation.
        // This preserves EMotionFX pose linking, native event processing, and blend-in behavior.
        // Smooth curves are approximated between samples; discontinuous Step curves are rejected.
        Result<AZStd::unique_ptr<EMotionFX::NonUniformMotionData>> Bake(const Clip& clip,
            const Skeleton& skeleton, float sampleRate)
        {
            if (clip.tracks.empty())
            {
                return {{}, "The initial native adapter requires at least one animated bone track."};
            }
            if (clip.duration > MaxBakedDurationSeconds)
            {
                return {{}, "The initial native adapter supports clips up to 600 seconds."};
            }
            const std::size_t intervals = static_cast<std::size_t>(std::ceil(clip.duration * sampleRate));
            std::vector<float> times;
            times.reserve(intervals + 2);
            for (std::size_t index = 0; index <= intervals; ++index)
            {
                times.push_back(static_cast<float>(std::min(clip.duration, static_cast<double>(index) / sampleRate)));
            }
            times.push_back(static_cast<float>(clip.duration));
            std::size_t keyCount = 0;
            for (const BoneTrack& track : clip.tracks)
            {
                keyCount += track.keys.size();
                for (std::size_t index = 0; index < track.keys.size(); ++index)
                {
                    const Keyframe& key = track.keys[index];
                    if (key.interpolation == Interpolation::Step && index + 1 < track.keys.size())
                    {
                        return {{}, "Step interpolation is supported by the core evaluator but not the initial native adapter. Use linear/smoothstep for engine playback."};
                    }
                    times.push_back(static_cast<float>(key.time));
                }
            }
            std::sort(times.begin(), times.end());
            times.erase(std::unique(times.begin(), times.end()), times.end());
            if (times.size() < 2 || times.size() > MaxBakedTransformSamples / std::max<std::size_t>(1, skeleton.bones.size()))
            {
                return {{}, "Native motion exceeds the two-million-transform baking budget or has unrepresentable duration."};
            }
            // The portable API revalidates authored data on every sample. Bound that
            // work as well as output memory until we have an immutable validated clip.
            std::size_t validationItems = skeleton.bones.size() + clip.events.size();
            for (const BoneTrack& track : clip.tracks)
            {
                validationItems += track.keys.size();
            }
            constexpr std::size_t MaxValidationVisits = 40000000;
            if (times.size() > MaxValidationVisits / std::max<std::size_t>(1, validationItems))
            {
                return {{}, "Native bake exceeds the validation-work budget; reduce clip length, keys or sample rate."};
            }
            // EvaluatePose revalidates its mutable input for every sample. Bound that work until
            // a validated immutable clip/cache API can move preprocessing out of the editor thread.
            const std::size_t workPerSample = skeleton.bones.size() + keyCount + clip.events.size();
            if (times.size() > MaxBakeValidationWork / std::max<std::size_t>(1, workPerSample))
            {
                return {{}, "Clip exceeds the initial adapter's bounded bake-work budget; split the clip or lower the sample rate."};
            }
            auto native = AZStd::make_unique<EMotionFX::NonUniformMotionData>();
            native->SetSampleRate(sampleRate);
            for (const BoneTrack& track : clip.tracks)
            {
                const Bone& bone = skeleton.bones[track.boneIndex];
                const EMotionFX::Transform bind = ToNative(bone.bindPose);
                const std::size_t joint = native->AddJoint(AZStd::string(bone.name.c_str()), bind, bind);
                native->AllocateJointPositionSamples(joint, times.size());
                native->AllocateJointRotationSamples(joint, times.size());
            }
            for (std::size_t sample = 0; sample < times.size(); ++sample)
            {
                const auto pose = EvaluatePose(clip, skeleton, std::min(static_cast<double>(times[sample]), clip.duration), {1.0, false});
                if (!pose)
                {
                    return {{}, pose.error};
                }
                for (std::size_t joint = 0; joint < clip.tracks.size(); ++joint)
                {
                    const EMotionFX::Transform value = ToNative(pose.value.localTransforms[clip.tracks[joint].boneIndex]);
                    if (!value.m_position.IsFinite() || !value.m_rotation.IsFinite())
                    {
                        return {{}, "A source transform exceeds the native float representation."};
                    }
                    native->SetJointPositionSample(joint, sample, {times[sample], value.m_position});
                    native->SetJointRotationSample(joint, sample, {times[sample], value.m_rotation});
                }
            }
            native->SetDuration(static_cast<float>(clip.duration));
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
        const auto skeletonJson = ReadJson(configuration.m_skeletonPath);
        if (!skeletonJson)
        {
            return Fail(skeletonJson.error.c_str());
        }
        const auto skeleton = ParseSkeleton(skeletonJson.value);
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
        playback.m_blendInTime = configuration.m_blendInSeconds;
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
            // Reset last time so scrubbing does not emit events from the skipped interval.
            live->SetCurrentTime(timeSeconds, true);
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
