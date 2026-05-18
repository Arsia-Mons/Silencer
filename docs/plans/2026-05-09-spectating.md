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
screen) gains a per-row "Spectate" affordance, surfaced through the
existing game-select panel
(`clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp`).

**Confirmed:**
- **Click action model — dual button.** Selecting a row contextually
  renders up to two buttons at the bottom of the panel: **Join** and
  **Spectate**. Each button is shown only when permitted for that
  user/game pair:
  - **Join** — the user has a slot. Either INLOBBY with player
    capacity, or INGAME with a parked peer slot matching the user's
    accountid (the rejoin path from
    [PR #152](https://github.com/Arsia-Mons/Silencer/pull/152)).
  - **Spectate** — the game is INGAME *and* has the spectatable
    flag set. Pre-game lobby (INLOBBY) is not spectatable.
  - Neither row showing both/either buttons is fine: a non-spectatable
    INGAME game with no parked slot for the user shows zero buttons.
- **Lobby surfaces rejoin permission per recipient.** Each running
  dedicated server reports its parked-peer accountids up to the
  lobby; the lobby derives a "can-rejoin" bit per recipient on the
  game-list payload. The client renders Join on INGAME rows from
  that bit alone — no round-trip to the AUTHORITY.
- **No row-level visual indicator.** The Spectate button appearing
  on row click is sufficient signal; no icon/badge/text in the row.
- **No spectator count.** Deferred to keep Phase 2 minimal.
- **`buttonenter` default** — Join when both visible; Spectate when
  only Spectate is visible.

### Phase 3 — Joining as spectator (any time)

Click → connect to the dedicated server in observer mode → receive
a snapshot of current world state → render. Joining mid-match is a
first-class case.

The mid-game connect mechanism is the same one
[PR #152](https://github.com/Arsia-Mons/Silencer/pull/152)
introduced for player rejoin: AUTHORITY accepts `MSG_CONNECT` while
`gameplaystate == INGAME`, the connecting peer is added to the
shared peer pool, and a `SendGameInfo` + `SendPeerList` round
serves as the resync handshake (no map download / `MSG_READY`
step). Spectator-join is a third branch of that accept block.

**Confirmed (aligned with PR #152):**
- **Reuse `MSG_CONNECT`.** Connect payload carries an `observer`
  bit. AUTHORITY's INGAME accept logic gets a third branch:
  - `accountid` matches a parked peer → rejoin (PR #152).
  - `observer` bit set + game is spectatable → admit as spectator.
  - else → reject.
  No new opcode; the `opConnect=6` repurposing idea is dropped.
- **`Peer::observer` flag** mirrors `Peer::disconnected` from
  PR #152. No third world mode — `REPLICA` plus the flag is enough.
- **Shared `maxpeers = 25` pool.** Spectators consume slots like
  rejoiners do. Per-game spectator cap (if any) layers on top.
- **Resync via `SendGameInfo` + `SendPeerList`** — same path PR
  #152's rejoin uses. No new mid-match handshake to design.
- **`SendSnapshots` gets an observer-aware branch** in the same
  shape as PR #152's `disconnected` skip. Filter rules — full
  state vs. team-relative fog of war — still open.
- **`HandleDisconnect` for an observer is always permanent.** No
  parking; the slot frees immediately on leave.
- **Lobby spectatable flag is authoritative.** No separate
  "accept spectators" flag on the dedicated server, mirroring how
  PR #152 has no separate "accept rejoins" flag.

**Client connect path** still needs new work: today `JoinGame`
always picks an agency and joins as a player REPLICA; the spectator
path must skip agency/team selection and flip the `observer` bit on
the outgoing `MSG_CONNECT`.

**Open questions (not addressed by PR #152):**
- Auth: who can spectate? (anyone, logged-in only, friends-of-host,
  admin-only, …)
- Per-game spectator cap and where it's enforced.
- Visibility model: full unfiltered state, or team-relative fog of
  war?
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

## Status

Tracked in [`2026-05-09-spectating-progress.md`](2026-05-09-spectating-progress.md).
