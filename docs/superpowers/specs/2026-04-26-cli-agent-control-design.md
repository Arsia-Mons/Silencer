# CLI agent control for the Silencer client — design

**Date:** 2026-04-26
**Scope:** A control channel on the Silencer client + a stateless
CLI wrapper, so a coding agent (Claude Code, etc.) can drive the
running game from a terminal — analogous to how the chrome MCP
drives a real browser.

## Motivation

Today there's no way for an agent to verify UI changes, navigate
menus, or read game state without a human at the keyboard. We want
the same leverage over `silencer` that we have over a browser:
launch it, drive it, screenshot it, inspect it.

## Non-goals (v1)

- Driving in-game actors (move, aim, fire). Punted to a v2 spec
  once the foundation is in place.
- Multi-agent / remote control. v1 binds to `127.0.0.1` only and
  expects the agent to run on the same host as the client.
- Authentication. Localhost-only is the boundary.
- Determinism guarantees / replay capture beyond what the existing
  replay machinery provides.

## Architecture

Two binaries, one transport.

```
┌────────────────────────┐    JSON-lines TCP     ┌──────────────────┐
│ silencer (game)        │ ◄────────────────────│ silencer-cli      │
│  long-running          │  127.0.0.1:<port>     │  short-lived      │
│  visible OR --headless │                       │  one cmd per run  │
│  control thread →      │                       │  Bun + TypeScript │
│  per-frame queue       │                       └──────────────────┘
└────────────────────────┘
```

- `silencer --control-port 5170 [--headless]` starts the game with
  an accept thread bound to `127.0.0.1:5170`.
- The accept thread parses one JSON command per line, pushes a
  `ControlCommand` (with a `std::promise<Reply>`) onto a lock-guarded
  queue, and writes the reply back when the promise resolves.
- `Game::Loop` (`clients/silencer/src/game.cpp`) drains the queue
  at one well-defined point per frame. All mutation of `Game`,
  `World`, `Interface`, `Input` runs on the game thread — the accept
  thread only does socket I/O and JSON.
- `silencer-cli <verb>` opens a socket, sends one line, prints the
  reply, exits. The game stays running across many invocations —
  that's the "stateful interaction" the agent needs.

## Loop integration

`Game::Loop` today is roughly:

```cpp
HandleSDLEvents();   // SDL → keystate, widget clicks
Tick();              // world simulation, interface processing
Present();           // render to window/screenbuffer
```

Becomes:

```cpp
HandleSDLEvents();
DrainControlQueue();   // immediate cmds + scheduled wakes
if (!paused || step_budget_remaining()) {
    Tick();
    consume_step_budget();   // frames or wallclock ms
}
Present();
PostFrameReplies();    // post-render cmds (screenshot)
```

**Why drain before `Tick`, not before `HandleSDLEvents`:** UI
commands (e.g. `click`) call into widget logic directly so the
click resolves *this* frame; `Tick` then runs the consequences.
State queries see a coherent post-input world.

**Three command shapes by reply timing:**

1. **Immediate** — pure reads and direct mutations. Reply fulfilled
   inside `DrainControlQueue`.
2. **Post-render** — `screenshot`. Captured from `screenbuffer`
   after `Present`, fulfilled in `PostFrameReplies`. One-frame
   latency.
3. **Multi-frame wait** — `wait_for_state`, `wait_frames`,
   `wait_ms`, `step`. Held in a pending-waits list, ticked each
   frame, replied when the deadline hits.

Concurrent connections are accepted but commands serialise through
the single per-frame queue. No parallel mutation of game state.

## Command surface (v1)

JSON in: `{"id": int, "op": "...", "args": {...}}`
JSON out: `{"id": int, "ok": bool, "result": {...}, "error": "...", "code": "..."}`

**Introspection (immediate)**

- `ping` → `{version, build, frame, paused}`.
- `state` → `{state: "MAINMENU"|..., current_interface_id, frame, paused, substate: {...}}`.
- `inspect [--interface-id N]` → `{widgets: [{id, kind, label, text, enabled, x, y, w, h}, ...]}`.
  Discovery for whichever screen is current.
- `world_state` → `{map, peers, players: [{id, name, agency, hp, x, y}], objects_count}`.
  Best-effort summary; richer dump deferred to v2.

**UI actions (immediate, mutate widgets directly — no SDL event roundtrip)**

- `click <id-or-label>` — `Button`, `Toggle`. Label match exact,
  case-insensitive; ambiguous → `WIDGET_AMBIGUOUS`.
- `set_text <id-or-label> <text>` — `Textbox`.
- `select <id-or-label> <index-or-text>` — `Selectbox`/list.
- `back` — equivalent to Escape / `GoBack()`.

**Effects**

