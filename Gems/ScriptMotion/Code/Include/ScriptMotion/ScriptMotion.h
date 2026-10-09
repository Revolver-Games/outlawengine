/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Wanted::ScriptMotion
{
    template<class T>
    struct Result
    {
        T value{};
        std::string error;
        [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
    };

    struct Vec3 { double x = 0.0; double y = 0.0; double z = 0.0; };
    // Quaternion component order is x, y, z, w. Rotations are local to each bone's parent.
    struct Quaternion { double x = 0.0; double y = 0.0; double z = 0.0; double w = 1.0; };
    struct LocalTransform { Vec3 translation; Quaternion rotation; };
    struct Bone { std::string name; int parent = -1; LocalTransform bindPose; };
    struct Skeleton { std::string name; std::vector<Bone> bones; };
    enum class Interpolation { Step, Linear, SmoothStep };
    struct Keyframe
    {
        double time = 0.0;
        LocalTransform transform;
        // Controls the segment starting at this key. Rotation always uses shortest-path SLERP.
        Interpolation interpolation = Interpolation::Linear;
    };
    struct BoneTrack { std::size_t boneIndex = 0; std::vector<Keyframe> keys; };
    struct AnimationEvent { double time = 0.0; std::string name; std::string payloadJson = "{}"; };
    struct Clip
    {
        std::string name;
        std::string skeletonName;
        double duration = 0.0;
        std::vector<BoneTrack> tracks;
        std::vector<AnimationEvent> events;
    };
    struct Pose { std::vector<LocalTransform> localTransforms; };
    struct PlaybackOptions { double speed = 1.0; bool loop = true; };
    struct EventOccurrence { std::size_t eventIndex = 0; double timelineSeconds = 0.0; };

    inline constexpr std::size_t MaxJsonBytes = 8 * 1024 * 1024;
    inline constexpr std::size_t MaxBones = 1024;
    inline constexpr std::size_t MaxKeysPerTrack = 8192;
    inline constexpr std::size_t MaxTotalKeys = 100000;
    inline constexpr std::size_t MaxEvents = 4096;
    inline constexpr std::size_t MaxCollectedEvents = 65536;

    // Empty string means valid. Parse failures and invalid runtime input are reported as Result::error.
    [[nodiscard]] std::string ValidateSkeleton(const Skeleton& skeleton);
    [[nodiscard]] std::string ValidateClip(const Clip& clip, const Skeleton& skeleton);
    [[nodiscard]] Result<Skeleton> ParseSkeleton(std::string_view json);
    [[nodiscard]] Result<Clip> ParseClip(std::string_view json, const Skeleton& skeleton);
    // Timeline starts at zero. Nonnegative speed is supported; negative/reverse playback is rejected.
    [[nodiscard]] Result<double> ResolveSampleTime(const Clip& clip, double timelineSeconds, PlaybackOptions options = {});
    [[nodiscard]] Result<Pose> EvaluatePose(const Clip& clip, const Skeleton& skeleton, double timelineSeconds,
        PlaybackOptions options = {});
    // Returns events in (from, to], ordered by timeline then source index. Start-at-zero is not emitted.
    // Loop boundaries can contain both the previous loop's duration event and the next loop's zero event.
    // Exceeding the event budget returns an error without a partial batch. Both times must be nonnegative.
    [[nodiscard]] Result<std::vector<EventOccurrence>> CollectEvents(const Clip& clip, double fromTimelineSeconds,
        double toTimelineSeconds, PlaybackOptions options = {}, std::size_t maxEvents = 1024);
    // Empty mask means all bones. Otherwise mask length must equal pose size; weights lie in [0,1].
    [[nodiscard]] Result<Pose> BlendPoses(const Pose& base, const Pose& other, double weight,
        std::span<const double> boneMask = {});
    // Additive rotation delta is layer * inverse(reference), applied on the left of base in parent space.
    [[nodiscard]] Result<Pose> ApplyAdditiveLayer(const Pose& base, const Pose& layer, const Pose& reference,
        double weight, std::span<const double> boneMask = {});
    // Compose local bone transforms into model space (parent rotation and translation).
    // The returned Pose uses the same bone order as the input skeleton; no bind-pose inverse is applied.
    [[nodiscard]] Result<Pose> ComputeModelSpacePose(const Skeleton& skeleton, const Pose& localPose);
    [[nodiscard]] Result<Pose> EvaluateModelSpacePose(const Clip& clip, const Skeleton& skeleton,
        double timelineSeconds, PlaybackOptions options = {});
    // Low-level math primitive: both rotations must be finite/nonzero and alpha must be in [0,1].
    // Prefer the validated pose APIs when accepting untrusted caller data.
    [[nodiscard]] Quaternion Slerp(Quaternion from, Quaternion to, double alpha);
}
