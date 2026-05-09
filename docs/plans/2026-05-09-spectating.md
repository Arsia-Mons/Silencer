# Spectating

**Status:** Tracking — design not finalized
**Date:** 2026-05-09

## Goal

Let players watch a running multiplayer match without participating
in it. A spectator can join a running game **at any time**, follow
whichever player they choose, and leave.

## Confirmed so far

- **Spectator pawn model:** attached follow-cam on existing players
  (no in-world body), cycle between players like the replay system's
  peer-cycle UX.
- **Late join:** spectators can join at any time during a running
  match, not only at game start.
- **Four user-facing surfaces** (one per phase below):
  1. Spectatable checkbox at game creation.
  2. Server browser shows running games with a spectatable affordance.
  3. Click a spectatable game to join as a spectator.
  4. Spectator controls (follow / cycle / etc.).

Everything else is open.

## Phases

### Phase 1 — Game-creation: spectatable flag

A boolean on each game, set at creation time, indicating whether the
game accepts spectators. Surfaces:

- New checkbox in the create-game dialog (client UI).
- Field on the lobby-side `LobbyGame` record.
- Replicated to clients in the lobby's game-list payload so the
  server browser can render the affordance.

**Confirmed:**
- **Default checkbox state:** on, but the client remembers the
  user's last setting and uses that on subsequent create-game
  dialogs.
- **Mid-game toggle:** no. Locked at creation.
- **Password-coupling:** if the game is password-protected, joining
  as a spectator requires the same password. (Spectatable + private
  is allowed; the password gate applies equally to players and
  spectators.)

### Phase 2 — Server browser: spectatable affordance

The lobby's running-games list (right-side panel of the lobby
screen) gains a visual indicator on rows that allow spectators, plus
a way to invoke "join as spectator" distinct from "join as player".

**Open questions:**
- Visual treatment: icon, badge, text label?
- Show current spectator count on the row?
- Treatment of full-but-spectatable games vs. spectatable-but-empty.
- Click action model: separate button, right-click, modal, …?

### Phase 3 — Joining as spectator (any time)

Click → connect to the dedicated server in observer mode → receive
a snapshot of current world state → render. Joining mid-match is a
first-class case.

This is the largest engineering item. Surfaces it touches:

- **Client connect path.** Today `JoinGame` always picks an agency
  and joins as a player `REPLICA`. We need a path that skips agency/
  team selection and flags the local peer as observer.
- **Lobby protocol.** A way for the lobby to hand a spectator off to
  a running dedicated server. Could be a new opcode or could reuse
  the dormant `opConnect=6` slot — design decision.
- **World/peer model.** Today the world has only `AUTHORITY` /
  `REPLICA` modes and `Peer` carries `agency` + team data. Need
  either an observer flag on `Peer` or a third world mode.
- **Mid-match join path.** Today the connection handshake assumes
  pre-game lobby (peer-list, map download, `MSG_READY`,
  `MSG_GAMEINFO`). Spectators need a "snapshot resync from current
  tick" path. This is engineering work to be designed and built.
- **Snapshot replication.** Server's snapshot path filters by team/
  relevance. Needs an observer-aware path.
- **Peer cap.** `maxpeers = 25` is shared by the player set today.
  Either spectators count against it or live in a parallel pool.

**Open questions:**
- Auth: who can spectate? (anyone, logged-in only, friends-of-host,
  admin-only, …)
- Per-game spectator cap and where it's enforced.
- Visibility model: full unfiltered state, or team-relative fog of
  war?
- Reuse `opConnect=6` or add a new opcode?
- Does the dedicated server need its own "accept spectators" flag,
  or is the lobby flag authoritative?
- Are spectators visible to players in any way (scoreboard count,
  "X is spectating" notice, nothing at all)?
- Can spectators send chat? Voice?
- Does a spectator joining mid-match trigger any side effects (pause
  timer, announcement, …)?
- What happens to spectators when the match ends?

### Phase 4 — Spectator controls

UI mirrors the existing replay system's input layer where it makes
sense. Working baseline:

- **Tab / Shift-Tab** — cycle next/previous followed player.
- **Free-cam mode** — pan with arrow/WASD keys; toggle in/out of
  follow.
- **Hold key for "show all names"** — reuse the replay overlay.
- **Disconnect** — leaves the match cleanly.

**Open questions:**
- Default mode on join: follow first player, follow random, follow
  the player who triggered the spectate, or free-cam at map center?
- Does the spectator have any inputs that interact with the world
  (open doors, ping, mark)?
- Live rewind/pause/scrub for the spectator's own view — desired or
  not?
- HUD: show the followed player's HUD as-is, or a spectator overlay
  (player name, team, kill feed, scoreboard)?
- Should spectator-side speed control exist (slow-mo / fast-forward)
  even in live mode, or strictly realtime?

## Adjacent prior art (informational, not constraints)

- **Replay system** (`clients/silencer/src/game/replay.{h,cpp}`) is
  the closest existing UX. Event-stream model, peer-cycle, free-cam,
  name-overlay. Spectator should reuse the input/render layer where
  it makes sense; the same UI may end up serving both pre-recorded
  and live streams.
- **Admin `/dashboard` "LIVE SESSIONS"** is list-only via Socket.IO
  + AMQP `silencer.events`. Unrelated to in-game spectating, noted
  here only because someone might confuse the two.
- **`opConnect=6`** is reserved/unused per the lobby protocol spec
  ("server does not currently send or process this opcode"). Not a
  required slot, just an option if we want to repurpose it.

## Status

- [ ] Phase 1 — game-creation checkbox
- [ ] Phase 2 — server browser affordance
- [ ] Phase 3 — spectator join flow (incl. mid-match join)
- [ ] Phase 4 — spectator controls
