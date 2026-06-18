# Headless screen reachability recipes

Concrete, verified control-op recipes for driving a headless Silencer to each
screen so it can be screenshotted for visual-parity verification. Every recipe
is grounded in the actual state machine (`StateName` in
`clients/silencer/src/game/loop/game_loop.cpp:544`), the control-op handler
(`clients/silencer/src/net/controldispatch.cpp`), the phase reconciler
(`clients/silencer/src/client/ui/app_shell/app_root.cpp:57` ←
`clients/silencer/src/game/ui/session_phase.h`), and the per-state tick bodies
(`clients/silencer/src/game/tick/tick_*.cpp`). The MAINMENU, SINGLEPLAYERGAME,
and tech-overlay paths below were smoke-tested live against the built binary in
this worktree.

---

## 0. Boilerplate (all recipes)

Source the harness — it auto-detects the binary, gives you `cli`, a free port,
and start/stop helpers (`tests/cli-agent/e2e/lib.sh`). **Must run under `bash`,
not `zsh`** (it reads `BASH_SOURCE`).

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")           # silencer --headless --control-port $PORT
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"                       # polls `ping` ~30s
```

Binary: `clients/silencer/build/Silencer.app/Contents/MacOS/Silencer` (macOS).
Screenshots are 640×480 RGBA PNGs (world + cppx UI composited) written by
`Game::CaptureCompositedFrame` (`game.cpp:25`). Take one with
`cli --port "$PORT" screenshot --out /tmp/foo.png` (POST_RENDER op; runs after
the frame is presented). Always insert `wait_frames --n 1` after a state/overlay
change before the screenshot so the new tree has rendered.

### State ↔ phase ↔ screen map

| GameState (`wait_for_state --state`) | SessionPhase | Screen component (`app_root.cpp`) |
|---|---|---|
| `MAINMENU` | MainMenu | `MainMenu` (`main_menu.cppx`) |
| `LOBBYCONNECT` | Connecting | `LobbyConnect` (`lobby_connect.cppx`) |
| `CREATECHARACTER` | CharacterCreate | `CharacterCreate` (`character_create.cppx`) |
| `LOBBY` | Lobby | `LobbyScreen` (`lobby_screen.cppx`) — incl. **Game Staging** + **Tech Select** sub-panels |
| `UPDATING` | Updating | `UpdateScreen` |
| `INGAME` / `TESTGAME` / `REPLAYGAME` | InMatch | `InGameScreen` (`in_game_screen.cppx`) |
| `SINGLEPLAYERGAME` | SinglePlayer | `InGameScreen` |
| `MISSIONSUMMARY` | PostMatch | `MissionSummary` (`mission_summary.cppx`) |
| `HOSTGAME` / `JOINGAME` | Loading | grey scaffold (no real screen yet) |
| `OPTIONS*` | (overlay over MainMenu) | options overlays |

Key architectural facts that shape the recipes:

- **Game Staging and Tech Select are NOT separate game states.** They render
  inside the `LOBBY` state. While connected to a game, `use_staging().active`
  is true and `LobbyScreen` swaps its right column to `StagingPanel`
  (`lobby_screen.cppx:268`). Tech Select (design doc §3.9) is a **net-new**
  sub-panel of that same staging cluster — it does not exist in code yet
  (`Choose Tech` button + tech list per the design doc), so it cannot be
  reached today.
- **The in-match buy/tech/chat/scoreboard "screens" are overlays**, driven by
  the `ingame_ui_mode` control op (`controldispatch.cpp:344`), not by
  `wait_for_state`. They require a populated world (a viewed player) first.
- **MISSIONSUMMARY requires a real multiplayer match** authenticated against a
  lobby: `tick_ingame.cpp:303` only routes to MISSIONSUMMARY when
  `world.lobby.state == AUTHENTICATED`. The Tutorial (SINGLEPLAYERGAME) and
  unauthenticated paths route to MAINMENU instead (`tick_singleplayer.cpp:401`,
  `tick_ingame.cpp:310`). There is no headless one-liner to it (see §5).

---

## 1. LOBBY (have it)

The full connect→auth→create-character→lobby path. Needs the Go lobby server
(`services/lobby/lobby`, present in this worktree) running locally with the
binary's baked version. The canonical, working recipe is
`tests/cli-agent/e2e/30_lobby_login.sh` — boot a lobby, start silencer with
`--lobby-host/--lobby-port`, then:

```bash
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label "Connect To Lobby"
cli --port "$PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
# type creds (Username autofocused; Tab → Password)
for ch in a l i c e; do cli --port "$PORT" key --key "$ch"; done
cli --port "$PORT" key --key tab
for ch in s e c r e t; do cli --port "$PORT" key --key "$ch"; done
# wait until the async connect flow reaches AUTHENTICATING, THEN click Login
# (state op exposes lobby_state)
cli --port "$PORT" click --label "Login/Create"
# fresh account → CREATECHARACTER 3-step wizard
cli --port "$PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000
cli --port "$PORT" click --label "Create New Character"
cli --port "$PORT" set_text --label "Alias" --text "Alice"     # or key a-l-i-c-e
cli --port "$PORT" click --label "Continue"
cli --port "$PORT" click --label "Noxis"                       # pick an agency
cli --port "$PORT" wait_for_state --state LOBBY --timeout-ms 15000
cli --port "$PORT" screenshot --out /tmp/lobby.png
```

The lobby boot/auth boilerplate (lobby flags, `wait_for_lobby_state`,
`wait_for_widget`) is verbatim in `30_lobby_login.sh` — copy that harness.
`lib.sh::create_initial_character` wraps the wizard once you're at
CREATECHARACTER.

---

## 2. GAME STAGING (lobby → create game → staging room)

Staging is the `LOBBY` state with `staging.active` true (right column =
`StagingPanel`: `Ready` / `Change Team` / `Leave`, controlIds `StagingReady`,
`ChangeTeam`, `LeaveGame`). Reached by creating a game from the lobby, which
asks the Go lobby to spawn a dedicated `silencer -s` server; the local player
auto-joins and the panel swaps. Canonical recipe:
`tests/cli-agent/e2e/40_lobby_basic.sh`. Continue from §1 at LOBBY:

```bash
# right column default = GameSelect (controlIds NewGame / JoinGame)
cli --port "$PORT" click --label "New Game"        # swap to GameCreate form
# form: CreateGame / CreateBack controls now present
cli --port "$PORT" set_text --label "Game Name" --text "MyGame"
cli --port "$PORT" click --label "Next Map"        # ensure a concrete bundled map
cli --port "$PORT" click --label "Create"          # spawns dedicated server, auto-joins
# poll up to ~20s for the staging panel to anchor:
#   StagingReady (or its sibling LeaveGame) → in staging
cli --port "$PORT" screenshot --out /tmp/staging.png
```

- **States:** stays in `LOBBY` throughout; do NOT `wait_for_state` for a new
  state. Detect staging by polling `inspect` for control id `StagingReady` or
  `LeaveGame` (use `wait_for_any_widget` from `40_lobby_basic.sh`).
- **Reliability caveat (real):** `40_lobby_basic.sh` documents that the
  dedicated-server spawn is *flaky in CI/headless* — it falls back to merely
  asserting the create form closed. For a guaranteed staging **screenshot**,
  the most reliable path is to add a second authenticated presence and a
  real spawned dedicated server: use `lobby spawn` to put a host online, have
  it `create_game`, then have the controlled silencer `JoinGame` and land in
  staging. If the spawn fails locally you will not get the staging panel —
  there is no pure-client shortcut, because `staging.active` is driven by the
  live game connection, not a UI toggle.

Game Staging golden: design doc §3.8 (`game_staging.png` — dual AgentCards;
current code is a single roster list, parity work pending).

---

## 3. TECH SELECT (net-new — not yet reachable)

Per design doc §3.9 (`tech_select.png`), Tech Select is the staging-room loadout
picker: a `Back To Teams` oval + `Tech slots left: N` header + a scrolling tech
list (`Laser [$3]`, `Rocket [$3]`, …). The design doc says *"current has NO
tech screen at all — build it."* Confirmed in code: `use_staging.h:26-28` notes
*"Tech loadout (slots/buyable/wanted + set/toggle) joins in SIL-21 (4/n)"* and
`StagingPanel` (`lobby_screen.cppx:185`) has only Ready/ChangeTeam/Leave — no
`Choose Tech` button, no tech list.

- **Recipe (once built):** reach Game Staging (§2), then
  `cli --port "$PORT" click --label "Choose Tech"` to swap the staging cluster
  to the Tech Select panel (a screen-local `use_state` panel swap, same idiom
  as GameSelect↔GameCreate in `lobby_screen.cppx`); `Back To Teams` reverses
  it. State stays `LOBBY`; detect via `inspect` for the `ChooseTech` /
  `BackToTeams` control ids.
- **Today:** unreachable. The screen recreation must add the `Choose Tech`
  button to `StagingPanel`, a net-new `TechSelectPanel` sub-component, and wire
  `use_staging` (or a new `use_loadout`) to the public World tech seam
  (`SendReady`/loadout slots). Until then there is nothing to screenshot.

> NB: do not confuse this with the **in-match** `Tech` station overlay (§4d),
> which is a different thing (`ingame_ui_mode --mode tech`) and *is* reachable.

---

## 4. IN-GAME (Tutorial → SINGLEPLAYERGAME, no lobby needed)

The fastest in-match screen: no lobby, no auth. **Verified live.**

```bash
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label Tutorial
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000
# wait for the world to populate before reading player-coupled state:
for _ in $(seq 1 60); do
  if cli --port "$PORT" world_state | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    process.exit((r.objects_count ?? 0) > 0 && (r.players?.length ?? 0) > 0 ? 0 : 1);'; then break; fi
  cli --port "$PORT" wait_frames --n 1
