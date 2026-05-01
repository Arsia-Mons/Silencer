# Changelog

All notable changes to Silencer are documented here.

## [v00043] — 2026-05-01

### Admin dashboard

#### Items Tool (`/items`) — new page

- **Item list** — all items from `items.json` in a sidebar.
- **Property panel** — inline-editable identity (id, enumId, name, description), sprite (bank + index), purchase prices, tech-tree (techChoice bitmask, techSlots), agency restriction dropdown (All / per-agency), stats & effects (ammo, heal, poison).
- **GAS store integration** — folder opened in `/items` (or any other tool) stays loaded when navigating across tools without re-picking.
- Saves back to `items.json` via browser download.

## [v00042] — 2026-04-30

### Admin dashboard

#### Weapon Tool (`/weapons`) — new page

- **Weapon list** — all weapons from `weapons.json` with search/filter.
- **Property panel** — sprite bank pickers (8-directional), sound pickers per event (`soundFire`, `soundHit1/2`, `soundLoop`, `soundExplosion`, `soundLand`, `soundThrow`) with live preview links to Sound Studio.
- **Ballistics preview** — tick-accurate canvas simulation: gravity, velocity, rocket hover, plasma gravity, explosion radius, splash damage labels. Grenade arc, rocket loft, EMP/neutron effects all simulated at 24 ticks/sec.
- **Agency loadout editor** — checkbox grid showing which agencies carry each weapon; saves back to `agencies.json`.
- **Shared GAS store** — folder opened in `/weapons` stays loaded when navigating to `/gas` or `/sound-studio` without re-picking the folder.

#### GAS editor (`/gas`)

- **Restore sync fix** — `↩ RE-ADD` and `↩ RE-ADD ALL` now update the shared GAS store so restored fields are not lost on navigation.

#### Sound Studio (`/sound-studio`)

- **Styling parity** — page now inherits the global dark theme instead of rendering with hardcoded inline background/color/font overrides.

#### Admin UI

- **Animated space background** — programmatic starfield (350 stars with twinkling), comets with gradient fading tails, slowly drifting Mars surface image, mouse parallax (±25 px, smoothed).

### Game client

- **Bundled map upload skip** — maps shipped inside the app bundle (`Resources/`) are pre-loaded on the server and no longer trigger a redundant upload attempt on game start.
- **FindMap absolute path fix** — `FindMap()` now captures an absolute path immediately after `CDResDir()` so the fallback path is always absolute regardless of the current working directory.
- **GAS: all projectile classes data-driven** — blaster, laser, rocket, flamer, plasma, flare, wall, and grenade projectiles read sprite banks, sounds, and physics from `weapons.json` with hardcoded fallbacks (zero behavior change).
- **Bug fixes from code review:**
  - Neutron bomb and EMP bomb: `0` damage values were incorrectly treated as "not set" and fell back to `0xFFFF` (max damage). Fixed.
  - Rocket launch sound: was reading from `soundLand` (landing/bounce field) instead of `soundFire`.
  - `std::min` macro clash on Windows in flamer/flare projectile constructors fixed with `(std::min)()`.

### Infrastructure

- **Docker build context fixed** — `admin-web` and `admin-api` Dockerfiles use the repo root as build context (workspace lockfile + all `package.json` manifests live there). `docker-compose.yml` context pointers updated to `..` with explicit `dockerfile:` paths.
- **Compiled-in production defaults** — CMakeLists.txt now defaults to `lobby.arsiamons.com`, `maps.arsiamons.com`, and `admin.arsiamons.com` so release builds connect to production without manual config.

---

## [v00041] — 2026-04-28

### Admin dashboard

#### GAS editor (`/gas`)

- **URL-linked tabs** — each GAS file tab updates the URL (`?tab=filename`) so tabs are bookmarkable and survive page refresh.
- **Baseline validation** — on folder open the editor captures a baseline snapshot of all loaded JSON. Saving is blocked if any field that existed at load time has been removed, including fields inside array entries matched by `id` (weapons, enemies, items, etc.).
- **Problems panel** — a VS Code-style inline tray below the tab bar lists every validation violation with full field path. Clicking a file header in the tray jumps to that tab.
- **RE-ADD button** — each violation row shows a `↩ RE-ADD` button that restores the missing field from the baseline in one click.
- **RE-ADD ALL** — each file section header in the Problems panel shows `↩ RE-ADD ALL (N)` to restore all missing fields for that file in a single atomic update.
- **Direct tab URLs** — each tab is an `<a>` tag so tabs can be opened directly or middle-clicked.

