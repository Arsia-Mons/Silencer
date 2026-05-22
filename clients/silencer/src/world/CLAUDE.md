# clients/silencer/src/world — World simulation

Owns `World` and its five subsystem classes. `World` is the authoritative
simulation container: it holds the map, resources, audio, lobby state,
trigger graph, and the subsystems below. `Game` calls into `World`; nothing
inside `src/world/` should depend on `Game`.

## Subdirectory map

| Dir | Class | What it owns |
|---|---|---|
| `world.h` / `world.cpp` | `World` | Thin coordinator: owns all subsystems as public members, drives `Tick()` and `DoNetwork()`, hosts map/resources/audio/lobby/triggerGraph/gameMode |
| `objects/` | `WorldObjectRegistry` | Object lifecycle, collision (`TestAABB`, `TestIncr`), relevance, illumination |
| `messaging/` | `WorldMessaging` | In-game messages, chat lines, status overlays, sound broadcasts |
| `network/` | `WorldNetwork` | Transport layer — socket, bind/connect/listen, lag simulator, ping |
| `network/` | `WorldPeerRegistry` | Peer list, team assignment, bot peers, disconnect handling |
| `network/` | `WorldReplication` | Snapshot queue, input queue, client-side prediction, delta encoding |
| `map/` | `Map` | Tile/geometry data, minimap, overlay rendering |
| `gameplay/` | *(free functions)* | Gameplay helpers that span multiple subsystems |
| `physics/` | `Hittable`, `Physical` | Physics base classes for objects with collision/mass |

## Subsystem ownership

```
World
├── objects    : WorldObjectRegistry   (world/objects/)
├── messaging  : WorldMessaging        (world/messaging/)
├── network    : WorldNetwork          (world/network/)
├── peers      : WorldPeerRegistry     (world/network/)
├── replication: WorldReplication      (world/network/)
├── map        : Map                   (world/map/)
├── resources  : Resources
├── audio      : Audio
├── lobby      : Lobby
├── triggerGraph: TriggerGraph
└── gameMode   : GameMode*
```

Access subsystems through `world.<member>`, never by reaching into
`World`'s private fields directly from outside the subsystem's own `.cpp`.

## Friend classes

`World` has 20+ friend classes (`Player`, `Team`, `Lobby`, `Audio`, `Map`,
`Replay`, `TriggerGraph`, `DedicatedServer`, …). Each new subsystem class is
also added to the friend list. External friends that access state now owned
by a subsystem should go through that subsystem's public API instead of
touching private fields.

## Adding a subsystem method

1. Declare it `public` in the subsystem `.h` in `world/<concern>/`.
2. Implement in the matching `.cpp`.
3. Update callers — external files go through `world.<subsystem>.Method()`.
4. No subsystem header may `#include` another subsystem header.

## Build / run

```bash
bash clients/silencer/build.sh     # macOS/Linux
clients/silencer/build.ps1         # Windows
```
