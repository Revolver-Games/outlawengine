/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <Wanted/Narrative.h>
#include <fstream>
#include <iostream>
#include <iterator>

namespace N=Wanted::Narrative;
int checks=0;
#define CHECK(...) do { ++checks; if(!(__VA_ARGS__)) { std::cerr<<"FAIL "<<__LINE__<<": "<<#__VA_ARGS__<<'\n'; return 1; } } while(false)
std::string Replace(std::string s,const std::string& from,const std::string& to)
{ const auto at=s.find(from); if(at!=std::string::npos) { s.replace(at,from.size(),to); } return s; }
int main(int argc,char** argv)
{
    CHECK(argc==2);
    std::ifstream input(argv[1]);
    CHECK(input.good());
    const std::string content((std::istreambuf_iterator<char>(input)),{});
    CHECK(N::ParseDefinition(content));
    N::Session s;
    CHECK(!s.Start());
    CHECK(!s.Restore("{}").empty());
    CHECK(s.Load(content).empty());
    CHECK(s.ObjectiveText()=="No active mission.");
    CHECK(s.BeginDialogue("ada_intro"));
    CHECK(!s.Choose("accept_delivery")); // Quest choices cannot be consumed before starting.
    CHECK(s.CloseDialogue());
    CHECK(s.Start().value.accepted);
    CHECK(!s.Start().value.accepted);
    CHECK(!s.Event("collect","mail_satchel").value.accepted);
    CHECK(!s.Event("dialogue","accept_delivery")); // Prevent forged dialogue choice shortcuts.
    CHECK(!s.BeginDialogue("ada_return"));
    CHECK(s.BeginDialogue("ada_intro").value.accepted);
    CHECK(s.Choices().size()==2);
    CHECK(!s.BeginDialogue("ada_intro"));
    CHECK(s.Choose("ask_town").value.accepted);
    CHECK(s.Story().dialogue=="ada_town" && s.Story().flags.contains("asked_about_town"));
    CHECK(s.Choose("back_to_delivery").value.accepted);
    CHECK(s.Choices().size()==1);
    CHECK(!s.Choose("ask_town"));
    CHECK(s.Choose("accept_delivery").value.objectiveChanged);
    CHECK(s.Mission().objective==1 && s.Story().dialogue.empty());
    CHECK(s.Story().reputation.at("ada_mercer")==2 && s.Story().flags.contains("met_ada"));
    CHECK(s.Event("collect","mail_satchel").value.objectiveChanged);
    CHECK(s.Mission().objective==2 && s.Story().flags.contains("has_satchel"));
    CHECK(!s.Event("collect","mail_satchel").value.accepted);
    CHECK(!s.Event("collect","fake_item").value.accepted);
    CHECK(!s.Event("arbitrary_code","mail_satchel"));
    const auto checkpoint=s.Save();
    CHECK(!checkpoint.empty());
    N::Session loaded;
    CHECK(loaded.Load(content).empty());
    CHECK(loaded.Restore(checkpoint).empty());
    CHECK(loaded.Mission().objective==2 && loaded.Story().reputation==s.Story().reputation);
    CHECK(loaded.Story().credits==0);
    CHECK(loaded.BeginDialogue("ada_return").value.accepted);
    CHECK(loaded.DialogueJson().find("Everything is here.")!=std::string::npos);
    const auto end=loaded.Choose("return_mail");
    CHECK(end && end.value.completed && end.value.creditsAdded==5 && end.value.cutscene=="mercy_delivery");
    CHECK(loaded.Mission().stage==N::Stage::Complete && loaded.Story().credits==5);
    CHECK(loaded.Story().reputation.at("ada_mercer")==5);
    CHECK(loaded.Story().flags.contains("satchel_returned") && loaded.Story().flags.contains("postmark_clue"));
    CHECK(!loaded.Choose("return_mail"));
    CHECK(!loaded.Start().value.accepted && !loaded.Fail().value.accepted);
    CHECK(loaded.BeginDialogue("ada_return"));
    CHECK(loaded.Choices().empty()); // Re-entering cannot farm reputation or rewards.
    CHECK(loaded.CloseDialogue());
    const auto completedSave=loaded.Save();
    for(int i=0;i<10;++i)
    { CHECK(loaded.Restore(completedSave).empty()); CHECK(loaded.Story().credits==5); }
    CHECK(!loaded.Restore(Replace(completedSave,"\"revision\":1","\"revision\":2")).empty());
    CHECK(loaded.Save()==completedSave);
    CHECK(!loaded.Restore(Replace(checkpoint,"mail_satchel","fake_item")).empty());
    CHECK(loaded.Story().credits==5); // Bad replay is atomic.
    CHECK(!loaded.Restore(Replace(checkpoint,"\"action\":\"start\"","\"action\":\"eval\"")).empty());
    CHECK(!loaded.Restore("{\"formatVersion\":1,\"formatVersion\":1}").empty());
    CHECK(!loaded.Load("{}").empty());
    CHECK(loaded.Story().credits==5);

    N::Session failed;
    CHECK(failed.Load(content).empty() && failed.Start());
    CHECK(failed.Fail().value.accepted);
    CHECK(failed.Mission().stage==N::Stage::Failed);
    CHECK(!failed.Event("collect","mail_satchel").value.accepted);
    CHECK(!failed.Start().value.accepted && !failed.Fail().value.accepted);
    CHECK(loaded.Restore(failed.Save()).empty());
    CHECK(loaded.Mission().stage==N::Stage::Failed && loaded.Story().credits==0);

    auto counted=Replace(content,"\"event\": \"collect\",","\"count\": 2, \"event\": \"collect\",");
    N::Session count;
    CHECK(count.Load(counted).empty() && count.Start() && count.BeginDialogue("ada_intro") && count.Choose("accept_delivery"));
    CHECK(count.Event("collect","mail_satchel").value.accepted);
    CHECK(count.Mission().count==1 && count.Mission().objective==1);
    CHECK(!count.Story().flags.contains("has_satchel"));
    CHECK(count.Event("collect","mail_satchel").value.objectiveChanged);
    CHECK(count.Mission().objective==2 && count.Mission().count==0);

    N::Session gated;
    const auto lockedContent=Replace(content,"\"reward\": 5","\"requires\": \"asked_about_town\", \"reward\": 5");
    CHECK(gated.Load(lockedContent).empty());
    CHECK(!gated.Start().value.accepted);
    CHECK(gated.BeginDialogue("ada_intro") && gated.Choose("ask_town"));
    CHECK(gated.Start().value.accepted); // Story decision unlocks mission activation.
    CHECK(gated.CloseDialogue());
    CHECK(gated.Restore(gated.Save()).empty() && gated.Mission().stage==N::Stage::Active);

    N::Session trigger;
    auto triggerContent=Replace(content,"\"event\": \"dialogue\"","\"event\": \"enter\"");
    CHECK(trigger.Load(triggerContent).empty() && trigger.Start());
    CHECK(!trigger.Event("signal","accept_delivery").value.accepted);
    CHECK(trigger.Event("enter","accept_delivery").value.objectiveChanged);
    CHECK(trigger.Story().flags.contains("met_ada"));

    CHECK(!N::ParseDefinition(Replace(content,"\"revision\": 1","\"revision\": 1.5")));
    CHECK(!N::ParseDefinition(Replace(content,"\"event\": \"dialogue\",","\"count\": 2, \"event\": \"dialogue\",")));
    CHECK(!N::ParseDefinition(Replace(content,"\"target\": \"return_mail\"","\"target\": \"accept_delivery\"")));
    CHECK(!N::ParseDefinition(Replace(content,"\"reward\": 5","\"reward\": -1")));
    CHECK(!N::ParseDefinition(Replace(content,"\"reward\": 5","\"reward\": 1000001")));
    CHECK(!N::ParseDefinition(Replace(content,"\"reputationDelta\": 2","\"reputationDelta\": 21")));
    CHECK(!N::ParseDefinition(Replace(content,"\"sets\": \"met_ada\"","\"sets\": \"unknown\"")));
    CHECK(!N::ParseDefinition(Replace(content,"\"next\": \"ada_town\"","\"next\": \"missing\"")));
    CHECK(!N::ParseDefinition(Replace(content,"\"target\": \"accept_delivery\"","\"target\": \"missing\"")));
    CHECK(!N::ParseDefinition(Replace(content,"\"reward\": 5","\"reward\": 5, \"reward\": 6")));
    CHECK(!N::ParseDefinition(Replace(content,"\"reward\": 5","\"exec\": \"bad\", \"reward\": 5")));
    CHECK(!N::ParseDefinition(std::string(1024*1024+1,' ')));
    CHECK(!N::ParseDefinition(std::string(17,'[')+std::string(17,']')));

    N::Session bounded;
    CHECK(bounded.Load(content).empty());
    for(int i=0;i<2048;++i) { CHECK(bounded.BeginDialogue("ada_intro")); CHECK(bounded.CloseDialogue()); }
    const auto full=bounded.Save();
    CHECK(!bounded.Start());
    CHECK(bounded.Mission().stage==N::Stage::NotStarted);
    CHECK(bounded.Restore(full).empty());
    CHECK(bounded.Save()==full);
    std::cout<<checks<<" mission/story assertions passed; delivery rewards once, guarded dialogue, atomic journal restore.\n";
    return 0;
}