### Game client

- **Community map upload** — client uploads the current map to the server before creating a game, so other players can download it. Upload URL read from `mapapiurl` config key.

### Infrastructure

- **nlohmann/json vendored** — `json.hpp` (v3.12.0) checked in to `clients/silencer/third_party/nlohmann/` to eliminate a flaky CMake FetchContent download step in CI.
- **Map symlinks** — lobby server maintains a `maps/` symlink directory so the dedicated server binary can read uploaded maps without a restart.

---

## [v1.9.0 / v00029] — 2026-04-26

### Game client / dedicated server

#### Behavior tree AI

- **C++ behavior tree interpreter** — loads `.json` BT files from the assets
  directory and evaluates them each tick. Supports Sequence, Selector,
  Inverter, and a full set of game-specific Condition/Action leaf nodes.
- **Guard AI wired to behavior tree** — full combat pipeline (Look, Aim, Shoot,
  Crouch, Patrol) now driven by the BT. `guard.cpp` state machine replaced by
  BT tick calls.
  - `SearchAndReturn` — non-patrol guards search the last known target position
    then walk back to spawn.
  - Ladder climbing in SearchAndReturn — guards climb/descend ladders during
    search phase with 2 s cooldown and 48 px vertical gap requirement to
    prevent stuck loops.
  - Fixed: crouch-shoot, stay-at-post patrol, stop gliding during crouch
    states, back-away-when-too-close, clear chasing on player death / base
    entry / untargetable.
- **Robot AI wired to behavior tree** — `robot.cpp` uses BT for patrol,
  `ReturnToSpawn` (replaces old Sleep), damage wakeup, `LookSides` returning
  Failure so Patrol runs every tick.
- **Civilian flee BT wired** — `civilian.cpp` flee logic driven by
  `civilian.json` behavior tree.

#### Actor definition system

- **Client syncs actordefs from server on startup** — fetched async on each
  map load so the admin tool changes are picked up without a client rebuild.
  Uses a separate `adminapiurl` config key to avoid affecting lobby traffic.
- **Per-frame sounds — data-driven** — `FrameDef` gains `sound` + `soundVolume`
  fields. `AnimSequence::GetFrameSoundByIndex(frameIdx)` looks up sound by
  sprite frame index (correct for `state_i % N` state machines).
- **Guard WALKING footsteps** — `guard.cpp` calls `GetFrameSoundByIndex` with
  `state_i % 19` to play `stostep1.wav` / `stostepr.wav` at frames 4/13,
  driven by `guard-*.json` actordefs — fully configurable in the actor editor.
- **Civilian footsteps** — `civilian.cpp` WALKING and RUNNING use
  `GetFrameSoundByIndex` to play footstep sounds from `civilian.json`.
- **Per-weapon guard actordefs** — `guard.json` split into
  `guard-blaster.json`, `guard-laser.json`, `guard-rocket.json`. Each can now
  have independently tuned animations, hurtboxes, and sounds. `ActorDefName(weapon)`
  helper maps weapon integer to the correct file name.
- **Body parts replicated to clients** via snapshot packets — previously only
  simulated on the server.
- **Player hurtboxes** — all player animation sequences (WALK, RUN, JUMP,
  CROUCH, etc.) now have default hurtboxes in the actordef; fixes bullet
  collision regression.
- **Guard kneel loop fixed** — CROUCHED uncrouch guard no longer loops endlessly.
- **`Look()` origin restored** to y=−55 for reliable target detection.
- **Version bumped to `00028`**.

### Admin dashboard

#### Behavior tree editor

- Visual drag-and-drop BT editor with node palette, JSON preview, and full
  undo/redo.
- **Local-file mode** — editor reads/writes `.json` files directly from a
  user-selected folder (no database required). `webkitdirectory` input on HTTP,
  `showSaveFilePicker` on HTTPS.
- Download button exports the current tree as a `.json` file.
- Blackboard key editor improved: sortable key list, type badges, one-click
  delete.
- State machine tab removed from actor editor — superseded by behavior trees.

