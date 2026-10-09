/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <CineScript/Timeline.h>
#include <cmath>
#include <iostream>
#include <limits>

namespace CS = Wanted::CineScript;
int checks = 0;
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) { std::cerr << "FAIL " << __LINE__ << ": " << #__VA_ARGS__ << '\n'; return 1; } } while (false)
bool Near(double a, double b) { return std::isfinite(a) && std::abs(a - b) < 1e-8; }
const char* Fixture = R"({"formatVersion":1,"id":"test","revision":2,"duration":4,
 "targets":[{"id":"ada","kind":"actor"},{"id":"wide","kind":"camera"},{"id":"close","kind":"camera"}],
 "tracks":[{"target":"ada","keys":[{"time":0,"translation":[0,0,0]},{"time":2,"translation":[2,0,0]}]}],
 "events":[{"time":0,"type":"camera","target":"wide"},{"time":0,"type":"music","value":"intro"},
 {"time":1,"type":"dialogue","target":"ada","value":"Howdy","duration":1},
 {"time":1,"type":"animation","target":"ada","value":"wave","duration":2},
 {"time":2,"type":"camera","target":"close"},{"time":2,"type":"signal","value":"reward","if":"allowed"},
 {"time":4,"type":"signal","value":"finish"}]})";
int main()
{
    auto scene = CS::ParseScene(Fixture);
    CHECK(scene);
    CS::Timeline t;
    CHECK(!t.Sample() && !t.Start());
    CHECK(t.Load(scene.value, {"allowed"}).empty());
    auto start = t.Start();
    CHECK(start && start.value.started && start.value.gameplayLocked);
    CHECK(start.value.camera == "wide" && start.value.cues.size() == 1);
    CHECK(!t.Start());
    auto f = t.Advance(1.5);
    CHECK(f && Near(f.value.placements[0].transform.translation.x, 1.5));
    CHECK(f.value.subtitle == "Howdy" && f.value.speaker == "ada");
    CHECK(f.value.animations.size() == 1 && Near(f.value.animations[0].time, 0.5));
    t.Pause(true);
    CHECK(Near(t.Advance(1).value.time, 1.5));
    CHECK(!t.Advance(-1) && !t.Advance(std::numeric_limits<double>::quiet_NaN()));
    CHECK(!t.Seek(5) && Near(t.Sample().value.time, 1.5));
    t.Pause(false);
    auto checkpoint = t.SaveCheckpoint();
    CHECK(!checkpoint.empty());
    CS::Timeline restored;
    CHECK(restored.Load(scene.value).empty());
    CHECK(restored.RestoreCheckpoint(checkpoint).empty());
    CHECK(restored.IsPlaying() && restored.Sample().value.cues.empty());
    f = restored.Advance(0.5);
    CHECK(f && f.value.camera == "close" && f.value.subtitle.empty());
    CHECK(f.value.cues.size() == 1 && f.value.cues[0].value == "reward");
    CHECK(restored.Advance(0).value.cues.empty());
    CHECK(!restored.RestoreCheckpoint("{}").empty());
    CHECK(Near(restored.Sample().value.time, 2));
    auto other = scene.value; ++other.revision;
    CS::Timeline mismatch;
    CHECK(mismatch.Load(other).empty());
    CHECK(!mismatch.RestoreCheckpoint(checkpoint).empty());
    CHECK(!restored.Load(scene.value, {"x", "x"}).empty());
    CHECK(Near(restored.Sample().value.time, 2));
    f = restored.Advance(10);
    CHECK(f && f.value.completed && !f.value.gameplayLocked && Near(f.value.time, 4));
    CHECK(f.value.cues.size() == 1 && f.value.cues[0].value == "finish");
    CHECK(restored.Advance(1).value.cues.empty() && !restored.Advance(1).value.completed);
    CHECK(!restored.Skip());
    CHECK(restored.RestoreCheckpoint(restored.SaveCheckpoint()).empty());
    CHECK(!restored.IsPlaying());

    CHECK(t.Load(scene.value).empty());
    CHECK(t.Start());
    CHECK(t.Seek(1.5).value.cues.empty());
    CHECK(t.Advance(0.5).value.cues.empty()); // Conditional cue disabled.
    CHECK(t.Seek(0.5));
    f = t.Skip();
    CHECK(f && f.value.completed && f.value.cues.empty() && !f.value.gameplayLocked);
    CHECK(Near(f.value.placements[0].transform.translation.x, 2));
    t.Stop();
    CHECK(!t.IsPlaying() && Near(t.Sample().value.time, 0));
    CHECK(t.Start());
    CHECK(t.Seek(4) && !t.IsPlaying()); // Silent seek to end releases control.
    CHECK(!t.Advance(1).value.completed);
    CHECK(t.Start());
    t.Pause(true);
    CHECK(restored.RestoreCheckpoint(t.SaveCheckpoint()).empty() && restored.IsPaused());
    CHECK(Near(restored.Advance(1).value.time, 0));

    CHECK(!CS::ParseScene("{\"formatVersion\":1,\"formatVersion\":1}"));
    CHECK(!CS::ParseScene("[1,2]"));
    CHECK(!CS::ParseScene(std::string(1024 * 1024 + 1, ' ')));
    CHECK(!CS::ParseScene(std::string(17, '[') + std::string(17, ']')));
    std::string unknown(Fixture); unknown.insert(1, "\"evil\":1,");
    CHECK(!CS::ParseScene(unknown));
    auto bad = scene.value; bad.targets.push_back(bad.targets.front());
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.tracks[0].keys[0].time = 0.1;
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.events[0].target = "ada";
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.events[3].value = "../../arbitrary/file";
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.events[1].time = -1;
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.events[2].duration = 10;
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.events.insert(bad.events.begin()+3, bad.events[2]);
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.events[3].kind = static_cast<CS::EventKind>(100);
    CHECK(!CS::ValidateScene(bad).empty());
    bad = scene.value; bad.duration = std::numeric_limits<double>::infinity();
    CHECK(!t.Load(bad).empty());
    CHECK(t.GetScene().id == "test");

    CS::Timeline split, whole;
    CHECK(split.Load(scene.value, {"allowed"}).empty());
    CHECK(whole.Load(scene.value, {"allowed"}).empty());
    CHECK(split.Start() && whole.Start());
    std::size_t count = 0, completed = 0;
    for (int i = 0; i < 32; ++i)
    {
        auto step = split.Advance(0.125);
        CHECK(step);
        count += step.value.cues.size(); completed += step.value.completed ? 1u : 0u;
    }
    auto all = whole.Advance(4);
    CHECK(all && count == all.value.cues.size() && completed == 1);
    CHECK(split.Sample().value.camera == all.value.camera);
    std::cout << checks << " CineScript assertions passed\n";
    return 0;
}
