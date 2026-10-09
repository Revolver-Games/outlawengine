# CineScript runtime validation — 2026-10-09

Branch `feature/cinescript` starts independently from development
`60778a83ddf363e0caf519b643415915c37a3b6e`. ScriptMotion runtime PR #4 is a separate
draft and is not required by this branch. PR #3 was verified merged before work.

## What changed

- `Gems/CineScript`: bounded strict JSON parser, owning timeline, camera/actor
  tracks, dialogue/animation snapshots, conditional cues, transport and checkpoints.
- Real O3DE component/module/CMake integration, reflected entity bindings,
  camera/Transform/ScriptMotion APIs, DebugDraw subtitles and notification EBuses.
- `Projects/Wanted/Assets/CineScript/mercy_delivery.cinescript.json`: original
  two-character scene. `DeliveryCinematic.lua` starts it from FirstErrand completion.
- Wanted project enables CineScript. Existing levels and mission rules are preserved.
- ScriptMotion request API gains GetPlaybackSpeed and idle speed configuration
  so cinematic transport can restore the user's speed after a motion disappears.
  One additional native test was added; it has not run.

Public APIs, schema, limits, native setup and cleanup contract are documented in
`Gems/CineScript/README.md`. Audio/music cues are aliases for host listeners, not
audible recordings. The native component is an implementation awaiting compilation;
the portable timeline is the compiled/tested portion.

## Exact executed checks

MSVC 19.51.36256.0, Windows SDK 10.0.22621.0, CMake 4.4.2, Ninja Multi-Config:

```powershell
cmake -S Gems/CineScript/Standalone -B <work>/build-cinescript -G "Ninja Multi-Config"
cmake --build <work>/build-cinescript --config Debug --parallel 2
ctest --test-dir <work>/build-cinescript -C Debug -V
cmake --build <work>/build-cinescript --config Release --parallel 2
ctest --test-dir <work>/build-cinescript -C Release -V
```

Both builds exit 0; both CTest runs pass 2/2. The timeline suite passes 97
assertions including malformed input, duplicate fields, oversize/deep data,
unknown bindings/types, overlaps, failed-load preservation, conditional cues,
checkpoint mismatch, pause/seek/skip and exactly-once completion. Split updates
and a single update deliver the same ordered signal count and final camera.

The compiled Mercy Crossing test runs the existing FirstErrand state machine,
rejects collecting before meeting Ada, completes the return once, executes two
camera cuts and two dialogue lines, evaluates 128 actual ScriptMotion wave poses
over 448 timeline updates, sends one conditional acknowledgement and releases the
timeline control lock. This is a headless test, not rendered gameplay or audio.

Development-base ScriptMotion regression after adapter edits: Debug rebuilt,
3/3 CTest passed (67 existing core cases plus example/bake). The new portable Player
from PR #4 is intentionally absent from this independently based branch.

## Unsuccessful / unverified checks

Attempted MSVC AddressSanitizer configuration using `/fsanitize=address /Zi` and
`/INCREMENTAL:NO`. CMake compiler probe failed with LNK1104: cannot open
`clang_rt.asan_dynamic_runtime_thunk-x86_64.lib`. A search of the installed VC
toolchain found only x86 sanitizer runtime DLLs, not the required x64 library.
No sanitizer test ran and no system installation was performed. Configure output
is retained in `cinescript-asan-unavailable.txt`. Existing D9025 exception-option
override warnings remain in portable builds; no source warning failures.

The engine gate is unchanged: approximately 46.65 GiB disk and 1.60 GiB available
RAM were recorded in the immediately preceding slice. Root engine configure was
actually attempted and stopped at missing zlib/Freetype with downloads deliberately
disabled. Full dependency and LFS provisioning was deferred for capacity. The
native CineScript Gem, ScriptMotion adapter, Editor, Asset Processor, GameLauncher,
Lua bridge and seven native ScriptMotion tests have not compiled/run here.

## Review and next steps

Author source review checked current CameraBus MakeActiveView/GetActiveCamera,
TransformBus, FileIOStream, DebugDraw zero-duration lifetime, ScriptMotion motion
ownership and idle-speed restoration. Commands are main-thread only; overlapping
native camera sessions are rejected. Cancel/error restores actors and camera;
normal finish retains final actor placements and restores camera/control. No
independent review is claimed. Keep the PR draft until native acceptance and review.

Next: execute the native acceptance procedure, bind a real player controller to
the control-lock API, add production subtitle/audio bindings and package authored
assets. General MissionScript/StoryScript, injury physics, multiplayer and Director
remain separate milestones. Do not merge this source integration as engine-verified.
