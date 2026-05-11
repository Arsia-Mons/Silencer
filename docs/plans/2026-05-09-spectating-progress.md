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
- [x] **Phase 4 — Spectator controls.** Implemented on branch
  (compile-verified; three-client manual smoke pending).
  Design at [2026-05-10-spectating-phase4.md](2026-05-10-spectating-phase4.md).
  Scope landed: ESC quit fix (Phase 3 carryover), Move Left/Right
  cycle prev/next player, hold Activate to reveal all player names,
  followed player's HUD rendered as-is. Free-cam state and renderer
  branch are preserved but no input is bound (movement keys
  moonlight as cycle prev/next instead). Minimap-click follow
  dropped from scope. No world-interacting inputs; realtime only.

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

## Phase 4 commits

On branch `hv/spectator` after `c77622c`:

| SHA | Message |
|---|---|
| `314f1e2` | `docs(spectating): phase 4 design — spectator controls` |
| `8b2e321` | `fix(spectating): allow observers to ESC out of match` (Phase 3 carryover) |
| `90ec196` | `feat(spectating): add viewedpeerid for camera/HUD focus` |
| `c023ecd` | `feat(spectating): implement spectator controls` |
| `e07b30d` | `fix(spectating): drop redundant Smooth() in spectator free-cam branch` |

Compile-verified on macOS via
`cmake -B clients/silencer/build -S clients/silencer && cmake --build clients/silencer/build`.

## Handoff prompt

> Branch `hv/spectator` (PR #148) is the umbrella for the entire
> spectating feature; PR merges when all four phases are in.
> All four phases are now implemented on branch. Remaining work is
> the **three-client manual smoke** below, then merge.
>
> **Phase 4 result.** Spectator controls implemented per
> [`2026-05-10-spectating-phase4.md`](2026-05-10-spectating-phase4.md):
>
> - `World::viewedpeerid` is the new camera/HUD focus peer, mirroring
>   `localpeerid` for normal players and overridden each tick by the
>   spectator-controls block for observers. Never serialized; network
>   identity stays on `localpeerid`. `World::spectator` holds free-cam
>   state (`freecam`, `camx/y`, `camvx/y`, `holdshowallnames`,
>   `initialized`).
> - Spectator-controls block in `clients/silencer/src/game/tick/tick_ingame.cpp`
>   (after the replay-controls block) gated on `world.IsLocalObserver()`:
>   default-mode follow picks first living non-AUTHORITY non-observer
>   peer; Tab/Shift-Tab cycle (edge-triggered); WASD/arrows free-cam
>   (snaps back on next Tab); hold-jump toggles `holdshowallnames`;
>   auto-recovery re-picks a peer if the followed one disconnects.
> - Renderer (`clients/silencer/src/render/renderer.cpp`):
>   `localplayer = world.GetPeerPlayer(world.viewedpeerid)` at the top
>   of the camera pass; new free-cam branch reads `spectator.camx/y`;
>   `DrawHUD` keys off `peerlist[viewedpeerid]` so the followed
>   player's HUD renders as-is; secret-beam team-highlight uses
>   `viewedpeerid`; name overlay (line 833) accepts the
>   `IsLocalObserver() && spectator.holdshowallnames` path.
> - ESC trap fix (Phase 3 carryover) at
>   `clients/silencer/src/game/events.cpp:362-378`: observer peers can
>   advance the quitstate without a `Player`. Chat/buy modal checks
>   remain for players.
>
> **Out of scope, intentionally not added**: spectator-only HUD,
> ping/mark/door inputs, live rewind/scrub/pause, in-game chat
> opening for observers (Phase 3 only coerces *sent* observer chat
> to all-chat; opening the chat interface for observers is a
> separate ask), spectator count, "X is spectating" indicator to
> players, per-game spectator cap, minimap-click follow (dropped —
> Tab cycle is sufficient).
>
> **Remaining work: three-client manual smoke.**
>
> Set up via the local-smoke recipe below. Verify (closes Phase 3
> carryover + validates Phase 4):
>
> 1. Host creates a game with **Spectatable** on; player joins
>    normally; spectator joins via the **Spectate** button.
> 2. Spectator view follows a living player by default (no Tab
>    needed).
> 3. `Tab` cycles to next player; `Shift+Tab` cycles previous.
> 4. `WASD` enters free-cam; `Tab` snaps back to follow.
> 5. Hold jump → all player names visible; release → hidden.
> 6. Followed player's HUD (health/fuel/shield/files) renders as-is.
> 7. `ESC` → confirm returns spectator to lobby; host and player
>    continue.
> 8. Spectator types a message — host and player see it as all-chat
>    regardless of `to` (Phase 3 chat coercion).
>    *Caveat: opening the chat interface for observers is not wired
>    in Phase 3/4 — if this step is blocked, mark it deferred and
>    move on; it is not a Phase 4 regression.*
> 9. Spectator disconnects abruptly → AUTHORITY frees the slot
>    immediately (no parking).
> 10. Match ends naturally → spectator returns to lobby alongside
>     players.
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
