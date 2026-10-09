/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/MotionBake.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Wanted::ScriptMotion
{
    Result<BakedMotion> BakeMotion(const Clip& clip, const Skeleton& skeleton, float sampleRate)
    {
        if (const std::string error = ValidateClip(clip, skeleton); !error.empty())
        {
            return {{}, error};
        }
        if (!std::isfinite(sampleRate) || sampleRate < 30.0f || sampleRate > 240.0f)
        {
            return {{}, "Native sample rate must be finite and in [30,240]."};
        }
        if (clip.duration > MaxBakedDurationSeconds)
        {
            return {{}, "Native playback supports clips up to 600 seconds."};
        }

        BakedMotion result;
        result.duration = static_cast<float>(clip.duration);
        result.sampleRate = sampleRate;
        const auto intervals = static_cast<std::size_t>(std::ceil(clip.duration * sampleRate));
        result.times.reserve(intervals + 2);
        for (std::size_t index = 0; index <= intervals; ++index)
        {
            result.times.push_back(static_cast<float>(std::min(clip.duration, static_cast<double>(index) / sampleRate)));
        }
        result.times.push_back(result.duration);

        std::size_t validationItems = skeleton.bones.size() + clip.events.size();
        // ValidateSkeleton walks every ancestor chain. Counting only joints would
        // allow a deep rig to multiply per-sample validation work quadratically.
        for (std::size_t index = 0; index < skeleton.bones.size(); ++index)
        {
            for (int current = static_cast<int>(index); current != -1;
                current = skeleton.bones[static_cast<std::size_t>(current)].parent)
            {
                ++validationItems;
            }
        }
        for (const BoneTrack& track : clip.tracks)
        {
            validationItems += track.keys.size();
            for (std::size_t index = 0; index < track.keys.size(); ++index)
            {
                const Keyframe& key = track.keys[index];
                if (index + 1 < track.keys.size())
                {
                    if (key.interpolation == Interpolation::Step)
                    {
                        return {{}, "Step segments cannot be represented by native linear motion samples; use linear/smoothstep."};
                    }
                    if (static_cast<float>(key.time) >= static_cast<float>(track.keys[index + 1].time))
                    {
                        return {{}, "Distinct keys collapse to the same native float timestamp; separate the keys."};
                    }
                }
                result.times.push_back(static_cast<float>(key.time));
            }
        }
        std::sort(result.times.begin(), result.times.end());
        result.times.erase(std::unique(result.times.begin(), result.times.end()), result.times.end());
        if (result.times.size() < 2 || result.times.size() > MaxBakedTransformSamples / skeleton.bones.size())
        {
            return {{}, "Native motion exceeds the two-million-transform budget or has unrepresentable duration."};
        }
        // EvaluatePose revalidates mutable data. Bound its work before materializing poses.
        if (result.times.size() > MaxBakeValidationWork / std::max<std::size_t>(1, validationItems))
        {
            return {{}, "Native bake exceeds its validation-work budget; reduce clip length, keys or sample rate."};
        }
        result.poses.reserve(result.times.size());
        for (const float time : result.times)
        {
            // Always sample the exact final source time, even when its float rounds down.
            const double sourceTime = time == result.duration ? clip.duration : static_cast<double>(time);
            auto pose = EvaluatePose(clip, skeleton, sourceTime, {1.0, false});
            if (!pose)
            {
                return {{}, pose.error};
            }
            result.poses.push_back(std::move(pose.value));
        }
        return {std::move(result), {}};
    }
}
