# CineScript — first runtime slice

CineScript is a new C++20 Gem with a compiled portable timeline and an O3DE
component implementation. The portable interpreter and real ScriptMotion/mission
integration test run on Windows. The native Gem, reflected editor configuration
and camera/Actor behavior have **not** compiled or run in O3DE yet.

## Implemented behavior

- Strict JSON scene loading with actor/camera identifiers, world-space transform
  tracks, smoothstep/linear/step movement and quaternion interpolation.
- Camera cuts, animated camera transforms, timed dialogue/subtitle state and
  ScriptMotion clip aliases with deterministic relative playback time.
- Conditional signal, audio and music cues. Audio/music are dispatched aliases;
  this slice contains no voice/music recordings or audio-control bindings.
- Play, pause, silent seek, skip, cancellation, once-only completion, bounded
  event processing and revisioned checkpoint serialization/restoration.
- A two-character delivery scene and a compiled 448-update mission/animation
  integration test. This is a headless runtime test, not a playable O3DE level.

`Code/Include/CineScript/Timeline.h` is the public portable API. `Load` owns and
validates its scene and condition flags. Invalid operations preserve the loaded
timeline. `Start` returns the opening snapshot and time-zero cues. `Advance`
returns placements, active camera/dialogue/animations, crossed cues and completion.
The caller applies those outputs. `Skip` samples final placements and completes
without firing pending cues. Gameplay rewards must not be granted by cinematic
signals: the authoritative mission state owns them.

`Seek` is silent and cancels no external effects already executed by the host.
Seeking backwards while playing can re-emit future cues on subsequent advances;
use side-effect-free preview listeners. At the end it unlocks control without
emitting completion. `Stop` rewinds silently. To restart a finished/stopped scene,
call `Start`. Pausing retains the control lock. A checkpoint records scene id,
revision, time, play/pause/completion state and condition flags; restore never
replays earlier cues. Bump the authored revision whenever scene semantics change.
Checkpoint data is local save state, not proof of multiplayer authority.

## JSON format v1

See `Projects/Wanted/Assets/CineScript/mercy_delivery.cinescript.json`.

- Root fields: `formatVersion: 1`, `id`, positive integer `revision`, `duration`,
  `targets`, `tracks`, `events`. All arrays are required; tracks may be empty.
- Targets: `{ "id": "ada_mercer", "kind": "actor" }` or kind `camera`.
- Tracks: `{ "target": "player", "keys": [...] }`. Keys use ScriptMotion's
  `time`, `translation`, `rotation` and `interpolation` fields, starting at zero.
  Coordinates are world-space O3DE Z-up metres, unlike the courier's glTF local
  Y-up bone coordinates. Missing translation/rotation default to zero/identity.
  No scale, lens/FOV curves or camera-to-camera dissolve are implemented.
- Events have `time`, `type`, optional `duration`, `target`, `value` and `if`.
  `if` names a true flag frozen for the session. Events must be ordered by time;
  equal-time events retain authored order.
- `camera`: camera target, no value/duration; require an unconditional opening cut.
- `dialogue`: actor target, subtitle in value, positive duration. Dialogue windows
  cannot overlap. The target identifier is the development speaker label.
- `animation`: actor target, whitelisted clip alias in value, positive duration.
  Animation windows on one actor cannot overlap. Active intervals are `[start,end)`.
- `signal`, `audio`, `music`: alias in value, no target/duration. Only Advance or
  Start dispatches them; Seek, restore and Skip do not replay them.

Unknown fields, duplicate JSON keys, unknown targets/types, nonfinite numbers,
invalid quaternions, bad ordering, overlaps and unsupported versions are rejected.
Bounds: 1 MiB input, nesting 16, 64 targets/tracks, 4,096 total transform keys,
1,024 events, 128 flags, 600-second scenes, and a 60-second maximum update.
Identifiers contain only ASCII letters, digits, `_` and `-`. Cue/clip aliases are
not paths or executable commands. No embedded code or AI service runs at playback.

## O3DE integration implementation (unverified)

The reflected CineScript component is intended for the editor's generic runtime
component wrapper. Configure Scene JSON, bindings, true flags, optional first-tick
autoplay and development subtitles. Calls are exposed through `CineScriptRequestBus`.
`CineScriptNotificationBus` exposes control lock, subtitle, typed cue, completion
and error events to C++ and Lua.

The adapter loads through FileIO, validates every entity/Camera/ScriptMotion
binding, captures original world transforms and the active gameplay camera, then
uses `TransformBus::SetWorldTM`, `CameraRequestBus::MakeActiveView` and the existing
ScriptMotion Play/Seek controls. It owns one camera session globally on the main
thread, ticks after animation, pauses the native motion and explicitly seeks it.
It restores configured animation speeds and releases its motions on completion,
cancellation, error or deactivation. Cameras always restore; actor placement
restores on cancel/error, and remains at the final scene position on completion.
Existing animation graphs/motions must not compete for those bound actors.

The adapter requires a restorable active gameplay Camera. Each actor supports one
configured ScriptMotion clip alias, not arbitrary scene-provided file paths.
Missing/deactivated entities stop the scene safely. Commands during notification
dispatch are rejected; defer them to the next game tick. Source reload occurs on
Play or Restore, preserving a valid session on invalid input. Source JSON loading
is development-only; a packaged Asset Processor builder remains to be implemented.

DebugDraw provides temporary development subtitles. Production LyShine UI,
audio playback, lighting events, physical actor movement/navigation, network
authority and the Outlaw Director timeline panel remain future work. Consumers
must enforce OnControlLockChanged (or IsPlaying) in their player input controller;
there is not yet a completed third-person controller in this project.

## Native acceptance procedure

1. Build CineScript, ScriptMotion, their dependencies, Editor, AssetProcessor and
   Wanted.GameLauncher with the actual engine toolchain. Run the native tests;
   this slice adds an idle-speed restoration case (seven total ScriptMotion native
   cases, all still unexecuted here).
2. Create a separate Mercy Crossing test level. Bind `player`, `ada_mercer`,
   `wide` and `ada_close` to distinct active entities with Transforms; the latter
   two require Cameras. Keep a separate active gameplay Camera for restoration.
3. Import the existing seven-joint courier for Ada. Configure her ScriptMotion
   Actor-bind mode, `courier_gltf` skeleton name, `courier_wave.scriptmotion.json`,
   non-looping playback and zero blend-in. Bind alias `courier_wave` in CineScript.
4. Set the scene source to `@projectroot@/Assets/CineScript/mercy_delivery.cinescript.json`.
   Add true flag `satchel_returned` only for this completed-delivery test.
5. Add `DeliveryCinematic.lua` to the mission controller, set its CinematicEntity,
   and drive the existing FirstErrand interactions. Completion starts the scene.
6. Verify two characters, approach, camera movement/cut, both subtitles, Ada's
   wave and restoration of gameplay camera/control. Exercise pause, seek, skip,
   cancel, invalid reload, actor deletion and entity deactivation. Verify actors
   and camera restoration, original animation speed, and exactly one completion.
7. Verify checkpoint continuation mid-dialogue/wave without replaying earlier
   cues. Repeat in the launcher before accepting native integration.

No existing level/prefab was replaced, and no playable-level result is claimed.
