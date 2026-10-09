/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <ScriptMotion/ScriptMotion.h>

namespace Wanted::ScriptMotion
{
    // A portable, validated bake consumed directly by the EMotion FX adapter.
    // Includes every skeleton joint (including untracked bind-pose joints).
    // Float timestamps match the native format. Values remain double until upload.
    struct BakedMotion
    {
        float duration = 0.0f;
        float sampleRate = 0.0f;
        std::vector<float> times;
        std::vector<Pose> poses; // one local-space pose per timestamp, in skeleton order
    };

    inline constexpr std::size_t MaxBakedTransformSamples = 2000000;
    inline constexpr std::size_t MaxBakeValidationWork = 20000000;
    inline constexpr double MaxBakedDurationSeconds = 600.0;

    // Validates before allocation; samples both endpoints and all authored key times.
    // Linear native interpolation approximates smoothstep/SLERP between samples.
    // Step segments and distinct keys collapsing to one native float time are rejected.
    // Failure returns no partial bake. Sample rate must be finite and in [30,240].
    [[nodiscard]] Result<BakedMotion> BakeMotion(const Clip& clip, const Skeleton& skeleton,
        float sampleRate = 120.0f);
}
