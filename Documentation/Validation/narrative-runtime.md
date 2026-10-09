# MissionScript / StoryScript validation — 2026-10-09

Branch `feature/missionscript` is independently based on development
`60778a83ddf363e0caf519b643415915c37a3b6e`. Existing FirstErrand source and behavior
are preserved. No workflow, scene replacement, upstream push or merge occurred.

## Implementation

`Projects/Wanted/Gem/Include/Wanted/Narrative.h` and `Source/Narrative.cpp` implement
strict authored mission/dialogue loading and an owning combined session with
separate MissionScriptState and StoryScriptState. It supports sequential/countable
objectives, declared flags, guarded branching choices, decisions, reputation,
mission unlock guards, failure/completion, local rewards, UI data, cutscene aliases
and revisioned accepted-command checkpoints. The original Unclaimed Post content
is included in `Assets/Narrative/unclaimed_post.narrative.json`.

The Wanted system-component/EBus source exposes opt-in narrative controls alongside
the original API. `NarrativeCinematic.lua` optionally starts a whitelisted CineScript
scene on completion. It never grants credits again. These native/Lua adapters are
uncompiled and unexecuted; the portable engine-independent session is verified.
Public APIs, schema, bounds and native acceptance are in `Projects/Wanted/NARRATIVE.md`.

## Executed standalone verification

MSVC 19.51.36256.0 / SDK 10.0.22621.0 / CMake 4.4.2 / Ninja Multi-Config:

```powershell
cmake -S Projects/Wanted/Tests -B <work>/build-mission -G "Ninja Multi-Config"
cmake --build <work>/build-mission --config Debug --parallel 2
ctest --test-dir <work>/build-mission -C Debug -V
cmake --build <work>/build-mission --config Release --parallel 2
ctest --test-dir <work>/build-mission -C Release -V
```

Debug and Release build successfully and pass 2/2 CTest each. FirstErrand passes
20 assertions. Narrative passes 4,212 assertions, including 4,096 repeated
journal-capacity operations; this is not a claim of 4,212 separate test scenarios.
Coverage includes wrong-order events, forged dialogue shortcuts, guarded choices,
branch decisions, count objectives, story-based mission unlocking, bounded rewards,
completed/failed state, atomic failed load/restore, corrupted journals, revision
mismatch, duplicate/unknown fields, resource budgets and once-only rewards.

Review caught impossible repeated dialogue objectives (one choice consumed by
multiple objectives, or count >1). The parser now rejects both and tests cover them.
Quest choices are hidden until their matching objective is current, avoiding
consuming a necessary quest choice before the mission starts.

## Combined runtime verification across independent drafts

An exact read-only source snapshot of CineScript local commit
`99c80759552cd370c1b12f0f9733d0b522c9c91e` was extracted into the task workspace.
That commit's tree was verified identical to published PR #5. No branch merge or
reset was used. The optional `WANTED_CINESCRIPT_SOURCE` test parameter points at
that snapshot's CineScript Gem:

```powershell
cmake -S Projects/Wanted/Tests -B <work>/build-combined -G "Ninja Multi-Config" -DWANTED_CINESCRIPT_SOURCE=<snapshot>/Gems/CineScript
cmake --build <work>/build-combined --config Debug --parallel 2
ctest --test-dir <work>/build-combined -C Debug -V
cmake --build <work>/build-combined --config Release --parallel 2
ctest --test-dir <work>/build-combined -C Release -V
```

Both builds succeed and both pass **5/5 CTest**. The combined test has 724
assertions across actual authored mission -> story flags -> CineScript -> the
matching courier ScriptMotion clip. It restores a mission checkpoint, returns
the satchel, passes the completed story flags to CineScript, evaluates 128 wave
poses over 448 updates, observes one acknowledgement/completion, then restores
and skips a cinematic checkpoint without changing the five-credit balance.
It also runs the original mission, narrative and both CineScript tests.

## Limits and remaining gates

No O3DE engine/Editor/Asset Processor/launcher or native bus/Lua test succeeded.
The existing engine configure fails before target generation with absent isolated
zlib/Freetype packages. Full provisioning was deferred with approximately 46.65 GiB
disk and 1.60 GiB available RAM at the session inventory. The sanitizer probe in
the CineScript slice failed because the MSVC x64 ASan runtime library is missing.
No sanitizer, renderer, physics, UI or real network verification is claimed here.

This is single-player trusted-host state. Event callers still validate proximity,
trigger occupancy and inventory. Saved journals are not authenticated and cannot
be accepted as multiplayer authority. Save data replaces the local reward balance;
it is not an external economy transaction. One active mission/chapter definition,
revision matching, once-only choices and bounded journals are intentional current
limits; multiple missions, save migrations, chapter transitions and compaction
remain future work. Native dynamic story-flag transfer to CineScript is pending;
the compiled combined core test passes those flags directly.

Author source review completed; independent review and native compilation/acceptance
remain draft merge gates. No current PR was merged by this session.
