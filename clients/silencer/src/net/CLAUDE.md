# src/net

All networking for the Silencer client: lobby server connection, peer-to-peer game sessions, control/input sockets for testing and tooling, map downloads, and development lag simulation.

## Files

| File | Purpose |
|---|---|
| `peer.h/cpp` | Per-player session record: id, host flag, input queue, latency stats. |
| `lobby.h/cpp` | TCP connection to the lobby server. Auth, chat, game list, update check. |
| `lobbygame.h/cpp` | Serializable game-list entry received from the lobby. |
| `dedicatedserver.h/cpp` | Dedicated-server heartbeat to lobby, ban list. |
| `controlserver.h/cpp` | Local JSON command socket for agent/CLI/test control. |
| `controldispatch.h/cpp` | Routes control commands to game state; manages multi-frame waits. |
| `inputserver.h/cpp` | Local input socket: action snapshot, scancode snapshot, mouse snapshot. |
| `lagsimulator.h/cpp` | Dev-only artificial lag + packet loss injection. |
| `mapfetch.h/cpp` | Download/upload maps from/to the community map server. SHA-1 verified. |

## Lobby (`Lobby`)

Connects to the lobby server via TCP. States are declared inline as an anonymous enum:
`IDLE → RESOLVING → CONNECTING → CONNECTED → CHECKINGVERSION → AUTHENTICATING → AUTHENTICATED`

Key entry points:
```cpp
lobby.Connect(host, port);
lobby.SendCredentials(user, pass);
lobby.DoNetwork();               // call every tick; drives state machine + receives messages
lobby.CreateGame(name, map, …);
```

- `updateavailable` / `updateurl` / `updatesha256` — set when lobby rejects with an update payload; read by `Updater`.
- `SetLocalUsername` / `GetLocalUsername` — captured before `SendCredentials`, read by CharacterPanel.

## ControlServer / ControlDispatch

JSON-over-TCP socket on a local port for automation (test harness, `silencer-cli`, agents).

Commands are classified into three phases:
- `IMMEDIATE` — handled before the sim tick.
- `POST_RENDER` — handled after the frame is rendered (e.g. screenshot).
- `MULTI_FRAME` — handled by `ControlDispatch::TickWaits` after the sim loop (e.g. `wait --frames N`).

```cpp
server.Start(port, onShutdownDrain);
auto cmds = server.DrainImmediate();   // call each game tick
auto cmds = server.DrainPostRender();  // call after render
ControlDispatch::HandleImmediate(game, cmd);
ControlDispatch::HandlePostRender(game, cmd);
ControlDispatch::EnqueueWait(game, cmd);
ControlDispatch::TickWaits(game);      // AFTER the sim loop, every tick
```

`ControlCommand::reply` is a `shared_ptr<promise<ControlReply>>`. Handler must call `reply->set_value(…)`.

## InputServer

One-way binary socket; latest-wins per channel. Three wire types:

| Type | Bytes | Description |
|---|---|---|
| `0x01` INPUT_SNAPSHOT | 12 | Action-level state (Input fields directly). Bypasses keymap. |
| `0x02` SCANCODE_SNAPSHOT | 64 | SDL scancode bitmask. Honors the user's keymap profile. |
| `0x03` MOUSE_SNAPSHOT | 5 | `[u16 x][u16 y][u8 buttons]` in engine-pixel space. |

Protocol: client sends 1 byte version (`0x01`), server rejects unknowns.

## MapFetch

```cpp
FetchMapFromServer(mapname, sha1, apiURL, &progress);  // → saved path or ""
FetchServerMapList(apiURL);                            // → [(name, sha1hex), …]
FetchAndSyncServerMaps(apiURL);                        // sync all missing maps (called at Create Game)
UploadMapToServer(mapname, filepath, apiURL);          // POST /api/maps
```

Maps are saved to `level/download/<mapname>` under the data directory.

## LagSimulator

Dev-only. Wrap outgoing UDP packets with `QueuePacket`; call `Process(world)` each tick to re-deliver them after a random delay within `[minLatency, maxLatency]` ms. `packetloss` is an integer percentage. Deactivated (`Active() == false`) in production builds.

## Rules

- `ControlServer` and `InputServer` own their listener threads. Always call `Stop()` before destruction.
- `ControlDispatch::TickWaits` must be called **after** the sim loop (not before), so `wait --frames 1` sees at least one tick.
- Do not write game logic in `controldispatch.cpp`; call `Game`/`World` methods and return a `ControlReply`.
- `UpdaterDownload::IsAllowed` must be called before any URL is fetched — `mapfetch.cpp` is exempt (uses its own curl/HTTP, no `Updater` pipeline).
