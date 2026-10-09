# WANTED

Original Western action-adventure project for Wanted Engine. The C++ project was
generated with O3DE's actual `DefaultProject` template, including its inherited
`EngineFinder` template, at upstream revision
`e0315902af47533267f273835b99d28b96c76e32`. Upstream copyright notices and licensing
are retained. The engine association deliberately remains `o3de` for compatibility.

## What exists

- Complete generated project metadata, source Gem, launcher/build configuration,
  platform files, shader configuration and default level prefab.
- ScriptMotion enabled as a project Gem through a relative source dependency.
- A small C++ mission named **Unclaimed Post**, with independently compiled and
  tested progression rules.
- An O3DE request/notification EBus adapter and Lua proximity/input bridge for
  that mission. These integration sources have not yet been compiled or launched
  in O3DE.

`Levels/DefaultLevel/DefaultLevel.prefab` is the unchanged upstream template
scene. It has **not** been verified in the editor and is **not yet a playable
WANTED level**. No controllable player, animated NPC or mission entities have been
wired into this prefab. See `FIRST_SCENE.md` for the next concrete scene task.

The project is kept inside the engine fork so one branch versions both engine
extensions and the game. The intended Windows path is
`C:\WantedDevelopment\WantedEngine\Projects\Wanted`.

## Original premise

The fictional Redwater Basin, 1887. Courier Ellis Vale arrives at Mercy Crossing
with a sack of undeliverable letters. Relief shipments are disappearing while
their recipients are listed as having collected them. Station keeper Ada Mercer
recognizes a postmark that should not exist. Ellis must trace the paperwork and
decide which communities to trust before a manufactured debt empties the basin.

**Unclaimed Post** is the first small gameplay demonstration: meet Ada, retrieve
the satchel at the water tower, and return it. Its current implementation is a
fixed state machine. It is not the promised future MissionScript/StoryScript
parser, branching story engine, save system or reward economy.

## Build and verify

Read the engine-root `BUILD_INSTRUCTIONS.md` before a full build. With the
official engine dependencies installed, project-local presets select the engine
through its relative `cmake` path rather than a machine-specific registration.

On Windows, from this directory:

```powershell
cmake --preset wanted-windows
cmake --build --preset wanted-windows-profile
```

The Linux preset is `wanted-linux` / `wanted-linux-profile` and requires the
upstream-supported Clang toolchain. These full-engine presets have been parsed
by CMake but have **not** completed a full configure/build in this session.

The mission rules have no O3DE binary dependency. From the engine root:

```sh
cmake -S Projects/Wanted/Tests -B Projects/Wanted/build/mission-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Projects/Wanted/build/mission-tests
ctest --test-dir Projects/Wanted/build/mission-tests --output-on-failure
```

Verified in the initial development environment with GNU C++ 13.3, C++20 and
warnings-as-errors: `Wanted.FirstErrand` passed, with 20 assertions covering
ordered progression, unknown targets, duplicate interactions, one-time
completion, and explicit restart. This verifies the mission rules only, not
EBus reflection, input events, Lua execution, entity placement or rendering.

## ScriptMotion examples

The canonical original test skeleton and wave are in
`../../Gems/ScriptMotion/Examples/`. Their JSON skeleton is a **data-only rig**;
it is not an imported/skinned EMotion FX Actor asset. Do not describe the
standalone evaluator's pose output as an animated character demonstration.

The project enables the ScriptMotion Gem; its runtime/editor adapter must still
be compiled and tested with a matching Actor. Refer to the Gem's examples and
the engine-root progress notes for what has been verified.

## Mission runtime interface

`WantedRequestBus` exposes `StartFirstErrand`, `ResetFirstErrand`,
`InteractWithMissionTarget`, `GetFirstErrandStage` and
`GetFirstErrandObjective`. Valid target keys are `ada_mercer` and `mail_satchel`.
Start is idempotent: repeated start calls do not discard progress. A fresh
demonstration requires explicit reset. Interactions in the wrong state return
false. A successful transition broadcasts `OnFirstErrandObjectiveChanged`; the
last transition also broadcasts `OnFirstErrandCompleted` exactly once until
reset.

`Scripts/FirstErrandInteraction.lua` starts a fresh mission on activation and
maps an `Interact` action to the current objective's target when the Player is
within 2.5 meters. Objective/dialogue output currently goes to the debug log.
Only one mission-controller script should be placed in the scene. Its EBus is
for trusted local scripts and has no networking or server-authority guarantee.
Mission progress is in memory only.

## Template generation record

Generation used `scripts/o3de/o3de/engine_template.py`'s public
`create_project` function with `project_name="Wanted"`, `version="0.1.0"`,
`keep_license_text=True`, `keep_restricted_in_project=True`, and
`no_register=True`. The project ID is
`{F648982C-5C58-4B30-907A-7A35E1DA7AF7}`. A temporary manifest directory isolated
the generator from user-wide settings. This source-only environment supplied
the generator's import-time resolver dependency from installed pip; normal
O3DE setup should use the engine's pinned Python requirements.

Only the DefaultProject template's LFS files were fetched for generation: 34
objects totaling about 1.7 MB. There are no unresolved LFS pointer contents in
the generated project working files. This does not hydrate the rest of the
engine or the level's external asset dependencies.
