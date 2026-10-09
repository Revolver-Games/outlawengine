# Outlaw Engine build instructions

Repository: https://github.com/Revolver-Games/outlawengine, branch `development`.
Keep work on feature branches and PRs; do not push to the upstream O3DE repository.
Preserve existing changes and inspect the current branch before switching.

## Verified portable targets

A C++20 compiler and CMake are sufficient. The vendored JSON dependency retains
its MIT license. The following commands use an already configured compiler shell:

```powershell
cmake -S Gems/ScriptMotion/Standalone -B build/scriptmotion -G "Ninja Multi-Config"
cmake --build build/scriptmotion --config Debug --parallel 2
ctest --test-dir build/scriptmotion -C Debug --output-on-failure
cmake --build build/scriptmotion --config Release --parallel 2
ctest --test-dir build/scriptmotion -C Release --output-on-failure
cmake -S Projects/Wanted/Tests -B build/mission -G "Ninja Multi-Config"
cmake --build build/mission --config Debug --parallel 2
ctest --test-dir build/mission -C Debug --output-on-failure
python -m unittest discover -s Gems/ScriptMotion/Examples/Tools -p "test_*asset.py" -v
```

Windows MSVC 19.51 + SDK 10.0.22621.0 with CMake 4.4.2/Ninja were executed in
this session: Debug/Release each pass 8/8 ScriptMotion CTest checks; the original
mission passes 1/1; asset checks pass 11/11. See the dated validation reports.
For a single-config Linux build use `-G Ninja -DCMAKE_BUILD_TYPE=Debug` and omit
`--config`/`-C`. New runtime changes have not been tested on Linux this session.

`scriptmotion_demo` evaluates/bakes a clip; `scriptmotion_player` executes the
idle/walk/greeting sequence. See `Gems/ScriptMotion/PLAYER.md` for arguments and
the distinction between portable playback and native animation.

## Full O3DE build — still unverified

Read the [official requirements](https://docs.o3de.org/docs/welcome-guide/requirements/)
and [Windows source setup](https://docs.o3de.org/docs/welcome-guide/setup/setup-from-github/building-windows/).
Source builds require substantial package/LFS/build storage (100+ GB free in the
published requirements) and about 2 GB RAM per compile thread beyond applications.
This machine had only 46.65 GiB disk and 1.60 GiB available RAM at inspection.

With sufficient resources and an installed supported Visual Studio/SDK:

```powershell
git lfs install --local
git lfs pull
# Standard upstream setup writes the user's .o3de/Python cache:
python/get_python.bat
cmake -S . -B build/windows -G "Visual Studio 18 2026" -A x64 -DLY_PROJECTS=Projects/Wanted -DLY_3RDPARTY_PATH=<workspace>/Packages
cmake --build build/windows --config profile --target ScriptMotion ScriptMotion.Editor ScriptMotion.Native.Tests Editor AssetProcessor Wanted.GameLauncher --parallel 1
```

Those full setup/build commands are instructions, not successful results. Choose
the generator matching the installed VS version (2022 uses `Visual Studio 17 2022`).
The standard engine Python setup modifies a user cache outside this task's
workspace; do not modify system settings or replace a user's existing environment.
Use explicit permission where required by the current workspace constraints.

Current isolated root configure fails before target generation because zlib and
Freetype packages are absent. Downloads were disabled in that bounded probe; no
claim that online provisioning is impossible is made. Native runtime/editor/test
compilation, Asset Processor and launcher have not succeeded here.

After native compilation, run the ScriptMotion native tests using O3DE's registered
CTest target, process the source glTF in Asset Processor and follow
`Gems/ScriptMotion/ACTOR_PREVIEW.md`. Verify visible playback in Editor and launcher.
DefaultLevel remains the original template; it is not the completed Mercy Crossing
playable scene. No workflow changes are needed for these local commands.
Only switch to `wanted-development` after the delivered commits have been published or restored from the milestone bundle. If the checkout already exists, inspect it instead of cloning over it. Do not reset, clean, force-push, or overwrite unrelated work. The source uses LFS; no Git submodules exist at the pinned base. There is no reason to invent or initialize unrelated submodules.

The development base is `e0315902af47533267f273835b99d28b96c76e32` from O3DE's `development` branch. Preserve that base when applying an incremental milestone bundle. The initial container clone used `GIT_LFS_SKIP_SMUDGE=1`; only DefaultProject template binary assets were hydrated. A full editor build needs the relevant remaining upstream LFS assets and O3DE packages.

## Full O3DE / Wanted build (not verified)

Official requirements and setup:

- https://www.docs.o3de.org/docs/welcome-guide/requirements/
- https://www.docs.o3de.org/docs/welcome-guide/setup/setup-from-github/
- https://www.docs.o3de.org/docs/welcome-guide/setup/setup-from-github/building-windows/

The official source-build guidance calls for 100+ GB free storage. Use supported Visual Studio/MSVC, the C++ desktop/game-development workloads, Windows SDK, Git LFS and CMake. CMake 3.30+ is recommended by current docs; Visual Studio 2026 requires 4.2+. The checked-out root CMakeLists still accepts 3.25. The provided Windows preset targets Visual Studio 2022. Linux engine builds require upstream-supported Clang and system dependencies; successful GCC core tests are not proof of an O3DE engine build.

**Workspace permission boundary:** upstream `python/get_python.bat` and its wrapper use `%USERPROFILE%/.o3de/Python` for the engine Python environment. That is outside `C:\WantedDevelopment`. The user restricted external changes, so get explicit approval for this standard O3DE cache before running first-time setup; do not redirect system `HOME` or `USERPROFILE` to evade the restriction. Engine/project registration also writes a user manifest; the supplied CMake presets avoid that registration requirement through a relative `CMAKE_MODULE_PATH`.

After dependency installation and approval for the standard Python cache, run from the engine checkout:

```powershell
$env:LY_3RDPARTY_PATH = 'C:\WantedDevelopment\Packages'
.\python\get_python.bat
Set-Location Projects\Wanted
cmake --preset wanted-windows
cmake --build --preset wanted-windows-profile --parallel 2
```

The presets keep downloaded third-party packages in `WantedDevelopment/Packages` and build outputs inside the project. The full build must compile `ScriptMotion`, `ScriptMotion.Editor`, `Wanted`, `Editor`, and `Wanted.GameLauncher`. Read the first compiler error, correct the adapter against actual upstream APIs, then rebuild. Do not declare editor preview or game behavior verified until the executable runs with processed assets and a compatible Actor.

See `Projects/Wanted/FIRST_SCENE.md` for the first real playable scene task. The shipped example skeleton is JSON data, not a skinned Actor asset. No automatic retargeting or proprietary character assets are included.

## CineScript portable verification (2026-10-09)

On an installed MSVC developer shell:

```powershell
cmake -S Gems/CineScript/Standalone -B build/cinescript -G "Ninja Multi-Config"
cmake --build build/cinescript --config Debug --parallel 2
ctest --test-dir build/cinescript -C Debug --output-on-failure
cmake --build build/cinescript --config Release --parallel 2
ctest --test-dir build/cinescript -C Release --output-on-failure
```

Both configurations actually passed 2/2 tests on Windows, including 97 assertions
and the mission/cinematic/animation integration. The native Gem/Editor is still
unverified. Full results and the unsuccessful sanitizer probe are in
`Documentation/Validation/cinescript-runtime.md`. Earlier environment statements
above are historical; updated complete build guidance is also in ScriptMotion PR #4.
