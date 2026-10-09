# ScriptMotion core verification

The standalone test executable contains 50 named checks. It has no external test-framework dependency. `ScriptMotionTests --list` lists the checks; passing a substring such as `events.` runs only that group. A failure produces a nonzero exit status.

The tests verify real JSON parsing and rejection, schema versions and unknown fields, duplicate JSON keys, Unicode and malformed input, resource limits, skeleton relationships, bind-pose fallback, exact key boundaries, linear/step/smoothstep interpolation, quaternion shortest paths, playback timing, event interval semantics, multiple loops, atomic event limits, masked pose blending, and additive rotation order. Expected transforms use independently specified values, including known quaternion products and the exact smoothstep value at one quarter of a segment.

## Verified on 2026-10-08

- GCC 13.3.0 on Linux, C++20.
- Direct build with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`: original 48 checks passed. The subsequent CMake build includes two additional parser robustness checks.
- CMake 3.31/Ninja Debug build: 50 checks passed. The core compiles with `-fno-exceptions` and warnings treated as errors.
- CTest: 2/2 passed (`ScriptMotionCoreTests` and the example-file evaluator).
- AddressSanitizer and UndefinedBehaviorSanitizer: 50 checks passed with no findings, using `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
- LeakSanitizer was attempted and could not run because this container denies access to `/proc/2/task`. Leak checking remains unverified; disabling leak detection above preserves the address and undefined-behavior checks.

The standalone results do not establish that the O3DE Gem, EMotion FX adapter, editor preview, or Windows target compile or run. Those require the full engine dependencies and separate integration validation.

## Commands

From the engine repository, with CMake and Ninja on `PATH`:

```sh
cmake -S Gems/ScriptMotion/Standalone -B ../build-scriptmotion-qa -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ../build-scriptmotion-qa -j 2
ctest --test-dir ../build-scriptmotion-qa --output-on-failure
../build-scriptmotion-qa/ScriptMotionTests

cmake -S Gems/ScriptMotion/Standalone -B ../build-scriptmotion-coreasan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRIPTMOTION_SANITIZERS=ON
cmake --build ../build-scriptmotion-coreasan -j 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ../build-scriptmotion-coreasan/ScriptMotionTests
```

Enable leak detection on a host that supports it. The `detect_leaks=0` setting is specific to the restricted verification container.
