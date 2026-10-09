/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <CineScript/Timeline.h>
#include <Wanted/FirstErrand.h>
#include <fstream>
#include <iostream>

namespace CS = Wanted::CineScript;
std::string Read(const char* path)
{
    std::ifstream input(path, std::ios::ate | std::ios::binary);
    if (!input || input.tellg() < 0 || input.tellg() > 1024 * 1024) { return {}; }
    std::string text(static_cast<std::size_t>(input.tellg()), '\0');
    input.seekg(0);
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size()))) { return {}; }
    return text;
}
int main(int argc, char** argv)
{
    if (argc != 4) { std::cerr << "Usage: cinescript_demo scene.json skeleton.json wave.json\n"; return 2; }
    auto scene = CS::ParseScene(Read(argv[1]));
    auto skeleton = CS::SM::ParseSkeleton(Read(argv[2]));
    if (!scene || !skeleton) { std::cerr << (!scene ? scene.error : skeleton.error); return 1; }
    auto clip = CS::SM::ParseClip(Read(argv[3]), skeleton.value);
    if (!clip) { std::cerr << clip.error; return 1; }
    Wanted::FirstErrand mission;
    if (!mission.Start() || mission.Interact("mail_satchel") != Wanted::FirstErrand::Transition::Ignored
        || mission.Interact("ada_mercer") != Wanted::FirstErrand::Transition::ObjectiveChanged
        || mission.Interact("mail_satchel") != Wanted::FirstErrand::Transition::ObjectiveChanged
        || mission.Interact("ada_mercer") != Wanted::FirstErrand::Transition::Completed
        || mission.Interact("ada_mercer") != Wanted::FirstErrand::Transition::Ignored) { return 1; }
    CS::Timeline timeline;
    auto error = timeline.Load(scene.value, {"satchel_returned"});
    if (!error.empty()) { std::cerr << error; return 1; }
    auto frame = timeline.Start();
    if (!frame) { return 1; }
    std::string previousCamera, previousSubtitle;
    std::size_t animationFrames = 0, completions = 0, acknowledgements = 0, cameraCuts = 0, lines = 0;
    for (int i = 0; i <= 448; ++i)
    {
        if (i) { frame = timeline.Advance(1.0 / 64.0); }
        if (!frame) { std::cerr << frame.error; return 1; }
        if (frame.value.camera != previousCamera)
        { ++cameraCuts; previousCamera = frame.value.camera; std::cout << "camera " << previousCamera << '\n'; }
        if (frame.value.subtitle != previousSubtitle)
        {
            previousSubtitle = frame.value.subtitle;
            if (!previousSubtitle.empty()) { ++lines; std::cout << frame.value.speaker << ": " << previousSubtitle << '\n'; }
        }
        for (const auto& animation : frame.value.animations)
        {
            if (animation.clip != clip.value.name) { std::cerr << "Unbound animation"; return 1; }
            auto pose = CS::SM::EvaluatePose(clip.value, skeleton.value, animation.time, {1, false});
            if (!pose || pose.value.localTransforms.size() != 7) { return 1; }
            ++animationFrames;
        }
        for (const auto& cue : frame.value.cues)
        {
            std::cout << "cue " << cue.value << '\n';
            if (cue.value == "delivery_acknowledged") { ++acknowledgements; }
        }
        if (frame.value.completed) { ++completions; }
    }
    if (animationFrames != 128 || completions != 1 || acknowledgements != 1 || cameraCuts != 2 || lines != 2
        || frame.value.gameplayLocked || timeline.IsPlaying()) { return 1; }
    std::cout << "Mission completed once; 2 camera cuts; 2 dialogue lines; " << animationFrames
              << " evaluated animation frames; one completion; player control released.\n";
    return 0;
}
