# Current acceptance status

Repository publication is established and PRs #1–#3 are merged. Portable playback
now has three authored animations, layers, transitions and root translation.
Engine compilation and visible O3DE playback are still open gates. The next
portable milestone is CineScript integrated with FirstErrand. No later milestone
is completed merely by this runtime addition.

# Wanted Engine roadmap

This is a scoped backlog, not a list of completed capabilities. Verify each acceptance criterion before marking it complete.

| Priority | Milestone | Acceptance criterion |
| --- | --- | --- |
| P0 | GitHub fork and branch publication | Repository metadata proves fork parent `o3de/o3de`; `wanted-development` remote commit independently matches delivered work |
| P0 | Full Gem compilation | Windows MSVC or supported Linux Clang builds ScriptMotion plus Wanted game targets against the pinned O3DE revision |
| P0 | Visible ScriptMotion demo | Import a licensed actor, map the example skeleton, play/pause/reload in the editor; inspect hierarchy and motion lifetime |
| P1 | Native motion fidelity | Tests compare authored keys, resampled curves, events, looping and blending in EMotion FX against the core; bound approximation error |
| P1 | Asset pipeline | Custom source extension, Asset Processor builder, versioned runtime asset and safe reload; verify packaged-game playback |
| P1 | Playable First Errand | Controllable player, NPC Ada Mercer, dispatch satchel, marked interaction prompt, objective and exactly-once completion in a saved level |
| P1 | CineScript vertical slice | Validated two-actor sequence with camera cuts, dialogue/subtitles, audio hooks, skip/resume and editor timeline debug |
| P1 | MissionScript / StoryScript | Schema-driven objectives and branching, validated flags/relationships/dialogue, save/load and deterministic completion tests |
| P2 | WantedSim foundation | Region injury state, movement/balance/stamina effects, persistence; real impulses and constrained animation/physics blend verified at multiple time steps |
| P2 | Procedural animation / IK | Stable foot placement, reach adjustment, layer ownership and joint limits tested on actual actor rigs |
| P2 | Optional blood/effects | Independent intensity settings, pooled decals/stains/material effects and measured budgets |
| P2 | WON private prototype | Dedicated local server, intent validation, server-owned movement/injury, two clients and stale/out-of-order packet tests |
| P3 | Open-world slice | One town and surrounding terrain, horse/NPC/wildlife behaviors, inventory, weather/time and persistent save data |

No issue backlog was published while GitHub fork creation remained blocked. These rows are ready to become issues after fork access is available. Do not enable public game servers as part of that step.

## CineScript acceptance update

Portable timeline, original two-actor scene, skip/checkpoints and ScriptMotion/
FirstErrand headless integration are implemented and compiled. The P1 CineScript
row remains open until the native camera, subtitles, game control and visible
Actor acceptance passes. Audio/music listeners, lighting changes, camera dissolve
and an editor timeline are still pending. General mission/story authoring follows.
## Narrative acceptance update

The first authored MissionScript/StoryScript core and combined cinematic test
are implemented. Their broad milestone remains open: only one active definition,
local save-owned rewards and host-supplied events are supported. Real trigger and
inventory adapters, production dialogue/objective UI, chapter transitions, multiple
missions, native acceptance and networking authority still need implementation.
WantedSim, WON and Outlaw Director are not implemented by this slice.
