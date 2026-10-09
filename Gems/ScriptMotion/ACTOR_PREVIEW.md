# Text animation / Actor preview slice

This slice adds a tested portable bake used by the native adapter, a matching
original skinned courier source, and editor pause/scrub improvements. It does not
establish a successful O3DE build or an editor demonstration. See
`Documentation/Validation/scriptmotion-actor-preview.md` for the executed checks.

## What changes

- `BakeMotion` produces bounded float timestamps and evaluated local poses for
  every joint. It includes authored key times, both endpoints, and untracked bind
  joints. The last sample evaluates the exact source endpoint even when its
  timestamp rounds down to a float. Bind-only clips are supported.
- Native baking rejects step segments, colliding float key times, durations over
  600 seconds, rates outside 30–240 Hz, over two million joint samples, and over
  twenty million estimated validation visits. Deep ancestor chains count toward
  that work limit. Rejected input returns no partial bake.
- The native adapter consumes that same bake and fills real
  `EMotionFX::NonUniformMotionData`. The portable sample data is not an O3DE
  `.motion` file or an Asset Processor product.
- Optional **Use Actor bind pose** obtains the exact loaded Actor's joint names,
  parents and local bind transforms. **Actor skeleton name** must match the
  authored clip. The existing explicit skeleton-file mode remains the default.
  Both modes reject invalid rigs and non-unit bind scales; this is not retargeting.
- **Pause preview** changes transport speed without reopening files or resetting
  the playhead. A paused motion uses full weight so an unfinished blend-in cannot
  hide the preview. Seek deliberately completes blend-in, resets native loop and
  frozen state, resets last time, and samples the Actor pose at zero elapsed time.
  It can therefore resume a previously completed non-looping motion.

## Engine acceptance procedure (not yet executed)

1. Build `ScriptMotion`, `ScriptMotion.Editor`, `ScriptMotion.Native.Tests`,
   `Editor`, `AssetProcessor`, and `Wanted.GameLauncher` against the complete
   engine toolchain. Run the native tests before attempting editor acceptance.
2. Open `Projects/Wanted`. Process
   `Assets/Characters/ScriptMotionCourier.gltf`. In Scene Settings, create/select
   an Actor group, select the skeleton root `root` and the `CourierMesh` skin,
   and save the generated manifest. Verify that an `.actor` product and skinned
   model/material products actually succeed. A model-only import is insufficient.
   Inspect the scene tree if the importer adds an outer root; don't guess a
   product UUID or commit a prefab referencing a nonexistent product.
3. Place an Actor entity using that processed Actor. Verify the body is about
   2.1 meters tall and its skin renders correctly. Add ScriptMotion on the same
   entity. Set **Use Actor bind pose** on, **Actor skeleton name** to
   `courier_gltf`, and **Animation JSON** to
   `@projectroot@/Assets/ScriptMotion/courier_wave.scriptmotion.json`.
   Leave Skeleton JSON empty in this mode. Do not add Simple Motion or Anim Graph
   to the same entity; these services are incompatible with ScriptMotion.
4. Enable **Preview in editor**. Verify a visible wave, then pause, seek to
   `0`, `0.5`, `1`, and `2` seconds, and resume. Disable Loop, let the clip finish,
   seek back, and confirm playback resumes. Repeat with configured speed zero
   and nonzero blend-in. The preview must remain visible when paused.
5. Introduce malformed source text or an unknown bone name and use **Reload and
   preview**. Verify the previous valid motion is retained and status reports
   the error. Restore valid text and reload. Check that scrubbing does not emit
   skipped greeting events; native loop-boundary event semantics still need QA.
6. Replace/delete the Actor, deactivate/re-activate the entity, enter/leave game
   mode, and repeat load/stop. Check for stale-instance access, duplicate motion
   instances and leaks. Confirm the game uses its own configuration (the editor
   pause checkbox is not exported).
7. Save a new preview prefab/level only after these checks. Preserve DefaultLevel.
   Check standalone launcher playback before calling this a playable demo.

### Coordinate basis

The original `wanted_test_skeleton.json` example is Z-up. The courier glTF source
is Y-up, as required by glTF. Its companion JSON uses those same **joint-local**
axes. O3DE's enabled `AssImpReadRootTransform` performs a conversion at the scene
root; it does not make every child joint's local axes Z-up. Reusing the original
wave unchanged would rotate around the wrong local axes. The generator converts
both local translations and quaternion vector components `[x,y,z]` to `[x,z,-y]`.
Actor-bind mode also preserves the imported root transform or extra importer
joints without weakening bone-name/clip validation. The standalone companion
skeleton describes the source rig, not a guarantee of exact imported root data.

## Verification commands

From an x64 Developer PowerShell with CMake/Ninja available:

```powershell
cmake -S Gems/ScriptMotion/Standalone -B build-scriptmotion -G 'Ninja Multi-Config'
cmake --build build-scriptmotion --config Debug
ctest --test-dir build-scriptmotion -C Debug --output-on-failure --timeout 30
cmake --build build-scriptmotion --config Release
ctest --test-dir build-scriptmotion -C Release --output-on-failure --timeout 30
python Gems/ScriptMotion/Examples/Tools/test_courier_asset.py
```

For a complete O3DE build (required dependencies must already be provisioned):

```powershell
cmake -S . -B build-wanted -G 'Ninja Multi-Config' -DLY_PROJECTS=Projects/Wanted -DLY_TEST_IMPACT_ACTIVE=OFF
cmake --build build-wanted --config profile --target ScriptMotion ScriptMotion.Editor ScriptMotion.Native.Tests
ctest --test-dir build-wanted -C profile -R 'ScriptMotion.Native.Tests' --output-on-failure
```

The six native tests use a real EMotion FX Actor/MotionSystem, temporary JSON
files and native pose sampling. They cover joint/event upload, malformed reload,
Actor-bind mode, pause and seek-after-completion, repeated stop/reload/detach, and
external removal of a motion instance. They have been added but not compiled or
executed in this session. Portable tests are not a substitute for this gate.

Remaining restrictions: native interpolation approximates smooth curves;
discontinuous step playback, Anim Graph integration, retargeting, scale tracks,
root-motion extraction and packaged JSON assets remain unsupported. This courier
is a simple rigid-skinned test mannequin, not final character art, a walking
controller, a ragdoll, or a completed Mercy Crossing level.
