# Wanted Engine progress — 2026-10-08

## Repository and environment

- Genuine upstream `https://github.com/o3de/o3de.git` cloned at `e0315902af47533267f273835b99d28b96c76e32` (upstream development commit dated 2026-10-08).
- Local branch `wanted-development`; remote `upstream` preserved. Clone is shallow, with real upstream source and ancestry at the selected base, not an empty stand-in repository.
- Git LFS 3.4.1 configured locally. Full LFS smudging deliberately deferred. DefaultProject template resources were selectively fetched: 34 objects, about 1.7 MB. No submodule entries at this base.
- Verified genuine O3DE forks: `Revolver-Games/outlawengine` and `arOSProject/outlawengine`. GitHub App installation now permits publication. The personal `wanted-development` branch is being published for a pull request into the organization fork; check GitHub for current PR/merge status.
- Full O3DE/editor build not attempted because source-build storage requirements exceed capacity and required engine toolchain/runtime dependencies are absent. Successful GCC portable tests do not establish O3DE compatibility.

## Implemented and tested

**ScriptMotion core**: strict JSON skeleton and clip parsing, version/field validation, hierarchy and rig matching, bone-local translation/rotation, step/linear/smoothstep timing, normalized shortest-path quaternion SLERP, bind-pose fallback, looping/clamping/speed, bounded ordered animation event collection, masked blend and additive pose layering. JSON input, depth, data counts, timing and generated events are bounded. Bad inputs return errors without C++ exceptions.

Original data-only test skeleton and frontier wave example plus command-line evaluator are included. The evaluator prints poses and event data; it does not render a character.

**Wanted mission foundation**: generated actual DefaultProject and EngineFinder source templates, preserved upstream notices, enabled ScriptMotion, and added `Unclaimed Post`, a fixed mission state machine with valid sequence and exactly-once completion. Its O3DE EBus/Lua adapters are source only pending a full build.

| Verification | Result |
| --- | --- |
| Portable C++20 core, Debug and Release configurations | Compiled |
| Independent ScriptMotion tests | 51 passed, 0 failed |
| Standalone CTest including example JSON | 2/2 passed |
| AddressSanitizer + UndefinedBehaviorSanitizer | Latest core/CLI CTest 2/2 passed, no findings |
| LeakSanitizer | Unavailable: container `/proc/2/task` access restriction; successful runs disabled leak detection |
| Wanted mission CTest | 1/1 passed, 20 transition assertions |
| Generated project JSON/prefab and O3DE metadata checks | Passed source validation |
| Full O3DE Gem/editor compilation | Not verified |
| Editor playback / playable level | Not verified; scene not wired |
| Windows compilation and hardware | Not accessible / not verified |

Sanitized test outputs are in `Documentation/Validation`. The ASan/UBSan run uses `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

QA found and corrected a portability risk at enormous loop ordinals: MSVC uses double precision for `long double`. Loop timelines above the defined precision budget are now rejected, and a regression test covers the case without confusing it with the event-count budget.

## Implemented, not engine-verified

ScriptMotion Gem metadata/build targets, EMotion FX native motion bake/playback, strict Actor rig compatibility, native event conversion, ownership-aware stop/lifecycle handling, runtime scripting bus, and reflected editor reload/preview/seek controls. Actual upstream declarations were inspected. Source inspection does not replace a successful compiler, editor, runtime or memory-lifecycle test.

Native playback approximates curves via bounded float samples and explicitly rejects discontinuous step interpolation. Only the portable evaluator supports all three timing modes. Actor retargeting, Animation Graph integration, scale tracks, IK, root motion, a source asset builder and packaged runtime assets remain unsupported. Reading raw development JSON is not a finished import pipeline.

## Not implemented

Complete CineScript, MissionScript and StoryScript authored runtimes; playable level/player/NPC setup; injury physiology, active ragdolls, physics reactions, blood/effects; open-world/horse/NPC/wildlife/weather/inventory/save systems; multiplayer replication, servers, profiles, economy or anti-cheat. `WantedOn/README.md` is architecture, not a running online mode.

## Completion boundaries

The first milestone delivers real portable C++ functionality and a genuine starting O3DE game project. It does not complete the engine/game request. Next gates are publication review, Windows/full-toolchain access, real Gem compilation, and a visible Actor/mission demo. See `NEXT_SESSION.md` and `ROADMAP.md`.
