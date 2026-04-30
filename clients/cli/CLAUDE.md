# clients/cli — agent control wrapper

Stateless Bun + TypeScript CLI that talks JSON-lines TCP to a running
`silencer` started with `--control-port <P>`. One command per
invocation; the game stays up across many calls.

## Run

```bash
bun ./index.ts ping
bun ./index.ts --port 5170 click --label OPTIONS
bun ./index.ts wait_for_state --state OPTIONS --timeout-ms 3000
bun ./index.ts screenshot --out /tmp/x.png
```

Or install into PATH locally:

```bash
bun link
silencer-cli ping
```

## Env

- `SILENCER_CONTROL_HOST` (default `127.0.0.1`)
- `SILENCER_CONTROL_PORT` (default `5170`)

## Exit codes

- `0` — `ok:true`; result JSON to stdout.
- `1` — `ok:false`; `[CODE] error` to stderr.
- `2` — transport failure (connect refused / closed prematurely / etc).

## Lobby fake players

`lobby` namespace spawns persistent authenticated lobby presences in a
shared supervisor daemon. See [`src/lobby/CLAUDE.md`](src/lobby/CLAUDE.md).

```bash
silencer-cli lobby spawn --as alice --host LOBBY_HOST --port 15170 \
                         --version 1.2.3 --user alice --pass hunter2
silencer-cli lobby chat  --as alice --channel main --text "hi"
silencer-cli lobby tail  --as alice    # streams events until SIGINT
silencer-cli lobby kill  --as alice
```

Defaults co-locate socket+log at `$SILENCER_LOBBYD_DIR` (override) or
the platform default (`$XDG_RUNTIME_DIR/silencer/` on Linux,
`$TMPDIR/silencer/` on macOS, `%LOCALAPPDATA%\Silencer\lobbyd\` on
Windows).

## Wire protocol

See `docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`.
