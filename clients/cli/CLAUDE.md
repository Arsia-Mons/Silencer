# clients/cli — Bun CLI for driving the game from scripts

Source for `silencer-cli`. To **use** the CLI, read
[`shared/skills/cli/SKILL.md`](../../shared/skills/cli/SKILL.md). The
rest of this file is for editing the CLI itself.

## How it fits

Stateless Bun + TypeScript wrapper. One invocation = one command,
routed three ways:

- **Game-control** commands → JSON-line TCP to a running game
  (`silencer --headless --control-port <P>`). Server side lives in
  `clients/silencer/src/control/`.
- **Local** commands (`gas validate`, anything in `LOCAL_OPS` at the
  top of `index.ts`) → in-process. No socket, no daemon. Used in
  tight edit loops where startup cost matters.
- **Lobby** commands → unix socket to `silencer-lobbyd`, auto-spawned.
  See [`src/lobby/CLAUDE.md`](src/lobby/CLAUDE.md).

`index.ts` does arg parsing, the routing decision, and the
read/write loop. Lobby handlers live under `src/lobby/`.

## Run during dev

```bash
bun ./index.ts --port 5170 click --label OPTIONS
# or: bun link  →  silencer-cli --port 5170 click --label OPTIONS
```

End-to-end tests source `tests/cli-agent/e2e/lib.sh` for cross-platform
binary detection and free-port allocation — the easiest way to
exercise a change against a real game build.

## Env / exit codes

- `SILENCER_CONTROL_HOST` / `SILENCER_CONTROL_PORT` — game-control TCP defaults.
- `SILENCER_LOBBYD_DIR` — overrides the lobby daemon's socket+log dir
  (see `src/lobby/paths.ts`).
- Exit `0` (ok, JSON to stdout) / `1` (`[CODE] msg` to stderr) /
  `2` (transport failure).

## Adding a game-control command

1. Add the C++ handler in `clients/silencer/src/control/`.
2. If args need special parsing, add entries to `STRING_FLAGS` /
   `VARIADIC_FLAGS` / `CHORD_SPLIT_FLAGS` in `index.ts`.
3. Document it in the SKILL.md table.

For lobby commands the boundary rules differ — see `src/lobby/CLAUDE.md`.

## Docs

- [`docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`](../../docs/superpowers/specs/2026-04-26-cli-agent-control-design.md)
  — full wire-protocol spec.
- [`shared/skills/cli/SKILL.md`](../../shared/skills/cli/SKILL.md)
  — user-facing guide; quick wire-protocol summary at the bottom.
