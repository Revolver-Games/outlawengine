/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/ScriptMotion.h>
#include <ScriptMotion/MotionBake.h>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace SM = Wanted::ScriptMotion;

static SM::Result<std::string> ReadText(const char* path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { return { {}, std::string("Cannot open ") + path }; }
    const auto size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(SM::MaxJsonBytes))
    {
        return { {}, "Input exceeds ScriptMotion's 8 MiB limit" };
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size()))) { return { {}, "Failed to read input" }; }
    return { std::move(text), {} };
}

int main(int argc, char** argv)
{
    if (argc < 3 || argc > 5 || (argc == 5 && std::string_view(argv[4]) != "--bake"))
    {
        std::cerr << "Usage: scriptmotion_demo skeleton.json clip.scriptmotion.json [timeline_seconds [--bake]]\n";
        return 2;
    }
    double time = 1.0;
    if (argc >= 4)
    {
        const std::string_view text(argv[3]);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), time);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        {
            std::cerr << "Invalid timeline value\n";
            return 2;
        }
    }
    auto skeletonText = ReadText(argv[1]);
    auto clipText = ReadText(argv[2]);
    if (!skeletonText || !clipText)
    {
        std::cerr << (!skeletonText ? skeletonText.error : clipText.error) << '\n';
        return 1;
    }
    auto skeleton = SM::ParseSkeleton(skeletonText.value);
    if (!skeleton) { std::cerr << skeleton.error << '\n'; return 1; }
    auto clip = SM::ParseClip(clipText.value, skeleton.value);
    if (!clip) { std::cerr << clip.error << '\n'; return 1; }
    if (argc == 5)
    {
        const auto baked = SM::BakeMotion(clip.value, skeleton.value);
        if (!baked) { std::cerr << baked.error << '\n'; return 1; }
        std::cout << "Native bake: samples=" << baked.value.times.size()
            << " joints=" << skeleton.value.bones.size() << " rate=" << baked.value.sampleRate << '\n';
    }
    auto pose = SM::EvaluatePose(clip.value, skeleton.value, time);
    auto sampleTime = SM::ResolveSampleTime(clip.value, time);
    if (!pose || !sampleTime)
    {
        std::cerr << (!pose ? pose.error : sampleTime.error) << '\n';
        return 1;
    }
    auto events = SM::CollectEvents(clip.value, 0.0, time);
    if (!events) { std::cerr << events.error << '\n'; return 1; }
    std::cout << std::fixed << std::setprecision(6)
        << "ScriptMotion " << clip.value.name << ": timeline=" << time
        << " sample=" << sampleTime.value << " bones=" << pose.value.localTransforms.size() << '\n';
    for (std::size_t i = 0; i < pose.value.localTransforms.size(); ++i)
    {
        const auto& transform = pose.value.localTransforms[i];
        std::cout << skeleton.value.bones[i].name
            << " translation=[" << transform.translation.x << ',' << transform.translation.y << ',' << transform.translation.z
            << "] rotation=[" << transform.rotation.x << ',' << transform.rotation.y << ',' << transform.rotation.z
            << ',' << transform.rotation.w << "]\n";
    }
    for (const auto& event : events.value)
    {
        const auto& source = clip.value.events[event.eventIndex];
        std::cout << "event " << event.timelineSeconds << ' ' << source.name << ' ' << source.payloadJson << '\n';
    }
    return 0;
}
