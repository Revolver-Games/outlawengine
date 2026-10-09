# MissionScript and StoryScript — first authored runtime

The WANTED Gem now contains an opt-in, portable C++20 narrative session. It
preserves the existing `FirstErrand` class and all original bus entry points.
MissionScript objective state and StoryScript dialogue/flag/reputation state live
in one owning session so save restoration is consistent across both.

`Gem/Include/Wanted/Narrative.h` is the public API, implemented in
`Gem/Source/Narrative.cpp` and compiled as `Wanted.Narrative` in O3DE or
`WantedNarrative` standalone. The core is compiled/tested. O3DE system-component
bindings and the optional Lua cinematic bridge have not compiled/run here.

## Working slice

`Assets/Narrative/unclaimed_post.narrative.json` defines an original delivery:
start the mission, choose to help Ada, collect the satchel, then choose to return
the mail. Out-of-order collection and locked return dialogue are rejected.
An optional conversation about town records a decision and flag. Accepted choices
change Ada's reputation; completion sets the postmark flag, awards five local
credits once and returns the `mercy_delivery` cinematic alias.

The compiled combined test passes actual story flags to CineScript, executes the
matching scene and evaluates the original courier's ScriptMotion wave. It verifies
one acknowledgement/completion, checkpoint continuation and skipping without
repeating mission rewards. This is headless integration, not a playable level.

## Public API and state

- `Load(json)` strictly validates a definition and starts a fresh session. An
  invalid load preserves current progress. Successful load intentionally resets
  it; use Save/Restore when reloading the same revision.
- `Start()` requires the mission unlock flag, if configured. It cannot restart
  active, completed or failed progress. `Fail()` only affects an active mission.
- `Event(kind,target)` consumes trusted host events for `interact`, `collect`,
  `enter` and `signal`. Only the current matching, unlocked objective advances.
  Counts support repeated collections/events. The host must validate spatial
  range, trigger occupancy, actual item ownership and event identity.
- `BeginDialogue`, `Choices`, `Choose`, `CloseDialogue` execute branching dialogue.
  Choice guards and next-node guards are checked before mutation. Mission dialogue
  objectives require an actual accepted choice; a forged `Event("dialogue",...)`
  is rejected. Quest choices cannot be consumed before their objective is active.
- Choices are once per session, preventing repeat reputation farming. Ordered
  decisions and declared flags survive checkpoints. Reputation is a configurable
  game score clamped to [-100,100], not an AI/NPC relationship simulation.
- `Mission()`, `Story()`, `ObjectiveText()` and `DialogueJson()` provide read-only
  state and UI output. `Change` reports acceptance, objective change, completion,
  newly added credits and an optional cutscene alias.

`Save()` returns a versioned accepted-command journal. `Restore()` checks content
id/revision and replays it through the same guards into a temporary session. A
malformed, out-of-order, unknown or rejected command aborts restoration without
changing the live state. No callbacks or cinematic/reward notifications escape
replay. The whole local credit balance is replaced by restored state, not added
again to an external wallet. Increase the content revision when editing semantics.

This save format is not an authentication mechanism: a client can forge a valid
local history. Do not expose Restore/Event or reward assignment as unrestricted
WON RPCs. Server-owned world validation and persistent transactions are future work.
No filesystem writes occur in the core; the host saves the returned string using
its storage policy. Journal compaction/migration and cross-version saves are pending.

## Definition format v1

Required root fields: `formatVersion: 1`, identifier `id`, identifier `chapter`,
positive integer `revision`, declared `flags`, declared `relationships`, `mission`
and `dialogue`. This slice loads one mission/chapter definition at a time. Chapter
transitions, multiple concurrent missions and cross-document unlocks are pending.

Mission fields: optional `requires` flag, optional `completedFlag`, optional
`cutscene` alias, integer `reward` (0..1,000,000), and nonempty `objectives`.
Objective fields: unique `id`, UI `text`, `event`, `target`, optional `count` (1..32),
optional `requires` flag and optional `sets` flag. Dialogue objectives name a
globally unique choice and must have count one. Flags represent facts, not a full
inventory; collecting/returning an item still requires host inventory handling.

Dialogue nodes have unique `id`, `speaker`, `text`, optional `requires`, and
`choices`. Choice fields: globally unique `id`, `text`, optional `next` node,
optional `requires`/`sets` flags, optional declared `relationship` and optional
integer `reputationDelta` (-20..20). Empty next closes the conversation. Branches
may loop but each choice is consumed once; close dialogue when no choices remain.

Unknown fields, duplicate keys/names, invalid types, undeclared variables, missing
dialogue links, unknown event types and unsupported versions fail safely.
Limits: 1 MiB JSON, nesting 16, 128 flags, 32 relationships, 32 objectives,
128 dialogue nodes, 16 choices/node, 512 choices total, 2,048-byte text fields,
128-byte ASCII identifiers and 4,096 accepted commands/session. A full journal
rejects further mutation without partial changes. Each source has original
narrative content; the reused nlohmann JSON dependency retains its MIT license.

## O3DE controls (source integration awaiting verification)

The existing `WantedRequestBus` now exposes LoadNarrative, StartNarrative,
SendNarrativeEvent, BeginNarrativeDialogue, ChooseNarrativeDialogue,
CloseNarrativeDialogue, FailNarrative, GetNarrativeObjective, GetNarrativeDialogue,
GetNarrativeError, HasNarrativeFlag, GetNarrativeReputation, GetNarrativeCredits,
SaveNarrative and RestoreNarrative. New notifications are OnNarrativeChanged
(objective and dialogue JSON) and OnNarrativeCompleted (cutscene alias and credits).
Mutating narrative calls during notification dispatch are rejected; defer them.
Restore emits only UI state refresh, never completion or another reward signal.

After compiling Wanted, load
`@projectroot@/Assets/Narrative/unclaimed_post.narrative.json`, start the session,
and bind interaction/trigger/inventory adapters to these trusted event methods.
Render the returned objective/dialogue data with a game UI; no production UI or
new player controller is supplied in this slice.

`Scripts/NarrativeCinematic.lua` optionally starts a whitelisted scene from the
completion notification when the separate CineScript Gem is available. Set its
CinematicEntity and AllowedScene. Configure CineScript's `satchel_returned` true
flag for this completed-delivery scene. The compiled combined test passes actual
story flags directly; dynamic native flag transfer remains to be connected.
The bridge never grants credits itself. Do not attach both the legacy and the new
mission-start bridges to the same gameplay path.

## Reproduce tests

```powershell
cmake -S Projects/Wanted/Tests -B build/narrative -G "Ninja Multi-Config"
cmake --build build/narrative --config Debug --parallel 2
ctest --test-dir build/narrative -C Debug -V
cmake --build build/narrative --config Release --parallel 2
ctest --test-dir build/narrative -C Release -V
```

Each configuration passed 2/2 tests: original FirstErrand (20 assertions) and
narrative (4,212 assertions, including 4,096 journal-capacity operations).
These are assertion counts, not thousands of distinct test cases.

For combined tests, supply `-DWANTED_CINESCRIPT_SOURCE=<repo>/Gems/CineScript`
to a separate build directory after obtaining the CineScript slice. This session
used an exact source snapshot from local commit
`99c80759552cd370c1b12f0f9733d0b522c9c91e` (published PR #5 has the identical tree),
without merging branches. Debug and Release each passed 5/5 CTest, including
724 assertions across mission → story flags → CineScript → ScriptMotion.
