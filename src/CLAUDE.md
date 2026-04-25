# src/ — C++ client

Flat layout: ~158 files, `.cpp` paired with `.h`, no subdirectories.
Build with the root `CMakeLists.txt` (`mkdir build && cd build && cmake ..
&& make`) — see top-level `README.md` for platform notes and the
`-DZSILENCER_LOBBY_*` knobs in *Gotchas* below.

## Object hierarchy

`Object` → `Hittable` → `Physical` → `Bipedal` → {`Player`,
`Civilian`, `Guard`, `Robot`}. Type registry / factory:
`objecttypes.cpp`. Live objects replicate over the wire via
`serializer.cpp` (bit-aligned little-endian). Adding a replicated
field means updating `Serialize()` and bumping the version so old
clients don't desync.

## Networking model

Peer-to-peer over UDP from `world.cpp`. One peer is AUTHORITY — it
runs simulation; others send inputs and apply deltas. Dedicated mode
(`-s`) = permanent AUTHORITY with no SDL video/audio. Snapshot ring
in `oldsnapshots` / `totalsnapshots`. TCP lobby client is
`lobby.cpp` + `lobbygame.cpp`; wire format is mirrored in
`server/protocol.go` — changes must land on both sides.

## Dedicated-server contract

The same binary runs the client and, when launched with `-s`, a
headless dedicated server:

```
zsilencer -s <lobbyaddr> <lobbyport> <gameid> <accountid>
```

- Parsed in `main.cpp:160` → `game.cpp:132`.
- Spawned by the Go lobby in `server/proc.go` on each `MSG_NEWGAME`.
- Skips `SDL_Init(VIDEO)` and audio; RSS ~12 MB.
- Heartbeats UDP to the lobby: `[0x00][gameid u32][port u16][state u8]`.
  No heartbeat in 30 s → lobby aborts the create.

## Where to look

- Top-level state machine (menus, lobby, in-game): `game.cpp` (5800+ lines).
- Simulation loop, socket, peer list, replay: `world.cpp`.
- Rendering: `renderer.cpp`, `surface.cpp`, `sprite.cpp`, `palette.cpp`.
- Audio (skipped in `-s`): `audio.cpp`.
- UI widgets: `interface.cpp`, `button.cpp`, `textbox.cpp`,
  `selectbox.cpp`, `scrollbar.cpp`, `toggle.cpp`, `overlay.cpp`,
  `minimap.cpp`.
- Projectiles: `*projectile.cpp` + `shrapnel.cpp`.
- Stations: `healmachine`, `creditmachine`, `inventorystation`,
  `techstation`, `walldefense`, `fixedcannon`, `terminal`.

## Gotchas

- **Lobby host is a compile-time constant.** Baked in via
  `-DZSILENCER_LOBBY_HOST=<host> -DZSILENCER_LOBBY_PORT=<port>`
  (`CMakeLists.txt:48`, used at `game.cpp:4018`/`:4032`). Default is
  `127.0.0.1:517`. CI sets it to `silencer.hventura.com`. Rebuild
  the client to point at a different lobby.
- **Version string must match the lobby.** Set at `game.cpp:31`
  (`world.SetVersion("00024")`); the lobby's `-version` flag
  defaults to the same. Bump both together. `CPACK_PACKAGE_VERSION`
  is installer metadata only — unrelated to the wire handshake.
- **macOS data dir.** Client `chdir`s to
  `~/Library/Application Support/zSILENCER` at startup
  (`main.cpp` `CDDataDir`) — copy `shared/assets/` contents there
  or run from the repo with the binary in place.
- **Android/Ouya code paths exist** in `main.cpp` but are not
  actively maintained; don't rely on them.
