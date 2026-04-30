---
name: using-silencer-cli
description: Use when you need to drive the Silencer game from a terminal — clicking menus, taking screenshots, reading game state, validating game-data JSON, rebinding controls, or filling a lobby with fake authenticated players for multiplayer dev — without a human at the keyboard.
---

# using-silencer-cli

`silencer-cli` is the script you reach for when you can't sit at the
keyboard. It does three jobs:

1. **Remote-control a running game** — click buttons, type into
   text fields, take screenshots, read live world state, pause/step
   single-player, change keybinds. Useful for verifying UI changes,
   reproducing bugs, and writing end-to-end tests without a human
   driving the mouse.
2. **Validate game-data JSON** (`gas validate`) — runs the same
   schema checks the C++ loader applies, but as a fast local command
   so you can fix `shared/assets/gas/*.json` in a tight edit loop.
3. **Run fake players in a lobby** (`lobby …`) — keeps N
   authenticated lobby connections alive in the background so you
   can chat, create games, and appear in presence lists from the CLI.
   Saves you from launching real game clients to test multiplayer.

Each invocation runs one command and exits. Output is JSON on stdout
(exit 0), `[CODE] message` on stderr (exit 1 for command errors,
exit 2 for transport failures).

```
bun clients/cli/index.ts [--port <PORT>] <command> [--key value ...]
```

