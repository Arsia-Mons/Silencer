# .github/workflows/ — GitHub Actions

Six CI builds (five required by branch protection on `main`,
`build-linux` optional until added), four deploys, one release.
Path filters for the CI builds live **inside the job**, not in
`on:` — see "Required check trap" below.

## CI builds (required checks on `main`)

Required status check IDs: `build-macos`, `build-windows`,
`build-admin-api`, `build-admin-web`, `build-lobby-docker`.
`build-linux` (from `ci-build-linux.yml`) follows the same shape
but isn't currently a required check — add it via branch
protection settings if/when you want to gate merges on it.

| Workflow | Triggers (`on:`) | Real work runs when… |
|---|---|---|
| `ci-build-macos.yml` | every PR + push to `main` | change touches anything **outside** the denylist (see below) |
| `ci-build-windows.yml` | every PR + push to `main` | same denylist as macOS |
| `ci-build-linux.yml` | every PR + push to `main` | same denylist as macOS |
| `ci-build-admin-api.yml` | every PR + push to `main` | `services/admin-api/**`, root `package.json`, `bun.lock`, or this workflow |
| `ci-build-admin-web.yml` | every PR + push to `main` | `web/admin/**`, `shared/gas-validation/**`, root `package.json`, `bun.lock`, or this workflow |
| `ci-build-lobby-docker.yml` | every PR + push to `main` | `services/lobby/**`, `clients/silencer/**`, `shared/assets/**`, or this workflow |

macOS / Windows / Linux denylist (skip the build when **only**
these change): `services/`, `web/`, `infra/`, `docs/`, `designer/`,
`shared/{design,skills}/`, top-level `*.md`, `.gitignore`,
`ci-build-admin-*.yml`, `ci-build-lobby-docker.yml`,
`deploy*.yml`, `release.yml`.

## Deploys

| Workflow | Triggers (`on:`) |
|---|---|
| `deploy.yml` (game client + lobby) | `workflow_run` after a successful `Release` on a `v*` tag, or manual dispatch |
| `deploy-admin-api.yml` | push to `main` touching `services/admin-api/**`, `shared/assets/**`, root `package.json`, `bun.lock`, or this workflow; or manual |
| `deploy-admin-web.yml` | push to `main` touching `web/admin/**`, `shared/gas-validation/**`, root `package.json`, `bun.lock`, or this workflow; or manual |
| `deploy-staging.yml` (lobby + dedicated server + admin-api + admin-web → single-box staging) | every push to `main` (no path filter — staging mirrors the full stack); `concurrency.cancel-in-progress: false` coalesces queued runs to the newest commit |

## Release

| Workflow | Triggers (`on:`) |
|---|---|
| `release.yml` | push of `v*` tag, or manual dispatch |

`release.yml` jobs: `build-macos` + `build-windows` + `build-linux`
(parallel) → `release` (creates the GitHub Release) → `publish-npm`
(stages and publishes the five npm packages described in
`clients/tui/CLAUDE.md`).

`publish-npm` requires the `NPM_TOKEN` secret (granular publish
token for the `arsia-mons` scope + the unscoped `silencer-tui`
name) and uses GitHub OIDC for `npm publish --provenance`. It's
gated to `refs/tags/v*`; manual dispatch on a non-tag ref skips it.

Gating note: a `publish-npm` failure marks the whole `Release`
workflow as failed, which gates `deploy.yml`'s `workflow_run`
trigger. If `publish-npm` fails after `release` has already created
the GitHub Release, the engine deploy doesn't auto-fire — manually
re-run `deploy.yml` (or `publish-npm`) after the npm side is fixed.

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
