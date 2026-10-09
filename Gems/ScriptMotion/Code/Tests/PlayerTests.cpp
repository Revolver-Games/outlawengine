/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <ScriptMotion/Player.h>
#include <cmath>
#include <iostream>
#include <limits>

namespace SM = Wanted::ScriptMotion;
namespace
{
    int checks = 0;
    bool Near(double a, double b) { return std::isfinite(a) && std::abs(a - b) < 1e-8; }
    SM::Skeleton Rig()
    {
        return {"rig", {{"root", -1, {}}, {"arm", 0, {{0, 2, 0}, {}}}}};
    }
    SM::Clip Motion(double height = 4.0)
    {
        SM::Clip c;
        c.name = "motion"; c.skeletonName = "rig"; c.duration = 1.0;
        c.tracks = {{0, {{0, {}}, {1, {{2, 0, 0}, {}}}}},
                    {1, {{0, {{0, height, 0}, {}}}, {1, {{0, height, 0}, {}}}}}};
        c.events = {{0, "start", "{}"}, {0.5, "step", "{}"}, {1, "end", "{}"}};
        return c;
    }
}
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) { std::cerr << "FAIL line " << __LINE__ << ": " << #__VA_ARGS__ << '\n'; return 1; } } while (false)

int main()
{
    SM::Player p;
    CHECK(!p.Sample());
    CHECK(!p.Advance(0.1));
    CHECK(!p.SetRootMotion(0).empty());
    auto source = Motion();
    CHECK(p.Load(Rig(), source).empty());
    source.tracks.clear(); // The session owns its data.
    CHECK(p.SetRootMotion(0).empty());
    auto f = p.Advance(1.25);
    CHECK(f && Near(p.GetTime(), 1.25));
    CHECK(Near(f.value.rootTranslationDelta.x, 2.5));
    CHECK(Near(f.value.pose.localTransforms[0].translation.x, 0));
    CHECK(Near(f.value.pose.localTransforms[1].translation.y, 4));
    CHECK(f.value.events.size() == 3);
    CHECK(Near(f.value.events[0].timelineSeconds, 0.5));
    CHECK(f.value.events[1].eventIndex == 0 && f.value.events[2].eventIndex == 2);
    CHECK(!f.value.completed);

    p.SetPaused(true);
    CHECK(p.Advance(10) && Near(p.GetTime(), 1.25));
    CHECK(p.Sample().value.events.empty());
    CHECK(p.SetSpeed(2).empty());
    p.SetPaused(false);
    f = p.Advance(0.25);
    CHECK(f && Near(p.GetTime(), 1.75) && Near(f.value.rootTranslationDelta.x, 1.0));
    CHECK(f.value.events.size() == 1 && Near(f.value.events[0].timelineSeconds, 1.5));
    CHECK(p.SetSpeed(0).empty());
    CHECK(p.Advance(1) && Near(p.GetTime(), 1.75));
    CHECK(!p.SetSpeed(-1).empty());
    CHECK(!p.SetSpeed(std::numeric_limits<double>::infinity()).empty());
    CHECK(!p.SetRootMotion(1).empty());
    CHECK(!p.SetRootMotion(5).empty());
    CHECK(!p.Advance(61) && Near(p.GetTime(), 1.75));
    CHECK(!p.Seek(-1) && Near(p.GetTime(), 1.75));
    CHECK(!p.Seek(std::numeric_limits<double>::quiet_NaN()));
    auto bad = Motion(); bad.skeletonName = "wrong";
    CHECK(!p.Load(Rig(), bad).empty() && Near(p.GetTime(), 1.75));
    CHECK(!p.TransitionTo(bad, 1).empty() && Near(p.GetTime(), 1.75));
    CHECK(!p.TransitionTo(Motion(), -1).empty());

    CHECK(p.SetSpeed(1).empty());
    f = p.Seek(0.9);
    CHECK(f && f.value.events.empty() && Near(f.value.rootTranslationDelta.x, 0));
    f = p.Advance(0.2);
    CHECK(f && Near(f.value.rootTranslationDelta.x, 0.4));
    CHECK(f.value.events.size() == 2);
    p.Stop();
    CHECK(p.IsPaused() && Near(p.GetTime(), 0));
    CHECK(p.Advance(1).value.events.empty());

    CHECK(p.Load(Rig(), Motion(), false).empty());
    f = p.Advance(2);
    CHECK(f && f.value.completed && Near(p.GetTime(), 1));
    CHECK(f.value.events.size() == 2); // Zero event is deliberately silent.
    CHECK(!p.Advance(1).value.completed && p.Advance(1).value.events.empty());
    CHECK(p.Seek(0.5));
    CHECK(p.Advance(0.5).value.completed);
    CHECK(p.Seek(10) && Near(p.GetTime(), 1));
    CHECK(!p.Advance(1).value.completed);

    CHECK(p.Load(Rig(), Motion()).empty());
    CHECK(p.TransitionTo(Motion(12), 1).empty());
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 4));
    f = p.Advance(0.25);
    CHECK(f && Near(f.value.pose.localTransforms[1].translation.y, 5.25));
    p.SetPaused(true);
    CHECK(Near(p.Advance(0.5).value.pose.localTransforms[1].translation.y, 5.25));
    p.SetPaused(false);
    CHECK(Near(p.Advance(0.25).value.pose.localTransforms[1].translation.y, 8));
    CHECK(p.TransitionTo(Motion(20), 1).empty()); // Interrupt without a pose jump.
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 8));
    CHECK(Near(p.Advance(1).value.pose.localTransforms[1].translation.y, 20));
    CHECK(p.TransitionTo(Motion(8), 0).empty());
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 8));
    CHECK(p.TransitionTo(Motion(16), 1).empty());
    CHECK(Near(p.Seek(0).value.pose.localTransforms[1].translation.y, 16));

    CHECK(p.Load(Rig(), Motion()).empty());
    SM::Layer layer{Motion(12), 0.5, {0, 1}};
    CHECK(p.SetLayers({layer}).empty());
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 8));
    CHECK(Near(p.Seek(0.5).value.pose.localTransforms[0].translation.x, 1));
    layer.additive = true;
    layer.reference.localTransforms = {Rig().bones[0].bindPose, Rig().bones[1].bindPose};
    CHECK(p.SetLayers({layer}).empty());
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 9));
    layer.mask = {1};
    CHECK(!p.SetLayers({layer}).empty());
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 9));
    CHECK(!p.SetLayers(std::vector<SM::Layer>(5)).empty());
    CHECK(p.SetLayers({}).empty());
    CHECK(Near(p.Sample().value.pose.localTransforms[1].translation.y, 4));

    auto dense = Motion();
    dense.duration = 0.001;
    dense.tracks.clear();
    dense.events = {{0.0005, "burst", "{}"}};
    CHECK(p.Load(Rig(), dense).empty());
    CHECK(!p.Advance(2) && Near(p.GetTime(), 0)); // Atomic event-budget rejection.
    CHECK(p.Advance(0.001).value.events.size() == 1);

    SM::Player split, whole;
    CHECK(split.Load(Rig(), Motion()).empty() && whole.Load(Rig(), Motion()).empty());
    CHECK(split.SetRootMotion(0).empty() && whole.SetRootMotion(0).empty());
    double distance = 0;
    std::size_t events = 0;
    for (int i = 0; i < 10; ++i)
    {
        auto step = split.Advance(0.125);
        CHECK(step);
        distance += step.value.rootTranslationDelta.x;
        events += step.value.events.size();
    }
    auto all = whole.Advance(1.25);
    CHECK(all && Near(distance, all.value.rootTranslationDelta.x) && events == all.value.events.size());
    CHECK(Near(split.GetTime(), whole.GetTime()));
    std::cout << checks << " playback assertions passed\n";
    return 0;
}
