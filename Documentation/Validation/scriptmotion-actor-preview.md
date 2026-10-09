# ScriptMotion Actor preview validation — 2026-10-09

## Base and preservation

Inspected GitHub development at `e243561ae8157f14ac1481d09b4349bc58b577d0`,
including merged PR #1 (initial core/project) and PR #2 (model-space pose API).
The latest commit at that point added the manual Windows workflow. No workflow
file is changed by this feature. A fresh local clone was clean before creating
`feature/scriptmotion-actor-preview`; no existing checkout was replaced.

## Executed checks

Windows x64; CMake 4.4.2; Ninja Multi-Config; MSVC 19.51.36256.0; Windows SDK
10.0.22621.0. The existing installed toolchain was used without a system install.

| Check | Actual result |
| --- | --- |
| ScriptMotion C++20 Debug build | Exit 0 |
| ScriptMotion Debug unit executable | 67 passed, 0 failed (55 existing + 12 bake cases) |
| ScriptMotion Debug CTest | 3/3 passed |
| ScriptMotion C++20 Release build | Exit 0 |
| ScriptMotion Release unit executable | 67 passed, 0 failed |
| ScriptMotion Release CTest | 3/3 passed |
| Courier CLI bake | 241 samples, 7 joints, 120 Hz; valid Ada Mercer event payload |
| Existing Wanted FirstErrand Debug build/CTest | Exit 0; 1/1 passed; 20 mission assertions |
| Python asset verification | 6/6 passed |
| Blender 5.2.1 LTS glTF import and deformation | One skinned mesh, 7 joints, 432 vertices, normalized weights; 2.10 m height |
| C++-sampled courier wave applied to imported skin at 1 second | Visible deformation, maximum displacement about 0.8384 m; bind/wave renders inspected |
| Whitespace / workflow diff check | `git diff --check` clean; no workflow diff |
| ASan/UBSan in this session | Not run; prior Linux results are historical |
| O3DE configuration | Failed before target generation: required zlib and Freetype packages absent in isolated dependency directory |
| O3DE runtime/editor/native tests compilation | Not run successfully; 6 real-engine test cases added, uncompiled/unexecuted |
| O3DE Asset Processor / Editor / GameLauncher | Not launched; Actor product and playback remain unverified |

The offline O3DE probe used the actual engine CMake project:

```powershell
cmake -S . -B <workspace>/build-o3de -G 'Ninja Multi-Config' -DLY_PROJECTS=Projects/Wanted -DLY_3RDPARTY_PATH=<workspace>/o3de-dependencies -DLY_PACKAGE_SERVER_URLS= -DLY_UNITY_BUILD=OFF
```

It reached MSVC/Windows SDK detection, then reported missing
`zlib-1.2.11-rev5-windows` and `freetype-2.11.1-rev1-windows` and stopped with
exit 1. Package downloads were deliberately disabled for this isolated probe;
it does not establish that online dependency provisioning is impossible. Roughly
49 GiB was free at inspection time; no full dependency/LFS download was attempted.
Provisioning the complete engine toolchain and sufficient build storage remains
a prerequisite for native verification.

Sanitized portable logs are included next to this report. The documented commands
in `Gems/ScriptMotion/ACTOR_PREVIEW.md` reproduce the separate portable and native
test boundaries. The Blender check is a source-asset check, **not an O3DE run**.

## Failures found and fixed

- Initial Visual Studio generator configuration hit an inherited duplicate
  `Path`/`PATH` environment error. Ninja with the installed MSVC/SDK succeeded.
  No machine-wide environment setting was changed.
- Initial portable compilation failed with MSVC C4530 under `/WX`: the standard
  library was compiled with exception support while the core disabled it.
  `_HAS_EXCEPTIONS=0` now propagates through the public standalone target.
- An intermediate private-only version of that definition caused a Debug heap
  assertion/access violation on malformed JSON, producing an application dialog.
  Propagating the definition to all consumers fixed the inconsistent STL class
  layouts. The rebuilt Debug and Release suites pass. Failed test processes ended.
- Review of native `MotionInstance::SetCurrentTime` showed it does not clear
  finished loops/frozen state. Seek now calls `ResetTimes` before setting time.
- Paused blend-in could remain invisible. Pause/start-at-zero/seek give the motion
  full weight, and zero-time Actor updates request motion sampling explicitly.
- Distinct double keys could silently collapse to one native float timestamp.
  Baking now rejects those tracks. The final source endpoint is sampled exactly.
- The prior bake limit did not count deep skeleton ancestor traversal. The shared
  bake now includes it in its validation-work estimate.
- The Z-up fixture wave cannot be copied unchanged onto Y-up glTF local joints.
  Matching local-coordinate skeleton/clip files are generated and checked.

Remaining MSVC D9025 command-line warnings report CMake's default `/EHsc` being
overridden for the deliberately exception-free core. No source warnings or
remaining compiler errors were reported by the successful portable builds.

## Source review and merge gates

Reviewed the changes against this checkout's real `MotionData`, `MotionSystem`,
`ActorInstance`, `MotionInstance`, `ActorComponentNotificationBus`, Actor builder,
AssImp scene/transform import, and upstream test-fixture APIs. Checked ownership
ordering, invalid-reload preservation, bounded allocations, coordinate conversion,
license notices and preservation of the existing fixtures/default scene.

This is author source review, not independent approval. Keep the PR as a draft
until the real native targets compile, all six native tests run, and the editor
acceptance procedure in `ACTOR_PREVIEW.md` passes. Obtain reviewer approval before
merging into development. Portable success and Blender renders do not meet those
merge gates. No merge was performed by this work.

## Next slices

1. Provision dependencies/storage, compile and fix native/editor targets, process
   the courier, then verify preview, game playback and lifecycle acceptance.
2. CineScript: one authored camera shot + Ada subtitle + ScriptMotion wave, with
   deterministic seek/cancel and a working O3DE camera binding.
3. MissionScript/StoryScript: use that scene for meet Ada → collect satchel →
   return satchel, with validated narrative state and exactly-once completion.
4. WantedSim: fixed-step, bounded character impulse/recovery and injury state;
   then driven ragdoll joints and optional restrained effects.
5. WantedOn: a real local server/two-client test where the server rejects invalid
   movement/interactions and owns mission/injury outcomes.
6. Outlaw Director: edit and preview the already-working text animation,
   cinematic and mission documents from one editor tool.
7. Mercy Crossing: preserve DefaultLevel; create a new level with third-person
   controls, courier/Ada, satchel, objective UI and cutscene, then verify in the
   standalone launcher. These later features are not implemented by this PR.
