# tests/cli-agent/ — CLI agent control E2E suite

End-to-end coverage of the `silencer --headless --control-port`
JSON-lines protocol via the `clients/cli/` Bun wrapper. Each
scenario boots a fresh silencer on an ephemeral port, drives it
through the CLI, and tears it down on `EXIT`.

## Test policy

**Integration/E2E by default.** This is the primary test suite for
the silencer client. New behavior is verified by adding a numbered
scenario here that drives the full stack (real binary, real TCP,
real CLI). Internal refactors that don't change the control protocol
or visible UI flow don't need tests.

**Unit tests are strongly discouraged** — especially mocked,
low-signal ones. Add a unit test only when (a) the logic is
extremely vital and (b) the test exercises the *real* behavior, not
a mock of it. If you find yourself mocking, write an E2E scenario
instead.

## Run

```bash
bash tests/cli-agent/run.sh
```

Requires a built silencer under `clients/silencer/build/` —
`e2e/lib.sh` auto-detects the platform layout (`Silencer.app`
bundle / `silencer` / `Silencer.exe`). Set `SILENCER_BIN` to override.

## Layout

- `run.sh` — runs every `e2e/[0-9]*_*.sh` in lex order; non-zero
  exits are tallied and the runner fails if any did.
- `e2e/lib.sh` — sourced by every scenario. Exposes `cli`,
  `pick_port`, `start_silencer`, `wait_alive`, `stop_silencer`.
  Cross-platform binary detection lives here.
- `e2e/NN_*.sh` — one scenario per file. Numeric prefix orders
  execution; pad with leading zeros if you need to insert one.

## Adding a scenario

1. Copy `e2e/00_ping.sh` as a minimal template.
2. `. "$(dirname "$0")/lib.sh"`, then `pick_port` →
   `start_silencer` → `trap stop_silencer EXIT` → `wait_alive`.
3. Drive the game with `cli --port "$PORT" <op>`. Op reference:
   `shared/skills/cli/SKILL.md`.
4. End with `echo "PASS NN_name"` so runner output is greppable.

## Gotchas

- Per-run logs land in `/tmp/silencer-e2e-<port>.log` — check there
  when a scenario hangs or `wait_alive` times out.
- `wait_alive` polls `cli ping` for ~30 s (`e2e/lib.sh:41`). If CI
  is consistently slow, bump the loop there rather than per-scenario.
- Scenarios are parallel-safe (ephemeral ports + per-scenario PIDs),
  but `run.sh` runs them serially for stable interleaved output.
