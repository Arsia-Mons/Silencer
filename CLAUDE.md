# Silencer

Multiplayer 2D action game (C++/SDL2) plus a self-hosted Go lobby
server, an admin web app (Next.js), and an admin API (Bun+TS).
Rebranded from zSILENCER — use "Silencer" in new content, rename
old identifiers opportunistically when touching the file.

## Tech stack

- Game client: C++14 / SDL2 / CMake
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
- `shared/icons/` — app icons used by `clients/silencer` build
- `infra/terraform/` — AWS infra

> Layout migration in progress. See
> [docs/plans/2026-04-25-monorepo-restructure.md](docs/plans/2026-04-25-monorepo-restructure.md).

## Universal rules

1. **JavaScript = Bun + TypeScript + oxfmt.** No `node`,
   `npm`/`pnpm`/`yarn`, `.js` source, or alternative formatters.
   Migrate as you touch. **Exception:** `services/admin-api/` and
   `web/admin/` runtime + lockfile already on Bun (see Phase 1 of
   the production deployment plan), but their source is still `.js`
   pending the Phase 2 source migration. Don't add new `.js` files
   in those dirs unless you're touching adjacent existing `.js`.
2. **Every component dir has a `CLAUDE.md`** with an `AGENTS.md`
   symlink alongside (one-line stub on Windows). Keep them
   identical.
3. **No backwards-compat shims during refactors** unless asked.
   Update everything to the new design and delete the old.
4. **Verify end-to-end before claiming done** for cross-service
   features. Real request through the full stack.
5. **Ask early when ambiguous.** Cheaper than redoing it.

## More

- Writing CLAUDE.md well: [docs/writing-a-good-claude-md.md](docs/writing-a-good-claude-md.md)
- Production setup: `docs/production.md`
- UI design system + asset formats: `docs/design-system.md`
- Intent docs: `docs/plans/`
- TDD execution plans: `docs/superpowers/plans/`
- Specs: `docs/superpowers/specs/`
