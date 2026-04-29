---
name: using-silencer-cli
description: Use when you need to drive the Silencer game from a terminal — verifying UI changes, navigating menus, taking screenshots, reading game state, validating GAS data files, or rebinding controls — without a human at the keyboard. Most ops talk JSON-lines TCP to a running silencer client; a few (e.g. `gas validate`) run locally with no daemon.
---

# using-silencer-cli

This skill teaches agents how to drive the Silencer game client via the
`silencer-cli` Bun+TS wrapper that talks JSON-lines TCP to the game's
`--control-port`.

## What it is

`clients/cli/index.ts` — a thin command-line client over the game's TCP
control socket. Each invocation sends one JSON command, prints the JSON
result, and exits.

```
bun clients/cli/index.ts --port <PORT> <op> [--key value ...]
```

Exit codes: `0` on success (JSON `result` to stdout), `1` on op error
(`[CODE] message` to stderr), `2` on transport failure.

The daemon is the normal Silencer binary launched with
`--headless --control-port <PORT>`.

## Quickstart

Use the E2E harness helpers — they handle binary detection across
macOS/Linux/Windows, pick a free port, and manage the daemon PID.

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

# ... do work (see ops below) ...
```

`lib.sh` defines a `cli` shell function (`bun .../clients/cli/index.ts`) so
paths with spaces don't word-split. Pass `--port "$PORT"` on every invocation.

## Supported ops

All ops accept flags as `--key value`. Where noted, a few accept a
positional shorthand (`click LABEL`, `set_text LABEL TEXT`,
`select LABEL N_OR_TEXT`).

| Op | Flags | Result |
|----|-------|--------|
| `ping` | — | `{version, build, frame, paused}` |
| `state` | — | `{state, current_interface_id, frame, paused}` |
| `inspect` | `[--interface-id N]` | `{widgets:[{id,x,y,kind,label,w,h,enabled,...}], interface_id}` — defaults to current interface |
| `world_state` | — | `{map, peers, players:[{id,hp,x,y}], objects_count}` |
| `click` | `--label X` or `--id N` | `{widget_id}`. Matches BUTTON or TOGGLE; toggles flip `selected`. |
| `set_text` | `--label X --text Y` | `{}` — clears textbox then types |
| `select` | `--label X --index N` or `--text Y` | `{}` — sets selectbox index |
| `back` | — | `{went_back: bool}` |
| `screenshot` | `[--out PATH]` | `{path}`. If `--out` omitted, daemon writes `$TEMP/silencer-<frame>.png` (or `/tmp/...` on Unix). |
| `pause` | — | `{}` — errors `WRONG_STATE` in live multiplayer |
| `resume` | — | `{}` |
| `step` | `--frames N` *or* `--ms N` | `{}` — advances sim then re-pauses; one of the two flags is required |
| `wait_frames` | `--n N` | `{}` — replies after N rendered frames |
| `wait_ms` | `--n N` | `{}` — replies after N wallclock ms |
| `wait_for_state` | `--state X [--timeout-ms 5000]` | `{}` or `TIMEOUT` error. **Timeouts are milliseconds.** |
| `quit` | — | `{}` — sets `quitRequested`; daemon exits cleanly |

### Noun-first ops: `gas` and `keybind`

A few ops use a `<noun> <subop>` shape (`gas validate`, `keybind put`, etc).
The first positional after the noun is the subop; flags follow as usual.

| Op | Flags / positional | Result | Daemon? |
|----|-------------------|--------|---------|
| `gas validate <dir>` | `--dir PATH` (or trailing positional) | `{ok, errors:[{file,instancePath,code,message}]}`. Exit 1 if `errors[]` non-empty. | **no** — runs in-process via `validateDirectory`; never opens the control socket |
| `gas reload` | — | `{counts:{agencies,weapons,items,enemies,abilities,gameObjects,terminals}, errors:[…]}` — re-runs the C++ GAS loader against the daemon's gas dir | yes |
| `keybind list` | — | `{profiles:[…], current}` | yes |
| `keybind actions` | — | `{actions:[…]}` — every bindable action id | yes |
| `keybind get` | `[--profile N] [--action A]` | `{bindings:{action:[…]}}` — defaults to current profile / all actions | yes |
| `keybind put` | `--profile N --action A --bindings KEY:F PAD:south` | `{}` — comma joins keys into an AND-chord (e.g. `--bindings KEY:Up,KEY:Left`) | yes |
| `keybind unset` | `--profile N --action A` | `{}` | yes |
| `keybind use <profile>` | positional or `--profile N` | `{}` — switches the active profile | yes |
| `keybind new` | `--profile N [--from M]` | `{}` — creates a new profile, optionally seeded from another | yes |
| `keybind delete <profile>` | positional or `--profile N` | `{}` | yes |

`--profile` and `--action` are kept as strings even when numeric; the
parser knows about this via `STRING_FLAGS` in `clients/cli/index.ts` so
`--profile 1` doesn't silently retarget profile `"1"` vs `1`.

### Validating GAS data files (no daemon)

```bash
bun clients/cli/index.ts gas validate shared/assets/gas
# → {"ok":true,"errors":[]}                       (exit 0)
# → {"ok":false,"errors":[{...}, {...}]}          (exit 1)
```

Use this in a remediation loop to drive edits against
`shared/assets/gas/*.json`. Errors carry `instancePath` as an RFC 6901
JSON Pointer that round-trips into an Edit against the source file.
Schema source is `shared/gas-validation/schemas.ts`, which mirrors the
C++ structs in `clients/silencer/src/gas/gasloader.h`.

### Hot-reloading GAS on the daemon

```bash
cli --port "$PORT" gas reload
```

Re-runs `GASLoader::Load()` against the daemon's gas directory and
returns the same `{file, instancePath, code, message}` error shape as
`gas validate`. Only safe from `NONE` / `MAINMENU` / `LOBBY` /
`MISSIONSUMMARY` — errors `WRONG_STATE` mid-game.

### State names

`state` and `wait_for_state --state` use the daemon's uppercase state
names (defined in `Game::StateName`):

`NONE`, `FADEOUT`, `MAINMENU`, `LOBBYCONNECT`, `LOBBY`, `UPDATING`,
`INGAME`, `MISSIONSUMMARY`, `SINGLEPLAYERGAME`, `OPTIONS`,
`OPTIONSCONTROLS`, `OPTIONSDISPLAY`, `OPTIONSAUDIO`, `HOSTGAME`,
`JOINGAME`, `REPLAYGAME`, `TESTGAME`.

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

### Screenshot a screen

```bash
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" screenshot --out /tmp/main.png
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

## Wire protocol

One JSON object per line, both directions.

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
exits 1 on any `ok:false` and prints `[CODE] error` to stderr.

You rarely need to speak the protocol directly — use the CLI wrapper.

## Gotchas

- **macOS binary path.** Lives at
  `clients/silencer/build/Silencer.app/Contents/MacOS/Silencer`, not
  `build/silencer`. `lib.sh` handles this automatically.
- **`--headless` required in CI.** Without it, the daemon opens an SDL
  window. The control socket still works either way.
- **Single-player only for `pause`/`step`.** `pause` errors with
  `WRONG_STATE` in live multiplayer (`peercount > 1` and INGAME).
  `step` always sets `paused = true` after the span ends.
- **One connection at a time.** The control socket accepts one client
  per session — don't run parallel CLI invocations against the same
  port.
- **Timeouts are in milliseconds**, not seconds (`--timeout-ms`,
  default `5000`).
- **Label matching is case-insensitive** and must be unambiguous —
  multiple matches return `WIDGET_AMBIGUOUS`.
- **`click` only matches BUTTON or TOGGLE.** Use `set_text` for
  textboxes, `select` for selectboxes.
- **Logs.** `start_silencer` redirects daemon stdout+stderr to
  `/tmp/silencer-e2e-<PORT>.log`. Check it when the daemon misbehaves.
- **`gas validate` is a local op.** No daemon, no `--port`. Don't wrap
  it in `start_silencer`/`stop_silencer` — it just reads files. The
  registry of local ops lives in `LOCAL_OPS` at the top of
  `clients/cli/index.ts`.
- **`gas reload` is state-gated.** Errors `WRONG_STATE` outside
  `NONE`/`MAINMENU`/`LOBBY`/`MISSIONSUMMARY`; loader does not run
  mid-game.