done
cli --port "$PORT" wait_frames --n 1
cli --port "$PORT" screenshot --out /tmp/ingame_hud.png     # bare HUD (design §3.14)
```

`Tutorial` loads `AGENCY04.SIL` and spawns a Noxis player
(`tick_singleplayer.cpp:14-48`). The bare HUD (`InGameScreen`) is now on screen.

### In-match overlays (drive with `ingame_ui_mode`, NOT wait_for_state)

After the world is populated, mutate the viewed agent / world to raise each
overlay, then screenshot. The op returns the overlay state for assertions
(`controldispatch.cpp:344`); modes: `clear`, `chat`, `buy`, `tech`,
`playerlist`, `status`, `all`. Canonical recipe:
`tests/cli-agent/e2e/51_ingame_ui_overlays.sh`.

```bash
# (a) scoreboard / player list (design §3.15) — F1 overlay
cli --port "$PORT" ingame_ui_mode --mode playerlist
cli --port "$PORT" wait_frames --n 1; cli --port "$PORT" screenshot --out /tmp/scoreboard.png

# (b) in-game chat (design §3.16)
cli --port "$PORT" ingame_ui_mode --mode chat
cli --port "$PORT" wait_frames --n 1; cli --port "$PORT" screenshot --out /tmp/ingame_chat.png