- `screenshot [--out <path>]` → writes PNG (default
  `<tempdir>/silencer-<frame>.png`, where `<tempdir>` is the
  platform temp dir), returns absolute path.
- `quit` — clean shutdown.

**Time and pacing**

- `wait_for_state <STATE> [--timeout-ms N]`
- `wait_frames N`
- `wait_ms N`
- `pause`, `resume`
- `step --frames N` | `step --ms N` (atomically resumes for the
  span, then re-pauses)

### Multiplayer pause caveat

`pause` / `step` freeze local simulation. In multiplayer INGAME
with live peers this *will* desync — the v1 contract is "supported
in MAINMENU, OPTIONS, replay, and SP/test contexts; returns
`WRONG_STATE` in multiplayer authority/peer mode". The actor-driving
v2 spec will revisit this.

## Wire format

Newline-delimited JSON, both directions, one object per line.

```
→ {"id": 1, "op": "click", "args": {"label": "Connect"}}
← {"id": 1, "ok": true, "result": {"interface_id": 4}}

→ {"id": 2, "op": "click", "args": {"label": "Nope"}}
← {"id": 2, "ok": false, "code": "WIDGET_NOT_FOUND", "error": "no widget matches \"Nope\""}
```

`id` is client-assigned and echoed back so a single persistent
connection can multiplex. `silencer-cli` opens one connection per
command for simplicity.

**Error codes** (stable, agent-branchable):
`BAD_REQUEST`, `UNKNOWN_OP`, `WIDGET_NOT_FOUND`, `WIDGET_AMBIGUOUS`,
`WRONG_STATE`, `TIMEOUT`, `INTERNAL`.

`silencer-cli` exits 0 on `ok:true`, 1 on `ok:false` (printing
`code` and `error`), 2 on transport failure.

## Code layout

**New files in `clients/silencer/src/`:**

- `controlserver.{h,cpp}` — accept thread, per-connection read/write
  loop, command queue, reply promises.
- `controldispatch.{h,cpp}` — `op` → handler map. Adding a command =
  one entry + one function.
- `controljson.{h,cpp}` — minimal hand-rolled JSON reader/writer.
  Surface is tiny (objects of strings/numbers/bools/arrays). No
  external dep; swap to nlohmann/json later if it gets painful.

**Edits:**

- `main.cpp` — parse `--control-port` and `--headless`.
- `game.{h,cpp}` — fields: `controlserver`, `paused`, step budget
  (frames remaining + wallclock deadline), pending-waits list. Add
  `DrainControlQueue()` / `PostFrameReplies()` calls. `Tick`
  early-returns when paused with no step budget left. Add
  `StateName()` for the existing state enum.
- `interface.{h,cpp}` — `FindWidgetByLabel`, plus introspection
  getters (`kind`, `label`, `text`, `enabled`, rect). Some exist
  already; consolidate.
- `renderer.cpp` — `CapturePNG(const char* path)` reading from
  `screenbuffer`. Vendor stb_image_write (single-header, MIT) under
  `clients/silencer/third_party/`.
- Headless toggle: `--headless` skips window creation and routes
  rendering to an offscreen `screenbuffer`. Confirm during
  implementation whether this composes cleanly with the existing
  `RenderDevice` abstraction or needs a new path.
- `CMakeLists.txt` — new sources, stb include.

**`clients/silencer-cli/`** (new component, Bun + TypeScript per
universal rules):

- `index.ts` — argv parsing, TCP socket, one JSON line in/out,
  print + exit.
- `package.json` with `bin: { "silencer-cli": "./index.ts" }`.
- `CLAUDE.md` + `AGENTS.md` symlink.

## Testing

- **Unit (C++).** JSON reader/writer round-trip, dispatch table
  coverage. The client has no unit harness today; add a minimal
  `tests/cli-agent/cpp/` linking the relevant TUs. If the cost of
  standing this up is high, drop it and lean on E2E.
- **E2E (where the value is).** Bash scripts under
  `tests/cli-agent/e2e/`. Each spawns
  `silencer --headless --control-port <random>`, polls `ping` until
  alive, drives a scenario (`state` → `click "Options"` →
  `wait_for_state OPTIONS` → `screenshot --out /tmp/x.png`),
  asserts exit codes and PNG existence/non-zero size. Run via
  `tests/cli-agent/run.sh`.
- **Acceptance.** A coding agent (no human help) can: launch the
  daemon, navigate MAINMENU → OPTIONS and back, screenshot each,
  confirm the PNGs render the expected screens. If an agent can't
  do that unaided, v1 isn't done.

## Out of scope (tracked for v2)

- `input_press`/`input_release`/`set_aim` and the rest of the
  `Input` struct surface — driving actors in-game.
- Full `world_state` graph (every replicated `Object`).
- Bot/NPC scripting hooks.
- Multi-agent or remote control.
- Pause/step in live multiplayer.
