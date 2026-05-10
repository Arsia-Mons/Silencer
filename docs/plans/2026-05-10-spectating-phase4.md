# Spectating — Phase 4: Spectator controls

**Status:** Design
**Date:** 2026-05-10
**Branch:** `hv/spectator` (PR #148, umbrella for all four phases)
**Builds on:** Phase 3
([progress](2026-05-09-spectating-progress.md),
[design](2026-05-09-spectating.md)).

## Scope

The fourth and final phase of spectating. Phase 3 landed the connect /
admit / snapshot pipeline; the spectator joins, but the camera is
pinned at default position and `ESC` does nothing (observers have no
`Player`, so the existing quitstate path in
`events.cpp:362-373` short-circuits). Phase 4 adds the controls.

Confirmed decisions (2026-05-10):

| Decision | Choice |
|---|---|
| Default camera mode on join | Follow first living player |
| HUD policy | Render followed player's HUD as-is |
| World-interacting inputs | None — view-only |
| Time control (slow-mo / rewind / pause) | None — realtime |

Everything below follows from those four picks plus the "fix ESC"
carryover.

## Controls

| Input | Action |
|---|---|
| `Tab` | Cycle to next player |
| `Shift+Tab` | Cycle to previous player |
| `WASD` / arrows | Switch to free-cam, pan |
| Hold jump key | Show all player names (overlay) |
| `F1` | Scoreboard (already global) |
| `F2` | Toggle team colors (already global) |
| `ESC` → confirm | Leave match → back to lobby |
| Click on a player on the minimap | Follow that player |

The first six already exist for the replay viewer in
`tick_ingame.cpp:96-190`. The spectator code path mirrors the same
control-to-state mapping; the difference is where the state lives
(see below) and that the renderer's HUD branch keeps running, since
we want the followed player's HUD.

The minimap click is new behavior the replay viewer doesn't have —
included here because it's the natural live-spectator affordance.
If it adds non-trivial complexity at implementation time, drop it
and keep `Tab` / `Shift+Tab` as the only target-switching path.

## State

One new field on `World`:

```cpp
int viewedpeerid;     // peer whose POV/HUD the local client renders
```

Plus a small embedded struct for free-cam:

```cpp
struct SpectatorView {
    bool   freecam;   // true = camera follows camx/camy, ignores viewedpeerid
    int    camx, camy;
    int    camvx, camvy;
} spectator;
```

Invariants:

- For normal players, `viewedpeerid == localpeerid` always; `spectator`
  is unused.
- For an observer, `viewedpeerid` is some peer's id (initially the
  first living player; `Tab` cycles it). When `spectator.freecam` is
  true, the camera reads `camx/camy` instead of the followed player.
- `viewedpeerid` is **purely client-local**. It is never serialized,
  never sent over the wire. Network identity stays on `localpeerid`.

The replay system already has analogous state on `Replay`
(`x`, `y`, `xv`, `yv`, `speed`, `showallnames`) and an ad-hoc trick
of reassigning `world.localpeerid` to switch followed peer
(`tick_ingame.cpp:162-178`). We do not reuse that trick for live
spectator — reassigning `localpeerid` would corrupt the observer's
network identity (chat sender, disconnect routing, etc.). Keeping the
identity peer and the camera peer separate is the right shape; the
replay code can be migrated to the same pattern opportunistically
later (out of scope for this phase).

## Renderer

`render/renderer.cpp:78` currently does:

```cpp
localplayer = world.GetPeerPlayer(world.localpeerid);
```

Change to:

```cpp
localplayer = world.GetPeerPlayer(world.viewedpeerid);
```

For normal players this is a no-op (the two ids are equal). For
observers it picks up the followed player so the existing camera
follow logic (lines 108-138) and HUD rendering downstream all "just
work" — health bar, ammo, money, minimap, scoreboard all key off the
returned `localplayer`.

Free-cam: extend the `world.pancameraactive` branch (lines 86-91) or
add a sibling `if(world.spectator.freecam)` branch that drives the
camera from `world.spectator.camx/camy` with the same velocity-clamped
pan replay uses. The replay code at `tick_ingame.cpp:108-161` is the
template — same key mapping, same map-bounds clamps.

