# src/ — C++ client

Flat layout: ~158 files, `.cpp` paired with `.h`, no subdirectories.
General context (build, dedicated-server contract, data-dir + lobby
hardcoding gotchas, version string) lives in the root `CLAUDE.md`
and is not repeated here.

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
