# ScriptMotion owning playback session

`Code/Include/ScriptMotion/Player.h` exposes the portable C++20 `Player`.
The existing EMotion FX component remains the native playback path. The new
session is compiled into `ScriptMotion.Core` and the standalone library, and
executed by `scriptmotion_player`. It is not yet connected to an EMotion FX
pose graph or the editor UI; portable transition/root-motion results do not
establish native support for those features.

## Public contract

- `Load(skeleton, clip, loop)` validates and owns copies. Invalid reload preserves
  the current session. A successful load clears layers, root extraction and fades.
- `Advance(dt)` advances source time by `dt * speed`, samples a pose, collects base
  events and optionally extracts root translation. Changing speed never remaps
  elapsed source time. Errors leave time and completion state unchanged.
- `SetPaused`, `SetSpeed`, `Stop`, `Sample`, `Seek` implement transport. Stop
  rewinds and pauses. Seek is silent, cancels a fade and clamps non-looping clips.
  Time-zero events are silent, matching the existing event API. Equal-time events
  sort by source index, including end/start events at a loop boundary.
- `TransitionTo` blends from the current unlayered pose snapshot to a progressing
  incoming clip with smoothstep weight. Interrupted transitions stay continuous.
  The outgoing clip is frozen, not independently ticking; only incoming events
  fire. Fades use wall time and pause when playback is paused or speed is zero.
- `SetLayers` accepts up to four ordered, owning overlays with bone masks and
  optional additive reference poses. Layers sample the base's source clock with
  independent loop/clamp behavior. Their events are deliberately not emitted.
- `SetRootMotion(rootIndex)` extracts translation from the base clip only, across
  any crossed loops, and removes that translation from the resulting pose.
  Rotation extraction, physics application and native entity motion are pending.
  During fades, incoming base displacement takes effect immediately; locomotion
  speed blending is not implemented. Seek/Stop never emit a teleport displacement.
- Non-loop completion is a single edge on Advance; seeking to the end is silent.
  Rewinding allows another completion. Consumers decide what completion means.

Bounds: four layers, 16x speed, 60-second maximum frame step/fade, 24-hour source
session clock and 1,024 events per update, in addition to existing clip/skeleton
limits. The implementation revalidates and allocates poses; it is a correctness
foundation for tools and a small cast, not a profiled crowd-animation hot path.
It is single-threaded. No arbitrary scripts, I/O, callbacks or network requests
are executed by the core. Check each returned error before using output.

## Original locomotion character and clips

`Projects/Wanted/Assets/Characters/ScriptMotionLocomotionCourier.gltf` adds a
16-joint rigid-skinned mannequin with separately weighted thighs, shins and feet.
The original seven-joint courier and its fixtures remain unchanged.

`Projects/Wanted/Assets/ScriptMotion/locomotion.skeleton.json` matches its Y-up
local joint basis. `idle`, `walk` and `greeting` `.scriptmotion.json` files show
breathing, an illustrative walk cycle (1.2 metres per cycle) and a greeting.
This is deliberately simple original test art, not final Ada Mercer animation.
Foot locking, IK, physically correct gait and retargeting are not implemented.
The walk displaces its root: use extraction in Player, or configure native motion
extraction appropriately after testing in O3DE, to avoid a visual loop reset.

Regenerate with `python Gems/ScriptMotion/Examples/Tools/generate_locomotion.py`.
All new source art shares this repository's Apache-2.0 OR MIT license; no external
art, textures, network requests or Python dependencies are used by the generator.

## Executed integration test

Run `scriptmotion_player` with the rig, idle, walk and greeting paths in that order.
It advances 768 frames at 64 Hz through two transitions, four walk loops and a
non-looping greeting. The executable requires 4.8 metres of motion, eight footstep
events and exactly one greeting completion to exit successfully. CTest invokes
this test and separately bakes all three clips through the real shared native
bake code. O3DE asset import and visible playback of this new rig remain untested.
