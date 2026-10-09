# Standalone ScriptMotion core

This C++20 target compiles the same parser and pose evaluator used by the ScriptMotion Gem. It does **not** compile O3DE or validate the Gem's EMotion FX integration.

From the repository root:

```sh
cmake -S Gems/ScriptMotion/Standalone -B build-scriptmotion -DCMAKE_BUILD_TYPE=Debug
cmake --build build-scriptmotion --parallel 2
ctest --test-dir build-scriptmotion --output-on-failure
./build-scriptmotion/scriptmotion_demo Gems/ScriptMotion/Examples/wanted_test_skeleton.json Gems/ScriptMotion/Examples/frontier_wave.scriptmotion.json 1.0
```

With Visual Studio, add `--config Debug` to the build command, add `-C Debug` to CTest, and run `build-scriptmotion/Debug/scriptmotion_demo.exe`. CMake 3.22+ and a C++20 compiler are required. The standalone target has no runtime downloads or O3DE package dependencies.

The `ScriptMotionCore` target builds with exceptions disabled. The public header does not expose the privately vendored JSON library. Invalid JSON, malformed schemas, invalid skeletons, invalid poses, and out-of-range timing produce `Result::error`. Files are capped at 8 MiB, input nesting at 32 levels, bones at 1,024, keys per track at 8,192, keys per clip at 100,000, and events at 4,096. Loop timelines are limited to 10^12 cycles to preserve boundary-counting precision on MSVC as well as GCC/Clang. Event collection has a separate configurable budget (default 1,024; maximum 65,536), checked before materializing repeated events.

Optional Linux GCC/Clang AddressSanitizer + UndefinedBehaviorSanitizer build:

```sh
cmake -S Gems/ScriptMotion/Standalone -B build-scriptmotion-asan -DCMAKE_BUILD_TYPE=Debug -DSCRIPTMOTION_SANITIZERS=ON
cmake --build build-scriptmotion-asan --parallel 2
ctest --test-dir build-scriptmotion-asan --output-on-failure
```

The standalone core evaluates exact authored interpolation. Engine playback is a separate adapter. `EvaluatePose` deliberately revalidates the mutable public data structures on each call in this first milestone; replacing that with an immutable validated clip cache is a future performance task. Memory exhaustion is not converted to a recoverable parse error. Reverse playback, YAML, IK, world-space retargeting, root-motion extraction, visual editor preview, and per-component translation/rotation curve times are not implemented here.
