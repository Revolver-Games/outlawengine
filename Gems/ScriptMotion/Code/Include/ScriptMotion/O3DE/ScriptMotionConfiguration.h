/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/string/string.h>

namespace AZ { class ReflectContext; }

namespace Wanted::ScriptMotion::O3DE
{
    struct ScriptMotionConfiguration
    {
        AZ_TYPE_INFO(ScriptMotionConfiguration, "{D28CB029-0D30-4108-9D76-790A63104F40}");

        // FileIO aliases are supported, e.g. @projectroot@/Assets/ScriptMotion/test.skeleton.json.
        AZStd::string m_skeletonPath;
        AZStd::string m_clipPath;
        bool m_playOnActivation = true;
        bool m_loop = true;
        float m_playbackSpeed = 1.0f;
        float m_blendInSeconds = 0.15f;
        float m_sampleRate = 120.0f;

        static void Reflect(AZ::ReflectContext* context);
    };
}
