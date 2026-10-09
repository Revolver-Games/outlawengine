# Latest continuation: ScriptMotion Actor preview (2026-10-09)

Start with `Documentation/Validation/scriptmotion-actor-preview.md` and
`Gems/ScriptMotion/ACTOR_PREVIEW.md`. The `feature/scriptmotion-actor-preview`
slice has compiled Windows portable coverage and an original skinned courier.
The immediate gate is a real O3DE build/import/editor run, followed by independent
review into development. Do not treat source-only native tests or Blender output
as a passed editor check. Keep workflows unchanged. The earlier handoff below
is retained as historical context.

# Resume Wanted Engine

## Locate the real work

Branch: `wanted-development`. Original upstream base: `e0315902af47533267f273835b99d28b96c76e32`.

Repositories: `https://github.com/Revolver-Games/outlawengine` (organization) and `https://github.com/arOSProject/outlawengine` (contribution fork).
Intended Windows checkout: `C:\WantedDevelopment\WantedEngine`.
Game source: `Projects/Wanted` inside the checkout; keeping it here versions game and engine extensions together.

Read `PROGRESS.md`, `BUILD_INSTRUCTIONS.md`, the ScriptMotion docs, and `Projects/Wanted/README.md` before making claims. Scratch may expire; the milestone recovery archive contains the custom source overlay and Git bundle if GitHub publication is still blocked. It does not replace or pretend to contain the entire upstream engine. Restore the bundle over a genuine upstream checkout at the required base.

## Immediate next tasks

1. Check the personal fork’s `wanted-development` branch and its pull request into `Revolver-Games/outlawengine:development`. Confirm the actual remote commit and PR state before claiming merge or starting further work. Publication uses the GitHub API, so remote commit IDs differ from the original local commits; file trees are verified. Never push Wanted changes directly to `o3de/o3de`.
2. Confirm local Windows access. Inventory Windows, CPU/GPU, RAM/storage, MSVC/SDK, Git/LFS, CMake and Python. This session only inspected Linux. Check that the designated folder is safe before creating or modifying it.
3. Re-run portable CTest and example. Preserve working user changes. Verify the commit and bundle integrity before proceeding from a recovery archive.
4. Install/obtain official O3DE build prerequisites. Ask before writing the normal O3DE Python environment outside the designated directory, as described in build instructions. Hydrate LFS assets as needed; don't blindly download assets into a disk-limited environment.
5. Compile the actual ScriptMotion Gem, editor module and Wanted game. Integration code was source-reviewed but not engine-compiled. Fix actual compiler errors rather than replacing the adapter with mock APIs.
6. Build or import a licensed compatible Actor with matching bone names, parent hierarchy and bind transforms. Add an Actor component and ScriptMotion component, supply the source JSON paths, then test reload/play/seek/stop and actor deletion/recreation. Capture visual evidence. Runtime refuses active Animation Graphs and unsupported step curves.
7. Validate native bake fidelity, event behavior and motion ownership with O3DE integration tests. The core evaluates double precision; native motion uses float samples. Document approximation limits.
8. Complete `Projects/Wanted/FIRST_SCENE.md`: player control, Ada Mercer NPC, satchel interaction and exactly-once mission completion. The existing default level is not yet wired for this gameplay.
9. Publish the ROADMAP rows as issues only in the user's confirmed fork. Then implement CineScript as the next end-to-end authored runtime slice. Keep unfinished online services private.

## Known implementation constraints

Portable ScriptMotion: JSON only, no YAML; local rotation/translation only; forward nonnegative playback; explicit event collection; optional masked pose blending/additive layers. No IK, retargeting, root-motion extraction, skeletal mesh import, graph node, asset builder, compiled runtime asset or allocation-free hot path.

Native adapter: source JSON development loading, bounded motion baking, native motion playback and reflected editor preview controls. Full engine compilation, packaging, visible playback and lifecycle behavior still need verification. Native events follow EMotion FX behavior, which has not yet been compared against portable event semantics.

Mission: fixed state machine with trusted local EBus/Lua adapter, not general MissionScript or StoryScript. No persistent save, rewards/economy, or multiplayer authority implementation.

CineScript, injury/active-ragdoll simulation, gore/effects, open-world systems and WON networking remain future work. Do not report them as implemented because their names appear in this repository.
