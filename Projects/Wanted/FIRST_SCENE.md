# Next scene: Mercy Crossing test yard

Status: scene assembly and editor verification are pending. This is the exact
next editor task, not a claim that these entities exist in DefaultLevel.

1. Build Editor, AssetProcessor and Wanted.GameLauncher with official O3DE
   dependencies and the ScriptMotion Gem enabled. Process the default scene's
   engine asset dependencies. Open the project and save a **copy** of
   `DefaultLevel` as `MercyCrossingTest`.
2. Keep the ground collider, sunlight and environment. Create a `Player` entity
   at `(0, 0, 1)` with a PhysX Character Controller and a following camera.
   Implement and verify WASD movement through the character-controller API;
   player movement is not yet provided by this project.
3. Create a visible `Ada` marker at `(0, 4, 1)` and a `Satchel` marker at
   `(4, 4, 0.5)`, using O3DE PrimitiveAssets geometry. These may be simple
   debug shapes during verification. Add labels or a debug UI so the two
   interaction targets are identifiable.
4. Define an `Interact` input action mapped to keyboard E with StartingPointInput
   and attach its input binding asset to the player. Create a mission-controller
   entity with a Lua Script component using
   `Scripts/FirstErrandInteraction.lua`. Assign Player, Ada and Satchel entity
   references in its properties. Do not attach this script to multiple entities.
5. Enter play mode. Verify the debug log asks to meet Ada. E at the satchel
   first must do nothing. At Ada, E must advance to finding the satchel. At the
   satchel, E must advance to returning it. At Ada, E must complete once.
   Remaining outside the interaction radius must block the interaction.
   Re-entering play mode should reset the demonstration deliberately.
6. Add a UI objective/subtitle panel that consumes
   `WantedNotificationBus`. No UI assets currently implement this step.
7. Create/import a licensed or original Actor with the same seven bone names and
   hierarchy as `Gems/ScriptMotion/Examples/wanted_test_skeleton.json`.
   The JSON file alone cannot provide a visible skinned Actor. Attach the Actor
   and ScriptMotion components to Ada, configure the skeleton and wave paths,
   then verify character validation and playback in both editor preview and
   runtime. Follow the Gem documentation; no matching Actor has been supplied
   by the original milestone. The follow-up now supplies an original skinned
   glTF courier and matching joint-local JSON; follow
   `Gems/ScriptMotion/ACTOR_PREVIEW.md` to process it as an Actor and verify
   playback. O3DE import and the playable scene are still pending.
8. Save the prefab and its owned assets. Point
   `Registry/load_level.setreg` to the new level only after standalone launcher
   verification. Record the exact engine build, asset processor results and
   verification outcome in the engine's `PROGRESS.md`.

Acceptance: a player can move, approach Ada and the satchel, and complete the
three-interaction mission exactly once. Ada can visibly play the ScriptMotion
wave on a validated Actor. Missing assets or a blank viewport do not pass.

Deferred from this scene: injury/physics reactions, horse movement, cinematic
camera scripting, persistence, branching narratives and networking.
