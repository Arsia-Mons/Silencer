# clients/silencer/src/game — Game coordinator

Owns the `Game` class and everything that drives its lifecycle: the main
loop, SDL input, rendering pipeline, session management, game modes, actor
state, and replay. Nothing in here should know about world simulation
internals — use `World` and its subsystems for that.

## Subdirectory map

| Dir | What it owns |
|---|---|
| `loop/` | `Game::Loop()` — the per-frame pump: polls SDL events, dispatches input, ticks world, renders UI, drives screen transitions |
| `init/` | `Game::Init()` — SDL setup, window creation, GAS load, SDL hints (including `SDL_HINT_ENABLE_SCREEN_KEYBOARD`) |
| `tick/` | Per-state tick handlers split by `GameState`: `tick_ingame`, `tick_hostjoin`, `tick_singleplayer`, `tick_replay`, `tick_misc` |
| `session/` | `GameSession` — load/unload map, join/leave/spectate game, ambience mixer, map downloader |
| `modes/` | `GameMode` subclasses — one file per game mode (Deathmatch, DataRetrieval, Extraction, etc.) |
| `state/` | `GameMode` base + `GameStateObject`, `Team`, `User`, `Stats` — shared multiplayer state |
| `actor/` | `Team` actor helpers, stats |
| `input/` | `GameInput` — raw SDL event → keybind action mapping |
| `render/` | `GameRenderer` — SDL3 GPU backend, surface resize, vsync, `kLegacyRenderWidth/Height` |
| `ui/` | `GameUiPipeline` — retained client UI frame lifecycle, client UI dispatch, `SDL_StartTextInput` gating |
| `replay/` | Replay recorder/playback |

## Key boundaries

- `Game` coordinates; `World` simulates. `Game::Loop` calls `world.Tick()`
  and `world.DoNetwork()` but does not reach into world subsystems directly.
- `GameSession` is the only place that calls `world.map.Load*` and
  `world.UnloadGame()`.
- `GameUiPipeline` owns `SDL_StartTextInput` / `SDL_StopTextInput`; never call
  SDL text input functions elsewhere.
- `GameRenderer` owns `kLegacyRenderWidth` / `kLegacyRenderHeight` (640/480)
  as `inline constexpr` in `render/game_renderer.h`. Import from there;
  do not redefine locally.

## Game modes

Each mode subclasses `GameMode` and overrides `Tick`, `OnKill`, `OnCapture`,
etc. The active mode is owned by `World` (`world.gameMode`). To add a mode:
1. Add a `GameModeId` enum value in `state/gamemode.h`.
2. Create `modes/your_mode.h` + implement in `modes/your_mode.cpp` if needed.
3. Wire into `World::CreateGameMode()`.

## Build / run

```bash
bash clients/silencer/build.sh     # macOS/Linux
clients/silencer/build.ps1         # Windows
```
