/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <ScriptMotion/Player.h>
#include <fstream>
#include <iostream>
#include <cmath>

namespace SM = Wanted::ScriptMotion;
static SM::Result<std::string> Read(const char* path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) { return {{}, "Cannot open input"}; }
    const auto size = in.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(SM::MaxJsonBytes)) { return {{}, "Invalid input size"}; }
    std::string text(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    if (!in.read(text.data(), static_cast<std::streamsize>(text.size()))) { return {{}, "Read failed"}; }
    return {std::move(text), {}};
}
int main(int argc, char** argv)
{
    if (argc != 5) { std::cerr << "Usage: scriptmotion_player rig.json idle.json walk.json greeting.json\n"; return 2; }
    auto rigText = Read(argv[1]);
    if (!rigText) { std::cerr << rigText.error; return 1; }
    auto rig = SM::ParseSkeleton(rigText.value);
    if (!rig) { std::cerr << rig.error; return 1; }
    SM::Player player;
    double distance = 0;
    std::size_t footsteps = 0, completions = 0;
    for (int index = 2; index < 5; ++index)
    {
        auto text = Read(argv[index]);
        if (!text) { std::cerr << text.error; return 1; }
        auto clip = SM::ParseClip(text.value, rig.value);
        if (!clip) { std::cerr << clip.error; return 1; }
        std::string error = index == 2 ? player.Load(rig.value, clip.value)
            : player.TransitionTo(clip.value, 0.25, index != 4);
        if (error.empty()) { error = player.SetRootMotion(0); }
        if (!error.empty()) { std::cerr << error; return 1; }
        for (int frame = 0; frame < 256; ++frame)
        {
            auto result = player.Advance(1.0 / 64.0);
            if (!result) { std::cerr << result.error; return 1; }
            if (index == 3) { distance += result.value.rootTranslationDelta.z; }
            if (result.value.completed) { ++completions; }
            for (const auto& event : result.value.events)
            {
                const auto& source = player.GetClip().events[event.eventIndex];
                if (source.name == "footstep") { ++footsteps; }
                std::cout << player.GetClip().name << " event " << event.timelineSeconds << ' ' << source.name << '\n';
            }
        }
    }
    if (std::abs(distance - 4.8) > 1e-8 || footsteps != 8 || completions != 1) { return 1; }
    std::cout << "768 frames; walk distance=" << distance << "m; footsteps=" << footsteps
              << "; greeting completions=" << completions << '\n';
    return 0;
}
