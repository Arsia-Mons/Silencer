# Spectating — Progress

Tracker for the multi-phase spectating feature. Design:
[2026-05-09-spectating.md](2026-05-09-spectating.md).

All four phases land in a single umbrella branch / PR:
`hv/spectator` /
[PR #148](https://github.com/Arsia-Mons/Silencer/pull/148). The PR
merges when the whole feature is in — phases below check off as
they're complete on branch.

## Phases

- [x] **Phase 1 — Game-creation: spectatable flag.** Done on branch.
  Toggle wired through wire format, create-game UI, and config
  persistence.
- [x] **Phase 2 — Server browser: spectatable affordance.** Done on
  branch. Dedicated server appends parked-peer accountids to its
  UDP heartbeat; lobby derives a per-recipient `can_rejoin` bit on
  every `opNewGame` push; game-select panel renders dual Join /
  Spectate buttons contextually. Spectate click is stubbed
  ("Spectating coming soon") pending Phase 3.
- [x] **Phase 3 — Joining as spectator (any time).** Done on branch.
  `MSG_CONNECT` carries a trailing `observer` bit; `Peer::observer`
  mirrors `Peer::disconnected` and is serialized on the peer-list.
  AUTHORITY-side admit gets a third branch (rejoin → observer → new
  player) that ignores the player-cap but respects the 25-slot pool
  and `gameinfo.spectatable` defense-in-depth. Observer inputs are
  dropped at the network layer; observer team-chat is coerced to
  all-chat; observers free their slot immediately on disconnect (no
  parking). Lobby panel `GSEL_BTN_SPECTATE` now calls
  `Game::SpectateGame`, which mirrors `JoinGame` but passes
  `observer=true` to `World::Connect`. Design spec at
  [docs/superpowers/specs/2026-05-10-spectating-phase3-design.md](../superpowers/specs/2026-05-10-spectating-phase3-design.md);
  plan at
  [docs/superpowers/plans/2026-05-10-spectating-phase3.md](../superpowers/plans/2026-05-10-spectating-phase3.md).
- [ ] **Phase 4 — Spectator controls.** Not started.

## Handoff prompt

> Branch `hv/spectator` is the umbrella for the entire spectating
> feature; PR #148 lands when all four phases are in. Phases 1 and 2
> are done on branch:
>
> - **Phase 1** — spectatable flag at game creation (checkbox, wire
>   format, config persistence).
> - **Phase 2** — server-browser affordance. Dedicated server
>   appends parked-peer accountids to its UDP heartbeat
>   (`clients/silencer/src/net/dedicatedserver.cpp`); the Go lobby
>   stores them on `LobbyGame.ParkedAccountIDs` and derives a
>   per-recipient `can_rejoin` byte appended to every `opNewGame`
>   push (`services/lobby/client.go::sendNewGame`,
>   `services/lobby/hub.go::OnHeartbeat`); the C++ client reads it
>   into `LobbyGame::canrejoin` and the game-select panel
>   (`clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp`)
>   renders Join / Spectate buttons contextually. Wire change is
>   mirrored in `shared/lobby-protocol/{protocol.md,vectors.json}`
>   with all three SDK test suites green. **Spectate click currently
>   stubs to `ctx.ShowMessage("Spectating coming soon")` in
>   `game_select_panel.cpp` — replacing that stub with the real
>   spectator-connect call is the entry point for Phase 3.**
>
> **Next up — Phase 3 (joining as spectator):** the design doc has
> seven open questions that need user resolution before code
> (auth, per-game spectator cap, visibility model, whether
> spectators are visible to players, chat/voice, mid-match-join
> side effects, match-end behavior). Walk through those first.
> Once locked, the engineering shape is already scoped against
> [PR #152](https://github.com/Arsia-Mons/Silencer/pull/152): reuse
> `MSG_CONNECT` with an `observer` bit, add a `Peer::observer` flag
> mirroring `Peer::disconnected`, third branch in the INGAME
> `MSG_CONNECT` accept block, observer-aware `SendSnapshots`,
> shared `maxpeers` pool, `SendGameInfo` + `SendPeerList` resync.
>
> **Smoke harness limitation:** the existing `tests/cli-agent`
> E2E suite still has no op for driving game creation
> (`GameCreatePanel`'s map selectbox crashes `cli select`, per PR
> #152's test plan). Phase 2's deep smoke (host → join → disconnect
> → rejoin → can_rejoin row → click Join → rebind) is therefore
> not yet automated; the wire-format change is covered by the
> Go/TS/C++ SDK golden-vector tests. Adding a CLI op for game
> creation is queued separately and would unblock both Phase 2's
> deep smoke and Phase 3's E2E coverage.
>
> **Build tips on Windows** (Phase 2 surfaced these):
> - Fastest game build: `cmake --preset win-ninja-unity -S
>   clients/silencer` (~15 s). `lib.sh` now finds the resulting
>   `build-unity/Silencer.exe` automatically.
> - C++ SDK on MSVC: `cd clients/lobby-sdk/cpp && cmake -B build -S
>   . -G Ninja && cmake --build build && build/codec_test.exe`.
>   `client.cpp` is POSIX-only and excluded from the static lib on
>   `WIN32`; codec/test path builds clean.