#### Actor editor

- **Local-file mode** — load/save actordefs from a local folder via browser
  file picker; no MongoDB write path in production.
- **Auto-size preview canvas** — canvas resizes to the largest sprite in the
  selected sequence.
- **Tab URL persistence** — `?tab=` query param keeps selected tab across
  navigation.
- **Hitbox editor**:
  - Auto-fit hurtbox button snaps the box to the non-transparent pixel bounds
    of the current sprite frame.
  - Clear all hurtboxes button.
  - All player animation sequences (WALK, RUN, CROUCH, etc.) available for
    editing.
- **Animation tab**:
  - Sound picker — searchable dropdown of all 98 in-game sounds, plus a ▶
    preview button per row that plays the WAV at the configured volume.
  - `soundVolume` field (0–128) per frame; preview respects it.
  - Scale toggle: 1×/2×/3×/4× preview size (defaults to 1×).
  - Grid background — matches the hitbox tab dark grid so transparent sprites
    are visible.

#### Lobby server (Go)

- Behavior trees stored in MongoDB and synced to the game client on startup.
- Actordefs and BTs migrated to filesystem-first (MongoDB write-through
  removed from the read path).
- `–version 00028` passed to lobby process in `docker-compose.yml`.
- MongoDB password redacted from `mongosync` log lines.
- Empty `–version` flag now falls back to manifest version (not crash).

### Infrastructure

- **Assets volume mounted read-write** in `docker-compose.yml` so `admin-api`
  can write actordefs back to disk from the actor editor.
- **Dedicated server bundle** — `SDL3` and `SDL3_mixer` `.so` files copied into
  the server package by `install-linux-server.sh`; fixes missing-library crash
  on fresh ARM64 hosts.
- **Lobby flags pinned**: `–maps-dir /var/lib/silencer/maps`,
  `–update-manifest /opt/silencer/update.json`.
- **CI (macOS)**: `SDL3_ROOT` env vars passed to `dylibbundler`; search dirs
  narrowed to avoid scanning the full filesystem during macOS release builds.

---

## [v00025] — 2026-04-25

### Game client / dedicated server

- **Community map downloads** — server-published maps now appear in the
  Create Game map list with an inline `[DL]` badge. Clicking the badge
  starts an async background download (no UI freeze). A progress bar fills
  the row (0–100%) while the file transfers via curl XFERINFO callback. On
  completion the interface rebuilds automatically so the map preview renders
  immediately. The Create button is blocked while a not-yet-downloaded map
  is selected.

- **Agency switch in pregame lobby** — switching agencies now broadcasts
  `MSG_SETAGENCY` to the dedicated server, which reassigns the peer to the
  correct team immediately. Previously the server only recorded agency at
  `MSG_CONNECT`; rejoining the lobby was required to see the change.
  Agency logo overlays now update in real-time when a peer switches.

- **Kick banned players from active games** — banning a player via the
  admin UI now sends `MSG_KICK` (UDP) to the dedicated server hosting their
  current game. The server authenticates the message by checking the sender
  IP matches the lobby address, then calls `KillByGovt`/`HandleDisconnect`
  to eject the player back to the main menu.

- **Tile-flip crash fix** — `patchTile` used a stale closure over
  `width`/`layers`, causing a client-side exception when flipping a tile on
  the X axis in the designer.

### Map designer (admin web)

#### Community map publishing

- **Publish maps from the designer** — maps can be uploaded to the lobby's
  community map API (`POST /api/maps`, multipart/form-data, SHA-1 indexed).
  `GET /api/maps` lists all published maps; `GET /api/maps/by-sha1/:sha1`
  serves the raw `.sil` for client download with hash verification.

#### Platform collision rendering

- **Diagonal cross-stitch restricted to stairs/ground** — the collision
  overlay cross-stitch pattern now only renders on `RECTANGLE`,
  `STAIRSUP`, and `STAIRSDOWN` platforms. `RAIN`, `ROOM`, ladders, and
  other volume types are no longer cross-stitched, matching their
  semantic meaning in the engine.

#### Platform tools

- **RAIN / OUTSIDEROOM platform** (`🌧 RAIN`, type1=3 type2=0) — the volume
  type the engine uses to calculate rain-puddle spawn locations; now
  available in the Toolbar.
