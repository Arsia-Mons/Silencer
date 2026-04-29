# Silencer

Multiplayer 2D action game (C++/SDL3) plus a self-hosted Go lobby
server, an admin web app (Next.js), and an admin API (Bun+TS).
Rebranded from zSILENCER — use "Silencer" in new content, rename
old identifiers opportunistically when touching the file. Same
applies to SDL: we migrated SDL2 → SDL3, so rename stale SDL2
references opportunistically when you touch surrounding code/docs.

## Tech stack

- Game client: C++14 / SDL3 / CMake
- Lobby server: Go (stdlib + `mongo-driver`/`amqp091-go`, both optional)
- Admin web + API: Bun + TypeScript + oxfmt
- Infra: Docker Compose, Terraform (AWS)

## Layout

Each component owns its own `CLAUDE.md` with build/run/test/gotchas:

- `clients/silencer/` — C++ game + dedicated server (same binary)
- `services/lobby/` — Go lobby server
- `services/admin-api/` — Express → Bun+TS admin backend
- `web/admin/` — Next.js admin dashboard + level designer
- `shared/assets/` — runtime game assets (sprites, tiles, sounds, levels)
- `shared/gas-validation/` — TS package: GAS schemas + validator shared by admin web and `silencer-cli`
- `shared/icons/` — app icons used by `clients/silencer` build
- `shared/skills/` — Claude Code skills surfaced to agents via `.claude/skills/` symlinks
- `infra/terraform/` — AWS infra

> Layout migration in progress. See
> [docs/plans/2026-04-25-monorepo-restructure.md](docs/plans/2026-04-25-monorepo-restructure.md).

## Universal rules

1. **JavaScript = Bun + TypeScript + oxfmt.** No `node`,
   `npm`/`pnpm`/`yarn`, `.js` source, or alternative formatters.
   Migrate as you touch. `services/admin-api/` and `web/admin/`
   are mid-migration: runtime is Bun, source is still `.js` —
   don't add new `.js` files unless you're editing adjacent ones.
2. **Every component dir has a `CLAUDE.md`** with an `AGENTS.md`
   symlink alongside (one-line stub on Windows). Keep them
   identical.
3. **No backwards-compat shims during refactors** unless asked.
   Update everything to the new design and delete the old.
4. **Verify end-to-end before claiming done** for cross-service
   features. Real request through the full stack.
5. **Ask early when ambiguous.** Cheaper than redoing it.
6. **Combat overengineering.** No features beyond what was asked.
   No abstractions for single-use code. No "flexibility" or
   "configurability" that wasn't requested. No error handling for
   impossible scenarios. If 200 lines could be 50, rewrite it.

## Bun workspaces

Bun packages share a single root `bun.lock`. Every Bun-runtime dir
(`clients/cli/`, `services/admin-api/`, `shared/fonts/`,
`shared/gas-validation/`, `web/admin/`, `web/website/`) is listed in
the root `package.json`'s `workspaces` array. Run `bun install` from
the repo root, never inside a sub-package.

- **Adding a workspace dep on another local package:** declare it as
  `"@silencer/<name>": "workspace:*"`. The lockfile resolves it to
  the in-tree path.
- **Preventing version drift:** when two workspaces declare the same
  third-party dep, identical version ranges dedupe in the lockfile
  automatically. Different ranges install side-by-side — that's the
  drift signal. To force alignment, add the dep to the root
  `package.json`'s `"overrides"` map. Treat per-workspace divergence
  as a deliberate exception, documented in that workspace's CLAUDE.md.
- **Docker builds:** images copy the workspace root + the target
  package's manifest, then `bun install --filter <pkg>`. Don't
  reintroduce per-package lockfiles.

## More

- Writing CLAUDE.md well: [docs/writing-a-good-claude-md.md](docs/writing-a-good-claude-md.md)
- Production setup: `docs/production.md`
- UI design system + asset formats: `docs/design-system.md`
- Intent docs: `docs/plans/`
- TDD execution plans: `docs/superpowers/plans/`
- Specs: `docs/superpowers/specs/`
