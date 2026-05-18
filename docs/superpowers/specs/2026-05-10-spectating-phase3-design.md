# Spectating — Phase 3 design (joining as spectator)

**Status:** design complete; tracker is
[`docs/plans/2026-05-09-spectating-progress.md`](../../plans/2026-05-09-spectating-progress.md).

**Goal:** let any logged-in lobby user click **Spectate** on a running,
spectatable game and start receiving full world snapshots. Mid-match
join is first-class. This is Phase 3 of the umbrella spectating
feature on branch `hv/spectator` / PR
[#148](https://github.com/Arsia-Mons/Silencer/pull/148).

This spec covers connect, slot accounting, snapshot delivery, chat,
disconnect, and match-end. **Spectator controls (follow-cam, cycle,
free-cam) are Phase 4** and are explicitly out of scope here; Phase 3
lands with a placeholder free-cam at map center.

---

## Decisions

| # | Question | Decision |
|---|---|---|
| 1 | Auth | Any logged-in lobby user. Same trust level as a player Join. |
| 2 | Per-game spectator cap | None. First-come from the shared 25-slot peer pool. Matches PR #152's rejoin model. |
| 3 | Snapshot visibility | Full state. The engine already sends full world state to every peer; visibility is renderer-side. |
| 4 | Player-side visibility | Hidden from scoreboard/peer-list. Spectators do appear in chat when they speak (revised — see Q5). |
| 5 | Chat | Spectators send and receive normal chat; no special affordance. Server coerces any `to=1` (team) chat from an observer into `to=0` (all). |
| 6 | Mid-match-join side effects | None. Silent join; no pause, no announcement. |
| 7 | Match-end | Same path as players. Watch the 3-second victory screen, return to lobby on teardown. No stats credit. |
| Wire | `MSG_CONNECT` observer bit | Always-present trailing 1-bit field. Same-version requirement is already enforced by `SILENCER_VERSION`. |

---

## Section 1 — Architecture overview

Phase 3 is a thin observer overlay on the rejoin scaffolding PR #152
introduced. The shape:

- One new boolean on `Peer` (`observer`), mirroring PR #152's
  `disconnected`.
- One new bit on the `MSG_CONNECT` wire payload.
- A third branch in the AUTHORITY `MSG_CONNECT` accept block (rejoin,
  observer, new player).
- One new `Game::SpectateGame(LobbyGame &)` client entry point that
  parallels `JoinGame` but skips agency/team selection and flips the
  observer bit on the outbound connect.
- Replace the Phase 2 stub at
  `clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp:348`
  (`ctx.ShowMessage("Spectating coming soon")`) with the real call.

`SendSnapshots`, `SendGameInfo`, `SendPeerList`, and match-end all
keep their existing shape. Only the chat broadcaster and the input
queueing path grow observer-aware guards.

### Where things change

| Layer | File(s) | Change |
|---|---|---|
| Wire format | `world.cpp` (MSG_CONNECT serialize and parse) | Append a trailing 1-bit `observer` field. Always present in the new version. |
| Peer struct | `world.h` / `world.cpp` | New `bool observer = false`. Serialized in `Peer::Serialize` so peers know which peer is an observer (used to hide from scoreboard / route chat). |
| AUTHORITY accept block | `world.cpp:277` (`case MSG_CONNECT`) | Third branch: `observer && gameinfo.spectatable` → admit. Password gate applies. `peercount >= maxplayers` no longer rejects when `observer` is set (player-cap doesn't apply to observers; the 25-slot pool ceiling still does via `AddPeer` returning null). |
| Snapshot send | `world.cpp:1855` (`SendSnapshots`) | No change. Observers receive the same per-peer delta everyone else does. |
| Chat receive | `world.cpp:464` (`case MSG_CHAT`) | If sender `peer->observer`, coerce `to=1` to `to=0` before broadcast. Otherwise unchanged. |
| Input receive | `world.cpp:386` (`case MSG_INPUT`) | Add `&& !peer->observer` to the gate. Observers never enter the input queue. |
| Disconnect | `world.cpp` (`HandleDisconnect`) | If `peer->observer`, never park; free the slot immediately. |
| Match end | `clients/silencer/src/game/ingame.cpp:59` (`CheckForEndOfGame`) | No code change required. Observer accountids are not added to `ingameusers` on connect, so they're naturally skipped in stats registration. |
| Client connect | `clients/silencer/src/game/game.cpp` (`JoinGame` neighborhood) | Add `Game::SpectateGame(LobbyGame &)` mirroring `JoinGame`'s setup. Skips agency picker and team selection; calls a `Connect` variant with `observer = true`. |
| Outbound MSG_CONNECT | `world.cpp:1675` (`Connect` neighborhood) | Take an `observer` bool; append it as the trailing bit after the password bytes. |
| Game-select panel | `clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp:348` | Replace `ctx.ShowMessage("Spectating coming soon")` with `ctx.game.SpectateGame(*lobbygame)`. |

---

## Section 2 — Data flow

### Connect

```
Lobby UI                Client                  Dedicated Server (AUTHORITY)
   |                       |                            |
   | click Spectate        |                            |
   |---------------------->|                            |
   |                       | SpectateGame(LobbyGame)    |
   |                       | SwitchToMode(REPLICA)      |
   |                       | state = CONNECTING         |
   |                       | MSG_CONNECT {agency=0,     |
   |                       |   accountid, pw, observer=1}|
   |                       |--------------------------->|
   |                       |                            | INGAME accept block:
   |                       |                            |   1. password check
   |                       |                            |   2. rejoinpeer? no
   |                       |                            |   3. observer && spectatable? yes
   |                       |                            |   AddPeer() with observer=true
   |                       |   MSG_CONNECT {accept=1,   |
   |                       |              peerid}       |
   |                       |<---------------------------|
   |                       |   MSG_GAMEINFO             |
   |                       |<---------------------------|
   |                       |   MSG_PEERLIST             |
   |                       |<---------------------------|
   |                       |   MSG_SNAPSHOT (full)      |
   |                       |<----- repeating -----------|
```

No map download. No `MSG_READY`. Resync is `SendGameInfo` +
`SendPeerList`, identical to PR #152's rejoin handshake. Client
transitions REPLICA `CONNECTING → INGAME` as soon as the first
snapshot arrives — same as a rejoiner.

### Slot accounting

- The shared peer pool ceiling is `world.h:219`, `maxpeers = 25`.
- `AddPeer` returning null on a full pool is the hard ceiling; the
  observer admit branch surfaces that as `accept=0`.
- Today's `dedicatedserver.active && peercount >= gameinfo.maxplayers`
  reject is a player-cap, not a slot-cap. The observer branch bypasses
  it. The rejoin branch already bypasses it.

### Chat

- Observers receive all-chat (`to=0`) and players' team-chat is
  naturally routed only to teammates' peer ids, which never include
  an observer. No change needed for the receive path.
- Observers send: on receive, if `peer->observer`, the server treats
  `to=1` as `to=0` and rebroadcasts via the existing `to=0` loop.
  Otherwise the broadcast path is unchanged.

### Disconnect / leave

- Spectator `MSG_DISCONNECT` → `HandleDisconnect`. New branch: if
  `peer->observer`, delete the peer immediately (no parking, no
  `disconnected=true`).

### Match end

- `CheckForEndOfGame` (`ingame.cpp:59`) loops `ingameusers` to
  register stats. Observers are not added to `ingameusers` on
  connect, so no special skip is needed. Confirmed by reading the
  player-path code.
- Observers stay connected through the 3-second victory message and
  then return to the lobby alongside everyone else when the dedicated
  server tears down.

---

## Section 3 — Error handling & edge cases

| Case | Behavior |
|---|---|
| Spectatable=false on the running game | AUTHORITY rejects `observer=1` with `accept=0`. The Phase 2 button gating already prevents this in normal flow; this rejection is defense-in-depth against stale `LobbyGame` rows. |
| 25-slot pool full | `AddPeer` returns null → `accept=0`. Client surfaces the existing "could not join" path. |
| Password mismatch on a private game | Same as players. `canjoin = false` → `accept=0`. |
| Spectator UDP goes silent | Standard peer-timeout path frees the slot. Q7 confirmed no parking. |
| Spectator sends `MSG_INPUT` | `MSG_INPUT` case is gated on `!peer->observer`. Inputs from observers never enter the queue. |
| Spectator connects mid-tick / mid-state-transition | Same path as a player connecting at that moment. No special branch. |
| Observer with same accountid as a parked rejoiner reconnects | Rejoin wins — `rejoinpeer` is checked before the observer branch. An observer with no parked slot falls through to the observer admit. |
| Spectatable game becomes non-spectatable mid-match | Cannot happen; Phase 1 locked the flag at creation. No code path required. |

---

## Section 4 — Testing

- **Manual smoke** (matches Phase 2's deep-smoke shape; runnable by
  hand on Windows, expected primary verification path):
  1. Two clients. One hosts a spectatable game; the other joins as
     player. Game transitions INLOBBY → INGAME.
  2. Third client clicks **Spectate** on the panel.
  3. Verify: spectator receives snapshots and renders the world.
     Scoreboard / peer list does not include the spectator. Chat
     from the spectator appears to the player as a normal chat line.
  4. Spectator disconnects. Verify dedicated server's peer count
     drops (slot freed immediately, not parked).
  5. Match ends. Verify the spectator and player both return to
     lobby together after the 3-second victory message.
- **Non-spectatable rejection:** repeat (1)–(2) with a
  non-spectatable game. The Spectate button is hidden by Phase 2;
  a hand-crafted observer connect attempt must be server-rejected
  with `accept=0`.
- **Wire-format unit:** if an existing test exercises `MSG_CONNECT`
  serialization, extend it; if none, the wire change is covered
  implicitly by the smoke flow.
- **CLI-agent E2E remains deferred.** Same constraint as Phase 2:
  the `tests/cli-agent` harness has no op for driving game
  creation, so the host → join → spectate → disconnect → end-match
  flow cannot be automated yet. Adding a game-creation CLI op is
  queued separately and would unblock both Phase 2's deep smoke and
  Phase 3's E2E.

---

## Out of scope

- **Phase 4 spectator controls.** Follow-cam, Tab/Shift-Tab cycle,
  free-cam, name-overlay, HUD policy. Phase 3 lands the connection
  and snapshot pipeline; rendering for spectators starts at free-cam
  at map center.
- **Reusing the dedicated server across matches** (Q7 option "stay
  parked for next match"). The lobby spawns a fresh dedicated server
  per game; this is unchanged.
- **Per-game spectator cap or host-configurable cap** (Q2 sub-cap
  option). Skipped; first-come from the 25-slot pool.
- **Aggregate spectator count or named list in the lobby/scoreboard**
  (Q4 alternates). Skipped; observers are hidden from scoreboard.
