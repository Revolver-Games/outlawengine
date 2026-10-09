# Wanted Engine architecture

## Source boundaries

Keep upstream systems intact. Add independently testable C++20 logic behind O3DE Gem adapters. Avoid changes to Atom, EMotion FX, PhysX, or Multiplayer until a concrete implementation requires them. Preserve O3DE's dual Apache-2.0/MIT notices and third-party licenses.

| Path | Responsibility | Verification boundary |
| --- | --- | --- |
| `Gems/ScriptMotion/Code/Include/ScriptMotion/ScriptMotion.h` | Public clip, skeleton, pose, timing, event and blend API | Portable C++20 |
| `Gems/ScriptMotion/Code/Source/Core` | Strict JSON import, skeleton validation, interpolation and layering | Portable C++20 |
| `Gems/ScriptMotion/Code/Source/O3DE` | Actor mapping, native EMotion FX motion conversion, component/module integration | Source-reviewed; full engine build required |
| `Gems/ScriptMotion/Standalone` | Reproducible core build and command-line sample | Compiled without engine dependencies |
| `Gems/ScriptMotion/Code/Tests` | Independent numerical and invalid-input tests | Compiled portable tests |
| `Gems/ScriptMotion/Examples` | Original small skeleton and wave animation | Parser/evaluator example |
| `Projects/Wanted` | Real O3DE DefaultProject instantiation, game Gem and first mission foundation | Mission state machine tested; editor/game unverified |
| `WantedOn` | Online design and authority contract | Planning only; no public services |

## ScriptMotion runtime

The parser rejects unknown fields, duplicate keys, unsupported format versions, malformed/nonfinite input, invalid quaternions, invalid hierarchies, incompatible skeletons, unordered keys, and excessive input sizes. Times are seconds; translation is in meters; local rotations use quaternion `[x,y,z,w]` order. Keyframes can be arbitrarily dense, including per-frame samples. Missing tracks retain skeleton bind transforms.

Evaluation starts from the bind pose, samples each bone track, applies step/linear/smoothstep segment timing, and interpolates rotations using normalized shortest-path SLERP. Timeline sampling defines speed, clamp and loop behavior separately from event collection. Event collection returns bounded, ordered occurrences in `(from,to]`, including defined loop-boundary behavior. Blend and additive-layer operations support per-bone masks.

The O3DE adapter constructs an EMotion FX native motion. Its bounded resampling includes authored key timestamps. Native playback between samples may approximate the portable evaluator, particularly smoothstep and rotational interpolation; do not claim bit-for-bit or continuous-curve equivalence. Validate visible joint motion and transitions in the actual editor before relying on the adapter.

The core has no network, audio, graphics, entity lifecycle, or file-system side effects. The adapter owns O3DE-specific data lifetimes and actor interaction. Core validation is deliberately conservative for this milestone; a future immutable, prevalidated runtime clip should remove repeated validation and allocation from per-frame evaluation.

## Actual upstream interfaces inspected

- `Gems/EMotionFX/Code/EMotionFX/Source/Motion.h`
- `Gems/EMotionFX/Code/EMotionFX/Source/MotionData/NonUniformMotionData.h`
- `Gems/EMotionFX/Code/EMotionFX/Source/MotionData/MotionData.h`
- `Gems/EMotionFX/Code/EMotionFX/Source/MotionSystem.h`
- `Gems/EMotionFX/Code/EMotionFX/Source/ActorInstance.h`
- `Gems/EMotionFX/Code/Integration/ActorComponentBus.h`
- `Templates/DefaultProject` and `Templates/EngineFinder`
- `Code/Framework/AzCore`, `Code/Framework/AzFramework`, `Gems/Multiplayer`, `Gems/PhysX`

## Planned extensions, not implemented features

CineScript will own a validated sequence timeline and actor/camera/audio/subtitle events. MissionScript will generalize the initial mission state machine to validated authored objectives, triggers, persistence and branching. StoryScript will organize original narrative state and dialogue. None is a complete authored-script runtime yet.

WantedSim will separate authoritative injury state from optional blood/decal/stain rendering. Integrate rigid bodies, impulses, constraints, ragdoll drive targets and animation/physics blending with PhysX and EMotion FX, with fixed-step tests for stability before richer effects. There is currently no injury or active-ragdoll implementation.

WANTED and WON will share gameplay state and simulation code. The server will own movement validation, damage resolution, injury outcomes, mission rewards and persistence; clients submit intents. Do not replicate client-computed outcomes as authority. This milestone has no server, replication, economy, anti-cheat or hosted service.

## CineScript implementation update (2026-10-09)

`Gems/CineScript/Code/Include/CineScript/Timeline.h` defines validated scene state and
stateless frame outputs. Timeline.cpp interprets movement, camera, dialogue and
ScriptMotion alias intervals; signal/audio/music cues are separate side effects.
The O3DE component applies snapshots through existing buses and restores camera,
transforms and animation speed on teardown. The core is compiled/tested; native
code remains unverified. The earlier planned-extension paragraph describes the
initial milestone, not this new implementation. See the CineScript README.
