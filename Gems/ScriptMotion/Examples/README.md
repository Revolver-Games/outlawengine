# ScriptMotion examples

A new original skinned courier source is available in
`Projects/Wanted/Assets/Characters/ScriptMotionCourier.gltf`, with a matching
Y-up local clip in `Projects/Wanted/Assets/ScriptMotion`. The files below remain
the original Z-up portable fixtures. See `../ACTOR_PREVIEW.md`; engine import is pending.

The original seven-bone `wanted_test_humanoid` skeleton is a data-only test rig, in meters with Z up. It is not a skinned actor asset. `frontier_wave.scriptmotion.json` animates the spine and right arm while untracked bones retain their bind pose. The two-second loop includes timed greeting events.

Version 1 accepts JSON only. Bone `parent` is another bone name or null. Parent order is unrestricted; missing parents, cycles, and duplicate names are rejected. Quaternions use `[x,y,z,w]` and are normalized when parsed. Translation is parent-local. Scale, retargeting, IK solving, and root-motion extraction are outside this initial core.

Clips name their skeleton and contain per-bone `keys`. Each key's optional translation and rotation default to that bone's **bind pose**, not the preceding key. The outgoing segment uses `step`, `linear` (default), or `smoothstep`. Quaternion interpolation uses shortest-path SLERP with the same time curve. Sampling before the first key holds its pose; sampling after the last key holds the last pose. Missing tracks retain bind pose. Loop sampling at exactly `duration` returns the start; non-looping sampling holds the end.

Event intervals are `(previous, current]`: an event at initial time zero is not automatically emitted. On subsequent loop boundaries zero-time events are emitted. Events at both zero and duration are distinct, and may fire together on a loop boundary. Negative time and reverse playback are rejected. Speed zero pauses evaluation at the start; the caller is responsible for maintaining a timeline offset when pausing an active player. Budgets prevent unbounded event materialization; overflow returns an error and no partial results.

The standalone demo accepts the skeleton path, clip path, and optional timeline seconds. It prints evaluated local transforms and all elapsed events. The independent C++ tests exercise the parser and evaluator without requiring the full O3DE SDK.
