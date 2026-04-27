---
name: using-silencer-cli
description: Use when you need to drive the Silencer game from a terminal — verifying UI changes, navigating menus, taking screenshots, reading game state — without a human at the keyboard. The CLI talks JSON-lines TCP to a long-running silencer client.
---

# using-silencer-cli

This skill teaches agents how to drive the Silencer game client via the
`silencer-cli` Bun+TS wrapper that talks JSON-lines TCP to the game's
`--control-port`.

## What it is

`clients/silencer-cli/index.ts` — a thin command-line client over the
game's TCP control socket (`--control-port`). Each invocation sends one
JSON command and prints the JSON response.

```
bun clients/silencer-cli/index.ts --port <PORT> <op> [args...]
```

The daemon is the normal Silencer binary launched with `--headless
--control-port <PORT>`. On macOS it is the `.app` bundle binary; on Linux
it is the lowercase `silencer` binary.

## Quickstart

Use the E2E harness helpers so binary detection and port allocation are
handled for you:

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
wait_alive "$PORT"

# ... do work (see ops below) ...

stop_silencer "$PID" "$PORT"
```

`lib.sh` auto-detects the binary across macOS/Linux/Windows, picks a free
port, and manages the daemon PID. Prefer this over rolling your own
start/stop logic.

If you need a one-shot manual start outside the harness:

```bash
PORT=5170
. tests/cli-agent/e2e/lib.sh
PID=$(start_silencer "$PORT")
wait_alive "$PORT"
```

## CLI alias

```bash
CLI="bun clients/silencer-cli/index.ts --port $PORT"
```

## Supported ops

| Op | Args | What it does |
|----|------|--------------|
| `ping` | — | Health check — returns `{"op":"pong"}` |
| `state` | — | Current game-state name (e.g. `MainMenu`, `Lobby`) |
| `inspect` | `<label>` | Dump a widget tree rooted at the named widget |
| `world_state` | — | Live snapshot: players, positions, health, … |
| `click` | `<label>` | Simulate a click on a widget by label |
| `set_text` | `<label> <text>` | Type into a text box |
| `select` | `<label> <value>` | Choose a drop-down option |
| `back` | — | Simulate Escape / back button |
| `screenshot` | `[path]` | Save a PNG to *path* (default `/tmp/silencer-screenshot.png`) |
| `pause` | — | Pause simulation (single-player only) |
| `resume` | — | Resume simulation |
| `step` | `[n]` | Advance *n* frames while paused |
| `wait_state` | `<state> [timeout_s]` | Block until game reaches *state* |
| `wait_widget` | `<label> [timeout_s]` | Block until widget appears |
| `quit` | — | Ask the daemon to exit cleanly |

## Common patterns

### Navigate to a menu and verify a widget

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port); PID=$(start_silencer "$PORT"); wait_alive "$PORT"
CLI="bun clients/silencer-cli/index.ts --port $PORT"

# Wait for main menu
$CLI wait_state MainMenu

# Click Play
$CLI click Play

# Confirm we reached lobby
$CLI wait_state Lobby

stop_silencer "$PID" "$PORT"
```

### Take a screenshot for visual verification

```bash
$CLI screenshot /tmp/my-screen.png
# Opens or inspects /tmp/my-screen.png to check rendering
```

### Read widget tree for a screen

```bash
$CLI inspect Root | jq .
```

### Inspect world state mid-game

```bash
$CLI world_state | jq '.players[] | {name,health,pos}'
```

### Set text in a field

```bash
$CLI set_text "Username" "testuser42"
$CLI click "Connect"
```

## JSON protocol (low-level)

Every message is a single JSON object terminated by `\n`. The daemon
reads from the TCP socket opened at `--control-port` and writes one
response per request.

Request:
```json
{"op": "click", "label": "Play"}
```

Success response:
```json
{"op": "click", "ok": true}
```

Error response:
```json
{"op": "click", "error": "widget not found: Play"}
```

You rarely need to speak the protocol directly — use the CLI wrapper.

## Gotchas

- **macOS binary path.** The binary lives at
  `clients/silencer/build/Silencer.app/Contents/MacOS/Silencer`, not
  `clients/silencer/build/silencer`. `lib.sh` handles this automatically.
- **--headless required.** Without `--headless`, the game opens SDL
  video/audio and the control socket still works, but you'll get a window.
  In CI always pass `--headless`.
- **Single-player only for pause/step.** `pause`, `resume`, and `step`
  error if the game is in multiplayer mode.
- **One connection at a time.** The control socket accepts one client;
  don't open parallel CLI invocations against the same port.
- **wait_* timeout.** Default timeout is 10 s. Pass a second arg to extend:
  `$CLI wait_state Lobby 30`.
- **Logs.** `start_silencer` redirects stdout+stderr to
  `/tmp/silencer-e2e-<PORT>.log`. Check that file when the daemon
  misbehaves.
