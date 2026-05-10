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
- [ ] **Phase 4 — Spectator controls.** Designed, not implemented.
  Design at [2026-05-10-spectating-phase4.md](2026-05-10-spectating-phase4.md).
  Scope: ESC quit fix (Phase 3 carryover) + Tab/Shift-Tab cycle +
  WASD free-cam + hold-jump name overlay + minimap-click follow
  (optional). Followed player's HUD renders as-is; no
  world-interacting inputs; realtime only.

## Phase 3 commits

On branch `hv/spectator` between `92f4aa7` (pre-Phase-3) and `c77622c` (current HEAD):

| SHA | Message |
|---|---|
| `5593b54` | `feat(spectating): add Peer::observer flag and serialize on peer-list` |
| `c898801` | `feat(spectating): add observer bit to MSG_CONNECT wire payload` |
| `b0ea6f0` | `feat(spectating): admit observers in AUTHORITY MSG_CONNECT handler` |
| `988d18b` | `feat(spectating): drop observer inputs and route observer chat to all` |
| `72a075a` | `feat(spectating): free observer slot immediately on disconnect` |
| `f54ad82` | `feat(spectating): wire Spectate button to SpectateGame entry` |
| `fd837e6` | `docs(spectating): phase 3 done — joining as spectator` |
| `c77622c` | `fix(spectating): skip team assignment for observers` (integration bug caught by final review — `AddPeer` was calling `FindTeamForPeer` for observers, which both polluted teams and could fail admit when `gameinfo.maxteams` was reached) |

## Phase 3 partial-smoke result (2026-05-10)

Manual smoke against a local lobby (`:15170`) with two clients confirmed:
- Spectator click on the lobby panel calls `Game::SpectateGame`.
- Client connects to dedicated server, receives `MSG_GAMEINFO` /
  `MSG_PEERLIST`, transitions into the in-game render path.
- World renders (free-cam at map center, as designed).

**Observed gap:** ESC does not advance the quitstate machine for
observers, so spectators are trapped until match-end teardown. Root
cause is in `clients/silencer/src/game/events.cpp:362–373` —
`OnScancodeDown` gates the quitstate advance on
`Player * localplayer = world.GetPeerPlayer(world.localpeerid)`
being non-null, which is never true for an observer. The
`chatinterfaceid`/`buyinterfaceid` checks in that block are
irrelevant for observers (no buy/chat modal). This was identified
as a Phase 3 leak that Phase 4 should fix as part of its broader
controls scope.

Three-client smoke (host + player + spectator, full
chat-coercion / disconnect-frees-slot / match-end-teardown
verification) is still pending; deferred until Phase 4 lands the
controls that let a spectator interact and leave gracefully.

## Handoff prompt

> Branch `hv/spectator` (PR #148) is the umbrella for the entire
> spectating feature; PR merges when all four phases are in. Phases
> 1–3 are done on branch; Phase 4 (spectator controls) is the final
> piece.
>
> **Phase 3 result.** Spectator connect/admit/snapshot pipeline
> works end-to-end. The wire format gained a trailing 1-bit
> `observer` field on `MSG_CONNECT`; `Peer::observer` is serialized
> on the peer-list; the AUTHORITY-side `MSG_CONNECT` handler has a
> third admit branch (rejoin → observer → new player) at
> `clients/silencer/src/world/world.cpp` around line 356; observer
> inputs are dropped at the network layer; observer team-chat is
> coerced to all-chat; observers free their slot immediately on
> disconnect (no parking); `Game::SpectateGame` mirrors `JoinGame`
> but passes `observer=true` to `World::Connect`. Manual smoke
> confirmed the spectator joins and the world renders.
>
> **Next up — Phase 4 (spectator controls).** No design doc exists
> yet; brainstorm first. Out-of-scope items the Phase 3 spec
> explicitly named that are now in scope for Phase 4:
>
> - Follow-cam (track a chosen player).
> - Cycle through players (Tab / Shift-Tab).
> - Free-cam (manual camera pan; current behavior is free-cam
>   pinned at map center).
> - Name overlay over each player from the spectator's POV.
> - HUD policy for spectators (which HUD elements show, hide).
>
> **Phase 3 carryover to address in Phase 4.**
>
> - **ESC trap.** `clients/silencer/src/game/events.cpp:362–373`
>   gates the quitstate advance on a non-null `localplayer`.
>   Observers have no Player, so ESC does nothing — they cannot
>   leave the match gracefully. Allow the quitstate to advance when
>   the local peer has `observer=true`. The `chatinterfaceid` /
>   `buyinterfaceid` checks are irrelevant for observers (no buy /
>   chat modal exists on the observer side).
> - **Three-client smoke.** Still pending. Once Phase 4's controls
>   let the spectator interact and exit, run the full Section 4
>   smoke from the Phase 3 design spec
>   (`docs/superpowers/specs/2026-05-10-spectating-phase3-design.md`):
>   chat coercion (observer `to=1` → all sees it as all-chat),
>   slot-frees-immediately on observer disconnect, match-end
>   teardown returns spectator to lobby with the players.
>
> **Reference docs.**
>
> - Phase 3 design spec:
>   `docs/superpowers/specs/2026-05-10-spectating-phase3-design.md`.
> - Phase 3 implementation plan:
>   `docs/superpowers/plans/2026-05-10-spectating-phase3.md`.
> - Original spectating design (pre-phase-split):
>   `docs/plans/2026-05-09-spectating.md`.
>
> **Local smoke setup that worked for Phase 3.**
>
> 1. Reconfigure the client for a local lobby and build:
>    ```
>    cmake -B clients/silencer/build-unity -S clients/silencer \
>          -DSILENCER_LOBBY_HOST=127.0.0.1 -DSILENCER_LOBBY_PORT=15170
>    cmake --build clients/silencer/build-unity
>    ```
> 2. Build the lobby: `cd services/lobby && go build`
> 3. Run the lobby with an isolated DB so it doesn't trample local
>    dev state:
>    ```
>    ./silencer-lobby -addr :15170 \
>      -game-binary <path-to-built-Silencer-binary> \
>      -db lobby-smoketest.json
>    ```
> 4. **Gotcha.** A saved client config from a prior dev session
>    pins its own `lobbyport` / `lobbyhost` and overrides the new
>    CMake defaults — `Config::LoadDefaults` only fires when no
>    saved file exists. On macOS the file is
>    `~/Library/Application Support/Silencer/config.cfg`. If a
>    freshly-built local client says "couldn't connect," patch
>    `lobbyhost` / `lobbyport` in that file to match the local
>    lobby (e.g. `lobbyhost = 127.0.0.1`, `lobbyport = 15170`)
>    before relaunching. (We hit this on 2026-05-10.)
>
> **CLI-agent E2E remains deferred.** The `tests/cli-agent` harness
> still has no op for driving game creation. Adding one would
> unblock automated end-to-end coverage of the entire spectating
> feature; it's queued separately.
>
> **Build tip.** Fastest client build on Windows:
> `cmake --preset win-ninja-unity -S clients/silencer` (~15 s).
> `build-unity/Silencer.exe` is what `lib.sh` picks up.
