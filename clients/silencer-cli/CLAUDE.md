# clients/silencer-cli — agent control wrapper

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

## Wire protocol

See `docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`.
