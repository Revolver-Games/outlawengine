# Wanted Engine build instructions

## Tested portable milestone

The ScriptMotion core and Wanted mission tests need a C++20 compiler and CMake. Ninja is optional; omit `-G Ninja` to use an installed native generator. They do not require the full O3DE SDK, graphics hardware, or network access. ScriptMotion's JSON dependency is vendored with its MIT license.

From the engine repository root:

```sh
cmake -S Gems/ScriptMotion/Standalone -B build/wanted-scriptmotion -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/wanted-scriptmotion --parallel 2
ctest --test-dir build/wanted-scriptmotion --output-on-failure

cmake -S Projects/Wanted/Tests -B build/wanted-mission -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/wanted-mission --parallel 2
ctest --test-dir build/wanted-mission --output-on-failure

build/wanted-scriptmotion/scriptmotion_demo Gems/ScriptMotion/Examples/wanted_test_skeleton.json Gems/ScriptMotion/Examples/frontier_wave.scriptmotion.json 1.0
```

The demo prints evaluated bone transforms and animation event occurrences. It is a command-line animation-data demonstration, not a rendered/skinned character.

For Visual Studio 2022, replace the generator with `-G "Visual Studio 17 2022" -A x64`, build with `--config Debug`, and run CTest with `-C Debug`. The demo executable is then `build/wanted-scriptmotion/Debug/scriptmotion_demo.exe`. Windows compilation is not yet verified.

Address/undefined-behavior checking on supported GCC/Clang:

```sh
cmake -S Gems/ScriptMotion/Standalone -B build/wanted-scriptmotion-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRIPTMOTION_SANITIZERS=ON
cmake --build build/wanted-scriptmotion-asan --parallel 2
ctest --test-dir build/wanted-scriptmotion-asan --output-on-failure
```

In this restricted container, LeakSanitizer cannot inspect `/proc/2/task`. The recorded successful ASan/UBSan run therefore used `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. This does not verify memory leaks.


## Windows source checkout

Windows was not accessible in this session. No Windows hardware or installation claim is made. Run the read-only inventory from the intended checkout:

```powershell
python scripts/wanted/preflight.py --workspace C:\WantedDevelopment --output C:\WantedDevelopment\Documentation\windows-preflight.json
```

The GitHub connection identified `arOSProject`; fork creation did not complete. Once an actual `arOSProject/WantedEngine` fork exists and its upstream relationship has been verified, these are the intended commands (not commands already executed here):

```powershell
New-Item -ItemType Directory -Force C:\WantedDevelopment
Set-Location C:\WantedDevelopment
git clone https://github.com/arOSProject/WantedEngine.git WantedEngine
Set-Location WantedEngine
git remote add upstream https://github.com/o3de/o3de.git
git lfs install --local
git switch wanted-development
git lfs pull
```

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