Two other renderer touchpoints use `world.localpeerid` directly:

- `renderer.cpp:1353` — minimap "you are here" pip. For observers we
  do **not** show a pip (the observer has no in-world body). Guard
  this call with `!world.peerlist[world.localpeerid]->observer`.
- `renderer.cpp:2959` — chat/buy interface routing. Leave as
  `localpeerid` (chat identity is the observer themselves).

`renderer.cpp:1962` is the debug overlay — leave as `localpeerid`.

## Name overlay (hold-jump)

`renderer.cpp:827` currently renders names when:

```cpp
(localplayer && player->GetTeam(world) == localplayer->GetTeam(world)
    && player != localplayer)
|| (world.replay.IsPlaying() && world.replay.ShowAllNames())
```

Extend the second clause to fire for live spectators too:

```cpp
|| ((world.replay.IsPlaying() && world.replay.ShowAllNames())
    || (world.IsLocalObserver() && world.spectator.holdshowallnames))
```

`World::IsLocalObserver()` is a one-line helper checking
`peerlist[localpeerid]->observer`. `holdshowallnames` is a bool flag
on `SpectatorView` set/cleared from the jump-key state each tick,
mirroring `tick_ingame.cpp:186-190`.

## Input handling

A new spectator-controls block lives alongside the replay-controls
block in `tick_ingame.cpp` (currently lines 96-190). Conditions are
mutually exclusive — replay block runs iff `world.replay.IsPlaying()`,
spectator block runs iff `world.IsLocalObserver()`. Normal player path
is unchanged.

The spectator block does, in order each tick:

1. **Cycle (`Tab`/`Shift+Tab`).** Edge-triggered on
   `keynextcam`/`keyprevcam` (already wired in keymap; replay uses
   them too). Step `viewedpeerid` to the next/previous peer that has
   a `Player` in `controlledlist`, skipping AUTHORITY and observers.
   Wrap on overflow. If no candidate exists, leave `viewedpeerid`
   alone.
2. **Free-cam (`WASD`/arrows).** If any movement key is held, set
   `spectator.freecam = true` and update `camx/camy/camvx/camvy` with
   the replay pan model (`tick_ingame.cpp:108-161`). On re-press of
   `Tab`/`Shift+Tab`, clear `freecam` and snap back to following.
3. **Hold-to-show-names (jump key).** Mirror lines 186-190 onto
   `spectator.holdshowallnames`.
4. **ESC (quit).** Independent of the spectator block — handled in
   `OnScancodeDown`; see next section.

We do **not** read input state by polling the keymap directly here —
we read `world.localinput` / `world.localinputhistory` exactly like
replay does, for consistent edge detection.

Spectator input is **not** sent over the wire. Phase 3 already drops
observer `MSG_INPUTSTATE` at the network layer; that stays. The
"input" referenced above is purely local: `keynextcam`, `keyprevcam`,
`keymoveleft/right/up/down`, `keyjump` — these are populated from the
keymap into `localinput` regardless of observer status.

## ESC trap fix (Phase 3 carryover)

`game/events.cpp:362-373`:

```cpp
if(sc == quitscancode){
    Player * localplayer = world.GetPeerPlayer(world.localpeerid);
    if(localplayer && !localplayer->chatinterfaceid && !localplayer->buyinterfaceid){
        if(world.quitstate == 0)      world.quitstate = 1;
        else if(world.quitstate == 2) world.quitstate = 3;
    }
}
```

Change to allow observers to advance the quitstate without needing a
`Player`:

```cpp
if(sc == quitscancode){
    Peer * lp = world.peerlist[world.localpeerid];
    bool isobserver = lp && lp->observer;
    Player * localplayer = world.GetPeerPlayer(world.localpeerid);
    bool playerok = localplayer
        && !localplayer->chatinterfaceid
        && !localplayer->buyinterfaceid;
    if(isobserver || playerok){
        if(world.quitstate == 0)      world.quitstate = 1;
        else if(world.quitstate == 2) world.quitstate = 3;
    }
}
```

