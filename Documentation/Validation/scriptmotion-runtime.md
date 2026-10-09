# ScriptMotion runtime verification — 2026-10-09

Development was fetched at `60778a83ddf363e0caf519b643415915c37a3b6e` after PR #3
merged. PRs #1 and #2 are also merged. Started a clean new branch
`feature/scriptmotion-runtime`; preserved the previous local and remote commits.
No workflow changes, upstream pushes, merges or system installation were made.

## Engine gate

Inspected this Windows host: 16 logical CPUs, 15.63 GiB physical RAM, about
1.60 GiB available RAM and 46.65 GiB free storage at inspection. Read-only CIM
inventory was denied, so memory was read with GlobalMemoryStatusEx. GPU readiness
was not verified. The installed MSVC 19.51, Windows SDK 10.0.22621.0, CMake 4.4.2
and Ninja compile the portable targets without system changes.

[O3DE requirements](https://docs.o3de.org/docs/welcome-guide/requirements/)
list 100+ GB free for source configurations and about 2 GB RAM per build thread
in addition to running applications. A full dependency/LFS download and engine
build were deferred because this host currently falls short of that headroom.
This is a capacity decision, not proof that downloading packages is impossible.

Re-ran the actual root configure against current development:

```powershell
cmake -S <repo> -B <work>/build-o3de -G "Ninja Multi-Config" -DLY_PROJECTS=Projects/Wanted -DLY_3RDPARTY_PATH=<work>/o3de-dependencies -DLY_PACKAGE_SERVER_URLS= -DLY_UNITY_BUILD=OFF
```

Exit 1 before target generation: zlib-1.2.11-rev5-windows and
freetype-2.11.1-rev1-windows are absent. This probe explicitly disables downloads.
The engine Python environment is also not provisioned. Asset Processor, native
ScriptMotion/Editor/Wanted targets and six native integration tests remain
uncompiled/unrun. No O3DE screenshots or successful editor result are claimed.

## Compiled and executed

Using the existing workspace-local Ninja Multi-Config build:

```powershell
cmake --build <work>/build-native --config Debug --parallel 2
ctest --test-dir <work>/build-native -C Debug -V
cmake --build <work>/build-native --config Release --parallel 2
ctest --test-dir <work>/build-native -C Release -V
python -m unittest discover -s Gems/ScriptMotion/Examples/Tools -p "test_*asset.py" -v
ctest --test-dir <work>/build-mission -C Debug --output-on-failure
```

- Debug and Release: builds exit 0; 8/8 CTest pass in each.
- Existing evaluator/bake suite: 67 cases pass in each configuration.
- New playback suite: 85 assertions pass, covering owning reload, speed changes,
  pause/stop/seek, loop motion, event budget rollback, completion edges, masked
  additive layers, interrupted fades and split-step displacement equivalence.
- Real authored sequence: 768 frames, 4.8m extracted walk motion, eight footsteps,
  one greeting completion. All three new text clips bake successfully.
- Asset structure: 11 distinct tests pass (six original, five locomotion).
- Existing FirstErrand regression: 1/1 CTest passes (20 assertions).
- Whitespace check clean; no workflows in diff. Author source review completed;
  independent review has not been obtained. Sanitizers not run in this slice.

The first new test run incorrectly expected end-before-start ordering at a loop
boundary; corrected the test to the existing documented source-index tie order.
The runtime did not require an ordering change. MSVC D9025 exception-flag override
warnings remain from the existing standalone setup; no source warnings remain.

## Next

Provision build storage/RAM headroom and official dependencies, hydrate needed
LFS assets, build native targets, process both courier rigs and execute editor
acceptance. Portable Player still needs a native pose/graph integration and an
editor panel. Next independently testable slice: a CineScript runtime using the
existing ScriptMotion APIs, camera bindings and the FirstErrand mission.
