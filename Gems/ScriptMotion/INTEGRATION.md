# ScriptMotion O3DE integration status

The portable core is compiled and tested. The CMake Gem, runtime adapter, EBus and editor component are **implemented source that has not been compiled or run against a complete O3DE build**.

The adapter reads bounded skeleton/clip JSON through O3DE FileIO, validates a matching Actor's bone count/names/parents/bind pose, evaluates sampled poses, fills `EMotionFX::NonUniformMotionData`, creates an `EMotionFX::Motion`, and starts it through the Actor's MotionSystem. Events become native MotionEventTrack/TwoStringEventData entries. Activation/deactivation follows ActorComponentNotificationBus. Stop removes its own live motion instance before releasing its owned motion.

Editor controls configure source paths, loop, speed, blend-in and bake sample rate; enable preview, reload, seek and show validation status. They are authored controls, not a verified live editor demonstration. Runtime EBus exposes Play, Stop, SetPlaybackSpeed, Seek, GetDuration and GetLastError.

## Restrictions

- Exact compatible rig required; no automatic retargeting. Bind translation and rotation are checked and non-unit bind scales rejected.
- Uses simple motion playback; refuses Actors driven by an active Animation Graph and declares incompatible services.
- Core step interpolation is not supported by the initial native adapter; discontinuous segments are explicitly rejected.
- Linear/smoothstep clips are sampled at configurable 30–240 Hz (default 120), plus authored key times. Native float interpolation approximates the portable evaluator between samples.
- Forward speed 0–10, clips up to 600 seconds, bounded bake size and work. Source file access is intended for development; packaged runtime assets need an Asset Processor builder.
- No IK, custom AnimGraph node, direct core-layer bridge, automatic source watcher, verified packaged-game assets or editor automation tests.
- Native event processing has not been proven equivalent to `CollectEvents` across seek/loop boundaries.

## Verify next

Compile both runtime and editor targets with the real O3DE dependencies. Test malformed edit preserving current playback, incompatible rigs, seek without skipped event emission, pause/loop, repeated load/stop, Actor replacement, deletion and component deactivation. Confirm no leaked Motion or stale MotionInstance use. Measure native-versus-core pose error before increasing clip duration or bake budgets.
