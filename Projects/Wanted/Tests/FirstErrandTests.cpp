/*
 * Copyright (c) Contributors to the Wanted Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <Wanted/FirstErrand.h>
#include <cstdlib>
#include <iostream>

namespace
{
    int checks = 0;
    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using Mission = Wanted::FirstErrand;
    Mission mission;
    Require(mission.GetStage() == Mission::Stage::NotStarted, "mission begins inactive");
    Require(mission.Interact("ada_mercer") == Mission::Transition::Ignored, "cannot interact before start");
    Require(mission.Start(), "first start succeeds");
    Require(!mission.Start(), "duplicate start cannot reset progress");
    Require(mission.GetStage() == Mission::Stage::MeetAda, "first objective is meeting Ada");
    Require(mission.Interact("mail_satchel") == Mission::Transition::Ignored, "cannot skip introduction");
    Require(mission.Interact("") == Mission::Transition::Ignored, "empty target rejected");
    Require(mission.Interact("unknown") == Mission::Transition::Ignored, "unknown target rejected");
    Require(mission.Interact("ada_mercer") == Mission::Transition::ObjectiveChanged, "meeting Ada advances objective");
    Require(mission.GetObjective() == "Recover the mail satchel beside the water tower.", "objective text follows stage");
    Require(mission.Interact("ada_mercer") == Mission::Transition::Ignored, "repeat dialogue does not complete mission");
    Require(mission.Interact("mail_satchel") == Mission::Transition::ObjectiveChanged, "satchel advances objective");
    Require(mission.GetStage() == Mission::Stage::ReturnToAda, "delivery requires return");
    Require(mission.Interact("mail_satchel") == Mission::Transition::Ignored, "satchel cannot be collected twice");
    Require(mission.Interact("ada_mercer") == Mission::Transition::Completed, "return emits completion result");
    Require(mission.GetStage() == Mission::Stage::Complete, "mission is complete");
    Require(mission.Interact("ada_mercer") == Mission::Transition::Ignored, "completion cannot repeat");
    Require(!mission.Start(), "start cannot erase completed mission");
    mission.Reset();
    Require(mission.GetStage() == Mission::Stage::NotStarted, "explicit reset clears progress");
    Require(mission.Start(), "a fresh demonstration can begin after reset");
    std::cout << checks << " mission assertions passed\n";
    return EXIT_SUCCESS;
}
