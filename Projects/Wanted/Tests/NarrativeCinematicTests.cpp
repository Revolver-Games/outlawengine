/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <Wanted/Narrative.h>
#include <CineScript/Timeline.h>
#include <fstream>
#include <iterator>
#include <iostream>

namespace N=Wanted::Narrative;
namespace C=Wanted::CineScript;
int checks=0;
#define CHECK(...) do { ++checks; if(!(__VA_ARGS__)) { std::cerr<<"FAIL "<<__LINE__<<": "<<#__VA_ARGS__<<'\n'; return 1; } } while(false)
std::string Read(const char* path)
{ std::ifstream input(path); return {std::istreambuf_iterator<char>(input),{}}; }
int main(int argc,char** argv)
{
    CHECK(argc==5);
    const auto content=Read(argv[1]);
    N::Session mission;
    CHECK(mission.Load(content).empty() && mission.Start());
    CHECK(!mission.Event("collect","mail_satchel").value.accepted);
    CHECK(mission.BeginDialogue("ada_intro") && mission.Choose("accept_delivery"));
    CHECK(mission.Event("collect","mail_satchel").value.accepted);
    const auto checkpoint=mission.Save();
    N::Session restored;
    CHECK(restored.Load(content).empty() && restored.Restore(checkpoint).empty());
    CHECK(restored.BeginDialogue("ada_return"));
    const auto completion=restored.Choose("return_mail");
    CHECK(completion && completion.value.completed && completion.value.creditsAdded==5);
    auto scene=C::ParseScene(Read(argv[2]));
    CHECK(scene && scene.value.id==completion.value.cutscene);
    C::Timeline cinematic;
    std::vector<std::string> flags(restored.Story().flags.begin(),restored.Story().flags.end());
    CHECK(cinematic.Load(scene.value,flags).empty() && cinematic.Start());
    auto rig=C::SM::ParseSkeleton(Read(argv[3]));
    CHECK(rig);
    auto wave=C::SM::ParseClip(Read(argv[4]),rig.value);
    CHECK(wave);
    std::size_t acknowledgements=0, finished=0, poses=0;
    for(int frame=0;frame<448;++frame)
    {
        auto step=cinematic.Advance(1.0/64.0);
        CHECK(step);
        for(const auto& cue:step.value.cues) { if(cue.value=="delivery_acknowledged") { ++acknowledgements; } }
        for(const auto& animation:step.value.animations)
        {
            CHECK(animation.clip==wave.value.name);
            CHECK(C::SM::EvaluatePose(wave.value,rig.value,animation.time,{1,false})); ++poses;
        }
        finished+=step.value.completed?1u:0u;
    }
    CHECK(acknowledgements==1 && finished==1 && poses==128);
    CHECK(!cinematic.IsPlaying() && !cinematic.Sample().value.gameplayLocked);
    CHECK(restored.Story().credits==5 && !restored.Choose("return_mail"));
    CHECK(restored.Restore(restored.Save()).empty() && restored.Story().credits==5);
    CHECK(cinematic.Start() && cinematic.Advance(3));
    const auto sceneCheckpoint=cinematic.SaveCheckpoint();
    C::Timeline resumed;
    CHECK(resumed.Load(scene.value).empty() && resumed.RestoreCheckpoint(sceneCheckpoint).empty());
    auto skipped=resumed.Skip();
    CHECK(skipped && skipped.value.completed && skipped.value.cues.empty());
    CHECK(restored.Story().credits==5 && restored.Story().flags.contains("postmark_clue"));
    std::cout<<checks<<" combined assertions passed: authored mission -> story flags -> CineScript -> ScriptMotion; save/skip without duplicate rewards.\n";
    return 0;
}