`--port` only matters for game-control commands — they connect to a
running game over TCP. The lobby commands ignore it (they go through a
separate background process). The `gas validate` command ignores it too
(it's pure local file I/O).

## Driving a running game

The "game" here is the normal Silencer binary launched with
`--headless --control-port <PORT>`. It listens on that TCP port and
accepts one JSON command per line. Headless means no window — useful
in CI; for local dev you can drop `--headless` and watch the game
react.

The test scripts handle binary-path detection across macOS/Linux/Windows
and pick a free port for you. Source `lib.sh` and you're set:

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label OPTIONS
cli --port "$PORT" screenshot --out /tmp/options.png
```

`lib.sh` defines a `cli` shell function (`bun .../clients/cli/index.ts`)
so paths with spaces don't word-split. Pass `--port "$PORT"` on every
invocation.

### Game-control commands

All commands take flags as `--key value`. A few accept a positional
shorthand for the most common arg (`click LABEL`,
`set_text LABEL TEXT`, `select LABEL N_OR_TEXT`).

| Command | Flags | Returns |
|---------|-------|---------|
| `ping` | — | `{version, build, frame, paused}` |
| `state` | — | `{state, current_interface_id, frame, paused}` — the game's current screen + frame number |
| `inspect` | `[--interface-id N]` | `{widgets:[{id,x,y,kind,label,w,h,enabled,...}], interface_id}` — every widget on the current screen, so you know what `click` / `set_text` can target |
| `world_state` | — | `{map, peers, players:[{id,hp,x,y}], objects_count}` — only meaningful in-game |
| `click` | `--label X` or `--id N` | `{widget_id}`. Matches a button or toggle; toggles flip their `selected` state. |
| `set_text` | `--label X --text Y` | `{}` — clears a textbox then types into it |
| `select` | `--label X --index N` or `--text Y` | `{}` — chooses an entry in a selectbox |
| `back` | — | `{went_back: bool}` — same as pressing Escape |
| `screenshot` | `[--out PATH]` | `{path}`. With no `--out`, the game writes to `$TEMP/silencer-<frame>.png` (or `/tmp/...` on Unix). |
| `pause` / `resume` | — | `{}` — single-player only; errors `WRONG_STATE` in live multiplayer |
| `step` | `--frames N` *or* `--ms N` | `{}` — advances the simulation, then re-pauses. One of the two flags is required. |
| `wait_frames` | `--n N` | `{}` — replies after N rendered frames |
| `wait_ms` | `--n N` | `{}` — replies after N wallclock ms |
| `wait_for_state` | `--state X [--timeout-ms 5000]` | `{}` or `TIMEOUT`. **Timeout is in milliseconds.** |
| `quit` | — | `{}` — asks the game to shut down cleanly |

#### Screen names

`state` and `wait_for_state --state` use these names (defined by
`Game::StateName`):

`NONE`, `FADEOUT`, `MAINMENU`, `LOBBYCONNECT`, `LOBBY`, `UPDATING`,
`INGAME`, `MISSIONSUMMARY`, `SINGLEPLAYERGAME`, `OPTIONS`,
`OPTIONSCONTROLS`, `OPTIONSDISPLAY`, `OPTIONSAUDIO`, `HOSTGAME`,
`JOINGAME`, `REPLAYGAME`, `TESTGAME`.

### Keybind editor

The `keybind` namespace edits the same control-binding profiles you
configure under Options → Controls. Useful for setting up known
control schemes in tests, or scripting bulk changes.

| Command | Flags / positional | Returns |
|---------|-------------------|---------|
| `keybind list` | — | `{profiles:[…], current}` |
| `keybind actions` | — | `{actions:[…]}` — every bindable action ID (jump, fire, …) |
| `keybind get` | `[--profile N] [--action A]` | `{bindings:{action:[…]}}` — defaults to current profile / all actions |
| `keybind put` | `--profile N --action A --bindings KEY:F PAD:south` | `{}`. Comma joins keys into an AND-chord (e.g. `--bindings KEY:Up,KEY:Left` = "press both"); space joins them as separate alternatives ("press either"). |
| `keybind unset` | `--profile N --action A` | `{}` |
| `keybind use <profile>` | positional or `--profile N` | `{}` — switches the active profile |
| `keybind new` | `--profile N [--from M]` | `{}` — creates a new profile, optionally cloned from another |
| `keybind delete <profile>` | positional or `--profile N` | `{}` |

Note: `--profile` and `--action` are kept as strings even when
numeric. The CLI knows about this so `--profile 1` doesn't get
auto-converted to a number and silently target the wrong profile.

### Hot-reloading game data

```bash
cli --port "$PORT" gas reload
```

Re-runs the C++ GAS loader against the running game's data directory
and returns the same `{file, instancePath, code, message}` error shape
as `gas validate`. Only safe from `NONE` / `MAINMENU` / `LOBBY` /
`MISSIONSUMMARY` — errors `WRONG_STATE` mid-game.

The exit code is **0 as long as the reload ran**. Whether the new
files were clean shows up in the response's `errors[]` array, not the
exit code. CI pipelines that need a hard pass/fail on data validity
should run `gas validate` first and gate the `gas reload` on its
exit.

## Validating game-data JSON (no game required)

```bash
bun clients/cli/index.ts gas validate shared/assets/gas
# → {"ok":true,"errors":[]}                     (exit 0)
# → {"ok":false,"errors":[{...}, {...}]}        (exit 1)
```

GAS = "Game Asset Spec," the JSON files under `shared/assets/gas/`
that define agencies, weapons, items, enemies, etc. This command runs
in-process — no game has to be running, no port required. Errors carry
`instancePath` as an RFC 6901 JSON Pointer, which round-trips cleanly
into a code-edit against the source file.

The TypeScript schemas live in `shared/gas-validation/schemas.ts` and
mirror the C++ structs in `clients/silencer/src/gas/gasloader.h`. If
the schemas drift from the loader, `gas reload` will catch it.

## Fake players in a lobby

When you're working on a multiplayer feature and need other authenticated
players to be present — talking, creating games, showing up in the
presence list — the `lobby` namespace gives you that without launching
real game clients.

It works by talking to a small background process called
`silencer-lobbyd`, which holds N authenticated lobby connections in a
single process. The first `lobby spawn` you run auto-starts the
background process; it auto-exits when its last session is killed.
You never start it manually.

```bash
# Bring two authed presences online.
bun clients/cli/index.ts lobby spawn --as alice --host 127.0.0.1 --port 15170 \
                                      --version "" --user alice --pass alice
bun clients/cli/index.ts lobby spawn --as bob   --host 127.0.0.1 --port 15170 \
                                      --version "" --user bob   --pass bob

# Watch what alice sees while bob talks.
bun clients/cli/index.ts lobby tail --as alice &
TAIL_PID=$!
bun clients/cli/index.ts lobby chat --as bob   --channel main --text "hi"
bun clients/cli/index.ts lobby chat --as alice --channel main --text "hey"

# Tear everything down — also exits the background process.
kill $TAIL_PID
bun clients/cli/index.ts lobby kill --all
```

Subsequent commands reference each session by the `--as <name>` you
gave it on `spawn`. Credentials live only in memory — passwords never
touch disk — so if the background process dies, all sessions die with
it and you have to respawn.

| Command | Flags / positional | Returns |
|---------|-------------------|---------|
| `lobby spawn` | `--as N --host H --port P --version V --user U --pass P [--platform 0\|1\|2]` | `{accountId}` — completes after the lobby version+auth handshake; rejects on bad creds |
| `lobby ls` | — | `{sessions:[{name,state,accountId,host,port}]}` — every active fake player |
| `lobby kill` | `--as N` *or* `--all` | `{}` — disconnects and removes; `--all` also winds down the background process |
| `lobby chat` | `--as N --channel C --text T` | `{}` |
| `lobby join_channel` | `--as N --channel C` | `{}` |
| `lobby game create` | `--as N --name X [--map M --max-players 8 --max-teams 2 --password P]` | `{gameId}` — resolves on the echoed `newGame` event (10 s timeout) |
| `lobby game join` | `--as N --id GAMEID` | `{}` — sends `setGame(gameId, Lobby)` |
| `lobby tail` | `--as N` | streams one JSON event per line (`chat`, `presence`, `channel`, `newGame`, `delGame`, `stateChanged`) until the session ends or you SIGINT |

`lobby tail` is the only streaming command. Pipe it into `jq` or
`grep` like any JSON-lines stream. A given session can only be tailed
by one CLI invocation at a time — a second concurrent tail returns
`ALREADY_TAILING`.

The background process puts its unix socket and log file under
`$SILENCER_LOBBYD_DIR` (override) or the platform default
(`$XDG_RUNTIME_DIR/silencer/` on Linux, `$TMPDIR/silencer/` on macOS,
`%LOCALAPPDATA%\Silencer\lobbyd\` on Windows).

## Common patterns

### Navigate menus

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port); PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label OPTIONS
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
cli --port "$PORT" back
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000
```

### Discover what's on screen

```bash
cli --port "$PORT" inspect | jq '.widgets[] | {id,kind,label}'
```

### Read live world state

```bash
cli --port "$PORT" world_state | jq '.players[] | {id,hp,x,y}'
```

### Pause + step + resume (single-player only)

```bash
cli --port "$PORT" pause
cli --port "$PORT" step --frames 30   # advances 30 frames, re-pauses
cli --port "$PORT" resume
```

### Sanity-check fake players

```bash
bun clients/cli/index.ts lobby ls | jq '.sessions[] | {name,state,accountId}'
```

## Gotchas

- **macOS binary path.** Lives at
  `clients/silencer/build/Silencer.app/Contents/MacOS/Silencer`, not
  `build/silencer`. `lib.sh` handles this for you.
- **`--headless` required in CI.** Without it, the game opens an SDL
  window. The control socket works either way.
- **Single-player only for `pause` / `step`.** They error `WRONG_STATE`
  in live multiplayer (`peercount > 1` and INGAME). `step` always
  re-pauses after the span ends.
- **One CLI at a time per game.** The control socket accepts one
  client per session — don't run parallel CLI invocations against the
  same port.
- **Timeouts are milliseconds**, not seconds (`--timeout-ms`, default
  `5000`).
- **Label matching is case-insensitive** and must be unambiguous —
  multiple matches return `WIDGET_AMBIGUOUS`.
- **`click` only matches buttons or toggles.** Use `set_text` for
  textboxes, `select` for selectboxes.
- **Logs.** `start_silencer` redirects the game's stdout+stderr to
  `/tmp/silencer-e2e-<PORT>.log`. Read it when the game misbehaves.
- **`gas validate` is local.** No game, no `--port`. Don't wrap it in
  `start_silencer` / `stop_silencer` — it just reads files. The
  in-process command registry lives at the top of
  `clients/cli/index.ts`.
- **`gas reload` is screen-gated.** Errors `WRONG_STATE` outside
  `NONE` / `MAINMENU` / `LOBBY` / `MISSIONSUMMARY`; the loader does
  not run mid-game.
- **`lobby` has its own background process.** `silencer-lobbyd` is
  separate from the game; it auto-spawns on first `lobby spawn` and
  auto-exits when its last session dies. To force a clean restart:
  `lobby kill --all`. Its log file is at
  `$SILENCER_LOBBYD_DIR/lobbyd.log` — read it when `lobby spawn` times
  out. On macOS, `$TMPDIR` is per-user and ephemeral (`periodic` GCs
  files after ~3 days idle), which is fine for dev.
- **Lobby creds are in memory only.** `--user` / `--pass` are passed
  once on `spawn` and never written to disk. Subsequent commands
  reference the session by `--as <name>`. If the background process
  dies (e.g. SIGINT), all sessions go with it.
- **`lobby game create` mapHash defaults to all-zeros.** Fine for
  emitting test traffic; a production lobby may reject games without
  a real SHA-1 of the map file. If you need a joinable game from the
  CLI, extend `commands.ts::game` with `--map-hash` / `--listen-port`
  flags.

## If you need to skip the wrapper

The wire protocol is one JSON object per line, both directions.

Request:
```json
{"id": 1, "op": "click", "args": {"label": "OPTIONS"}}
```

Success:
```json
{"id": 1, "ok": true, "result": {"widget_id": 17}}
```

Error:
```json
{"id": 1, "ok": false, "code": "WIDGET_NOT_FOUND", "error": "no widget matches \"X\""}
```

Common error codes: `BAD_REQUEST`, `UNKNOWN_OP`, `WRONG_STATE`,
`WIDGET_NOT_FOUND`, `WIDGET_AMBIGUOUS`, `TIMEOUT`, `INTERNAL`. The CLI
wrapper exits 1 on any `ok:false` and prints `[CODE] error` to stderr.
You almost never need to speak this protocol directly — use the CLI.