- **ROOM / SPECIFICROOM platform** (`▣ ROOM`, type1=3 type2=1) — added
  alongside RAIN.
- **Platform drag fix** — all three event handlers (mouseDown / mouseMove /
  mouseUp) now include the new platform types so dragging to place volumes
  works correctly.

#### Platform selection, resize, and move (SELECT tool)

- Click any platform volume in SELECT mode to select it; an animated
  marching-ants outline appears.
- Eight white square handles (TL / T / TR / L / R / BL / B / BR) allow
  resizing by dragging. Corner handles move two edges; mid-point handles
  move one edge. Minimum size enforced at 16 world units.
- Drag the body of a selected platform to move it without resizing.
- Click empty space to deselect.
- Resize/move operations are added to undo history via `updatePlatform`.

#### Actor marching-ants selection

- Selecting an actor in SELECT mode or clicking an actor in the Actors
  panel now shows an animated dashed-outline highlight around the sprite
  bounds, drawn on the pointer-events-none overlay canvas.

#### Earlier designer additions (since v00024)

- **Erase tool** — removes tiles, actors, or platforms under the cursor.
- **New map / Map properties** — create a blank map or edit name,
  width, height of the current map; map resize preserves existing content.
- **Actor drag** — actors can be repositioned by dragging in SELECT mode.
- **Tile filter** — search tiles by name in the tile picker.
- **Keyboard shortcuts** — S(elect), T(ile), P(latform), A(ctor),
  E(rase), Ctrl+Z/Y undo/redo, Ctrl+drag to pan.
- **Actor list panel** — lists all placed actors; click to select and
  center the viewport.
- **Minimap** — thumbnail overview of the full map; click to jump.
- **Save dialog** — uses File System Access API with a download fallback
  so `.sil` files can be saved directly from the browser.
- **Actor 60 (Camera Focus)** — designer-only placement marker for
  scripting camera positions; no `map.cpp` handler required.
- **Actor types 47 / 50 / 61 / 67 / 68 / 69** — added with correct
  labels; Laser Defense label corrected.
- **Powerup actor** — correct 7 subtypes with dynamic sprite bank lookup.
- **Pickup type list** — corrected to match `pickup.h` enum (21 types).
- **Tile right-click context menu** — Flip X, LUM toggle, Clear tile.
- **Visibility toggles** — independently show/hide tile layers, platforms,
  actors, grid, and lighting overlay.
- **STAIRSUP / STAIRSDOWN rendering** — drawn as triangles matching
  in-game appearance.
- **Terminal sprite** and actor right-click properties panel.
- **Actor sprites** — rendered from the game's sprite banks (128×128
  sheets) rather than placeholder icons.
- **Per-cell luminance** — ambient/dark lighting applied per tile via
  `ctx.filter` brightness; LUM flag and toggle button added.
- **Undo / redo history** — full undo stack for tile paint, actor place,
  platform draw, and all property edits.
- **Map resize** — width/height resize via UI.
- **Performance** — eliminated per-tile `ctx.filter` changes for a large
  rendering speedup on dense maps.

---

## [v00024]

- **Auto-updater** — client consent modal + in-place binary swap driven by
  a lobby manifest; players are prompted when a new version is available.
- **Lobby presence sidebar** — shows which players are currently online in
  the main lobby.
- **libcurl + libminizip** — added to both cloud-init (Terraform VM
  provisioning) and the GitHub Actions Deploy workflow so the dedicated
  server binary builds correctly on ARM64.

---

## [v00023] and earlier

- Self-hosted Go lobby server replacing the defunct `lobby.zsilencer.com`.
- Headless dedicated-server mode (`zsilencer -s …`).
- Admin dashboard (Express API + Next.js frontend): player management,
  audit log, game stats, community leaderboard, ban/unban, password reset.
- Community game-stats page (`/gamestats`) with drill-in rows, sorting,
  agency filter, and search.
- How-to-Play guide (`/howto`) with original game images from the Wayback
  Machine.
- 200 concurrent game slots (ports 20000–20199).
- One-script Linux server install (`scripts/install-linux-server.sh`).
- AWS Terraform module (EC2 + EBS + Tailscale) for cloud hosting.
- GitHub Actions workflows: tag-triggered ARM64 lobby deploy, macOS +
  Windows client release zips.