The chat/buy interface checks are irrelevant for observers (no buy
modal; chat coercion in Phase 3 routes through the existing chat
interface and doesn't block ESC any differently than for a player).

## Default-mode logic

On observer connect, `viewedpeerid` initializes to `localpeerid` (the
observer themselves) as a safe default. On the first tick after the
peer-list confirms at least one non-observer non-AUTHORITY peer has
a `Player`, set `viewedpeerid` to the first such peer's id. If none
exists yet (rare — the observer connects after the dedicated server
spawns but before any player has joined / spawned), leave it at
`localpeerid` and the renderer's `localplayer == null` branch keeps
the camera idle until a player appears, at which point the same
"first living" search picks one.

"Living" means: peer has at least one entry in `controlledlist` AND
the controlled `Player` is not in `Player::DEAD` state. If the only
choice is a dead player, pick them anyway — the spectator can `Tab`
off if they want.

## What we do NOT add

Explicitly out of scope for Phase 4 (closing the design loop on the
open questions from `2026-05-09-spectating.md` §Phase 4):

- **No spectator-only HUD.** Followed player's HUD renders as-is.
- **No ping / mark / door / world-interacting inputs.**
- **No live rewind, scrub, pause, slow-mo, or fast-forward.**
- **No spectator chat beyond what Phase 3 already enables.** Phase 3
  already coerces observer team-chat to all-chat.
- **No "X is spectating" indicator to players.** Players see no
  evidence of spectators beyond what the peer-list naturally exposes.
- **No spectator count in lobby or in-game.**
- **No per-game spectator cap** beyond the existing 25-slot
  `maxpeers` pool already enforced by Phase 3.

If any of these come up during implementation as cheap add-ons, defer
them anyway — keep Phase 4 to the controls scope and ship.

## Smoke test (closes Phase 3 carryover)

Three-client smoke against a local lobby (host + player + spectator):

1. Host creates a game with the **Spectatable** checkbox on.
2. Player joins normally.
3. Spectator joins via the **Spectate** button on the game-select
   panel.
4. Verify the spectator's view follows the player by default.
5. Press `Tab` — verify it cycles (only meaningful with two
   players; with one player it should no-op).
6. Press `WASD` — verify free-cam pan; press `Tab` — verify snap
   back to follow.
7. Hold jump — verify all names show; release — names disappear.
8. Press `ESC` → confirm — verify spectator returns to lobby; host
   and player continue uninterrupted.
9. Spectator chat: type a message — verify host and player see it
   as all-chat regardless of team setting.
10. Spectator disconnects abruptly (kill window) — verify slot is
    immediately freed on AUTHORITY (Phase 3 behavior; re-confirm
    here).
11. Match ends naturally — verify spectator is returned to lobby
    alongside the players.

Phase 3 local-smoke setup (lobby on `:15170` with isolated DB) is
already documented in
[progress](2026-05-09-spectating-progress.md#handoff-prompt) — reuse it.

## Implementation order

1. **ESC trap fix** (`events.cpp:362-373`). Smallest, unblocks
   manual testing of everything else.
2. **`viewedpeerid` + renderer line 78 swap.** Make the field
   default to `localpeerid` for everyone; verify nothing regresses
   for normal players or replay.
3. **Default-mode follow on join.** Observer initializes
   `viewedpeerid` to first living player. Verify spectator sees
   gameplay on connect (no Tab needed).
4. **Tab / Shift+Tab cycle.**
5. **Free-cam pan (WASD/arrows).**
6. **Hold-jump name overlay** (extend renderer.cpp:827).
7. **Minimap pip guard** (renderer.cpp:1353).
8. **Minimap-click follow** (optional — drop if non-trivial).
9. **Three-client smoke** above.

Each step is independently testable; commits map 1:1.

## References

- Phase 3 design spec:
  `docs/superpowers/specs/2026-05-10-spectating-phase3-design.md`.
- Phase 3 plan:
  `docs/superpowers/plans/2026-05-10-spectating-phase3.md`.
- Umbrella design: [`2026-05-09-spectating.md`](2026-05-09-spectating.md).
- Progress tracker: [`2026-05-09-spectating-progress.md`](2026-05-09-spectating-progress.md).
- Replay system (closest prior art): `clients/silencer/src/game/replay.{h,cpp}`,
  input handling in `clients/silencer/src/game/tick/tick_ingame.cpp:96-190`.
