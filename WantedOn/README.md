# WantedOn (WON)

This directory records the first multiplayer architecture decision. No online
server, account system, endpoint or network gameplay component is implemented
or deployed by this milestone. The WANTED project does not enable Multiplayer
yet. Its prototype mission EBus accepts trusted local script calls only.

## Shared systems and ownership

ScriptMotion's portable parser/evaluator is shared source. Visual animation
playback belongs to the client. A future authoritative server should replicate
clip/state identifiers, start ticks and validated gameplay outcomes, not trust
a client-authored pose or injury value.

The future WantedSim injury model should be an engine-independent simulation
used by both the local single-player authority and the dedicated server. This
module has not been implemented yet. Animation, sound and optional blood
effects consume simulation outcomes and cannot change authoritative injury
state.

The first future network component should extend O3DE's existing replicated
entity/input path. Relevant upstream source locations are:

- `Gems/Multiplayer/Code/Include/Multiplayer/Components/NetBindComponent.h`
- `Gems/Multiplayer/Code/Include/Multiplayer/Components/NetworkCharacterComponent.h`
- `Gems/Multiplayer/Code/Source/AutoGen/NetworkCharacterComponent.AutoComponent.xml`
- `Gems/Multiplayer/Code/Source/AutoGen/NetworkTransformComponent.AutoComponent.xml`
- `Gems/Multiplayer/Code/Include/Multiplayer/NetworkInput/IMultiplayerComponentInput.h`
- `Gems/Multiplayer/Code/Tests/NetworkCharacterTests.cpp`

## First implementation contract

| Data or action | Authority | Proposed handling |
| --- | --- | --- |
| Movement input | Client proposes; server validates | Tick-stamped bounded input, server simulation, local prediction and correction |
| World position and velocity | Server | Existing O3DE replicated transform/character components |
| Interaction | Client requests; server validates | Server checks entity ownership, distance, scene membership and objective state |
| Injury outcome | Server | Versioned body-region state delta and stable outcome/event ID |
| Injury presentation | Client | Consume validated outcome; configurable effects never affect damage |
| Mission progress and reward | Server | Idempotent transition and reward transaction; private local authority in single-player |

The first network milestone is a local dedicated server with two test clients:
move one character, observe it on the other client, apply a server-originated
test injury state, and reject a client request that attempts to set the injury
value directly. Then test disconnect/reconnect, repeated event IDs and stale
input. Do not enable a public listener during this work.

Persistence, character creation, posses, free roam, economy, matchmaking and
anti-cheat integrations remain backlog items. Server validation is required;
it is not itself a complete anti-cheat system.