# (c) buy station overlay (design §3.17)
cli --port "$PORT" ingame_ui_mode --mode buy
cli --port "$PORT" wait_frames --n 1; cli --port "$PORT" screenshot --out /tmp/buy.png

# (d) tech station overlay (design §3.17; VERIFIED: tech_active=true, 2 items)
cli --port "$PORT" ingame_ui_mode --mode tech
cli --port "$PORT" wait_frames --n 1; cli --port "$PORT" screenshot --out /tmp/tech_overlay.png

cli --port "$PORT" ingame_ui_mode --mode clear   # tear the overlay back down
```

`down`/`up` keys move the buy/tech selection cursor (the focus drives the
replicated cursor; see `51_ingame_ui_overlays.sh:107`). `ingame_ui_mode --mode
status` reports state without changing it.

> The full multiplayer in-match HUD (`INGAME`) is identical to render but needs
> the lobby+staging path (§2) followed by all peers readying up; the dedicated
> server then transitions everyone to `INGAME`. For HUD/overlay screenshots,
> SINGLEPLAYERGAME is equivalent and far cheaper — prefer it.

---

## 5. MISSION SUMMARY (hard — needs a finished authenticated match)

`MissionSummary` (PostMatch) is only entered from `INGAME` when the match ends
**and** the client is lobby-authenticated:

```
tick_ingame.cpp:303  if(gameSession.CheckForEndOfGame()){
                       if(world.lobby.state == Lobby::AUTHENTICATED) GoToState(MISSIONSUMMARY);
                       else GoToState(MAINMENU);  }
```

So there is **no cheap headless recipe**. To reach it you must:

1. Run the full lobby path (§1) to authenticate.
2. Create + start a real game (§2) and get into `INGAME` (all peers ready →
   dedicated server starts the match).
3. Drive the match to `CheckForEndOfGame()` — i.e. satisfy the active
   `GameMode`'s win/score condition (`world.gameMode`, `modes/*.cpp`). There is
   no `end_match` control op; the end condition must be met in simulation.
4. `cli --port "$PORT" wait_for_state --state MISSIONSUMMARY --timeout-ms …`
   then `screenshot`.

The Tutorial path is a dead end for this screen: `tick_singleplayer.cpp:401`
and the unauthenticated `tick_ingame` branch both go to MAINMENU, never
MISSIONSUMMARY.

**Recommended approach for screenshot verification:** since there is no golden
for Mission Summary (design doc §3.18 is flagged "no golden — needs-golden")
and reaching it requires a fully-played authenticated match, the practical path
is one of:

- **(preferred) A temporary debug seam:** add a one-shot control op (or a
  `GoToState(MISSIONSUMMARY)` reachable after a Tutorial match) purely for
  capture, behind the control socket, mirroring how `ingame_ui_mode` exists as
  test/automation plumbing. This is the lowest-effort reliable capture and can
  be removed after blessing.
- **(heaviest) A capstone-style e2e** that provisions a lobby + dedicated
  server (extend `40_lobby_basic.sh`), readies up, and forces the game mode's
  end condition. This is the only "real request through the full stack" route
  but is the most fragile (depends on dedicated-server spawn, which the e2e
  itself documents as flaky headless).

Flag MISSIONSUMMARY as needs-golden + needs-capture-seam before attempting
parity work on it.

---

## Quick reference: which screens are capturable today

| Screen | Reachable headless now? | Recipe |
|---|---|---|
| Main menu / Options | Yes (trivial) | `wait_for_state MAINMENU`; click into Options |
| Lobby Connect | Yes | §1 (click `Connect To Lobby`) |
| Character Create (3 steps) | Yes (needs lobby) | §1 |
| Lobby | Yes (needs lobby) | §1 |
| **Game Staging** | Yes, but flaky (needs lobby + dedicated spawn) | §2 |
| **Tech Select** | **No — net-new, not built** | §3 |
| **In-game HUD + buy/tech/chat/scoreboard** | **Yes (Tutorial, no lobby)** | §4 |
| **Mission Summary** | **No cheap path (needs finished authed match)** | §5 |
