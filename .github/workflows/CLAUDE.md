# .github/workflows/ — GitHub Actions

Five CI builds (required by branch protection on `main`), three
deploys, one release. Path filters for the CI builds live **inside
the job**, not in `on:` — see "Required check trap" below.

## CI builds (required checks on `main`)

Required status check IDs: `build-macos`, `build-windows`,
`build-admin-api`, `build-admin-web`, `build-lobby-docker`.

| Workflow | Triggers (`on:`) | Real work runs when… |
|---|---|---|
| `ci-build-macos.yml` | every PR + push to `main` | change touches anything **outside** the denylist (see below) |
| `ci-build-windows.yml` | every PR + push to `main` | same denylist as macOS |
| `ci-build-admin-api.yml` | every PR + push to `main` | `services/admin-api/**`, root `package.json`, `bun.lock`, or this workflow |
| `ci-build-admin-web.yml` | every PR + push to `main` | `web/admin/**`, `shared/gas-validation/**`, root `package.json`, `bun.lock`, or this workflow |
| `ci-build-lobby-docker.yml` | every PR + push to `main` | `services/lobby/**`, `clients/silencer/**`, `shared/assets/**`, or this workflow |

macOS / Windows denylist (skip the build when **only** these
change): `services/`, `web/`, `infra/`, `docs/`, `designer/`,
`shared/{design,skills}/`, top-level `*.md`, `.gitignore`,
`ci-build-admin-*.yml`, `ci-build-lobby-docker.yml`,
`deploy*.yml`, `release.yml`.

## Deploys

| Workflow | Triggers (`on:`) |
|---|---|
| `deploy.yml` (game client + lobby) | `workflow_run` after a successful `Release` on a `v*` tag, or manual dispatch |
| `deploy-admin-api.yml` | push to `main` touching `services/admin-api/**`, `shared/assets/**`, root `package.json`, `bun.lock`, or this workflow; or manual |
| `deploy-admin-web.yml` | push to `main` touching `web/admin/**`, `shared/gas-validation/**`, root `package.json`, `bun.lock`, or this workflow; or manual |

## Release

| Workflow | Triggers (`on:`) |
|---|---|
| `release.yml` | push of `v*` tag, or manual dispatch |

## Required check trap

Branch protection treats a workflow that's filtered out at `on:`
as "Expected — Waiting for status to be reported" and blocks
merge forever. So the five required CI builds always trigger; a
gate step (`dorny/paths-filter@v3` for allowlists, a `git diff`
shell step for the macOS/Windows denylist) sets
`steps.changes.outputs.relevant`, and every real step is
`if: steps.changes.outputs.relevant == 'true'`. Skipped *steps*
inside a running job still let the job report success — that's
what unblocks merges. Don't move filters back to `on:` without
also dropping the check from branch protection.

Deploys aren't required checks, so they keep `on: paths` filters.
