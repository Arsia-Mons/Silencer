# .github/workflows/ — GitHub Actions

Nine CI builds (five required by branch protection on `main`;
`build-linux` and the three `build-launcher-*` are optional until
added), three deploys, one release.
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
| `ci-build-launcher-{macos,windows,linux}.yml` | every PR + push to `main` | `clients/launcher/**`, `clients/lobby-sdk/cpp/**`, the **reused** `clients/silencer/src/{ui,render/cppx_ui,client/ui,updater}/**` subtrees, `shared/{fonts,assets,icons}/**`, or the matching action/workflow |

The three launcher builds are **not required checks** — add them via
branch protection if you want merges gated on them. They exist because
`clients/launcher/CMakeLists.txt` compiles those `clients/silencer/`
subtrees **by absolute path**, so a change there can break the launcher
while every game check stays green. Nothing else catches that. They run
the same composite actions `release.yml` does, so a CI pass means the
release build works.

`ci-build-launcher-macos.yml` runs on `macos-latest`, not the `macos-15`
`release.yml` pins. Deliberate: the newer dyld is the one that
hard-aborts on a duplicate `LC_RPATH`, so CI is where
`package-macos.sh`'s dedupe gets proven.

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

## Release

| Workflow | Triggers (`on:`) |
|---|---|
| `release.yml` | push of `v*` tag, nightly cron (07:00 UTC), or manual dispatch |

### The `version` job — two version strings, not one

Every build job takes its versions from the single `version` job. It
emits **two** strings, and conflating them is the trap it exists to
prevent:

- **`protocol`** — the wire protocol number. The lobby compares it
  against every connecting client (`services/lobby/client.go:157`). A
  nightly **never** invents a new one: the moment the lobby redeployed
  at it, every stable client would be rejected. On a non-tag ref it
  comes from **the newest `v*` tag**.

  > Not from `clients/silencer/CMakeLists.txt`. That number is not what
  > production speaks — `release.yml` overrides the compiled-in value
  > with the tag, and `deploy.yml` ships the lobby at `"${TAG#v}"`
  > (`deploy.yml:181`). The tree has said `00058` since v00058 while
  > `v00062` is live. Building a nightly off the file produces a client
  > the production lobby rejects on sight, which is the same lockout
  > this split exists to prevent, arriving from the other direction.
  > The file is only a fallback for a repo with no release tags at all.
- **`build_id`** — the build's own identity
  (`00058+nightly.20260805.a1b2c3d`), and the only thing self-update
  compares. On a tag the two are equal.

The number stays out of `build_id` because
`CFBundleShortVersionString` and the Windows VERSIONINFO quad both have
to parse as a number — `clients/launcher/CMakeLists.txt` falls back to
`0.0.0.0` for anything non-numeric. So the launcher takes both:
`launcher-version` (numeric, stamped into the plist and the resource)
and `launcher-build-id` (the identity, a compile define).

Before this job existed each build job ran its own
`SILENCER_VERSION=${GITHUB_REF_NAME#v}`, which on a branch ref yielded
the literal string `main`. That path had never been exercised.

**Nightlies skip when `main` has not moved** in 24h — a scheduled run
with nothing to ship costs three platform builds and a notarization
round-trip. `workflow_dispatch` always builds, so a nightly can be
forced. Nightlies publish to the `latest` prerelease tag; `publish-npm`
stays tag-only.

### `update.json`

The `release` job generates the update manifest from the built
artifacts (per-platform URL + sha256) and uploads it as a release asset.
Nothing generated it before, which is why the lobby's
`-update-manifest` path and the launcher's `manifest_url_*` both
resolved to a 404 in production. Its shape is
`services/lobby/update.go`'s `manifestFile` plus `build_id` and
`channel`, which only the launcher reads; Go ignores the extra fields.

`release.yml` jobs: a `version` job, then six parallel builds — `build-macos`,
`build-windows`, `build-linux` for the game and
`build-launcher-macos`, `build-launcher-windows`,
`build-launcher-linux` for the launcher
→ `release` (creates the GitHub Release) → `publish-npm`
(stages and publishes the five npm packages described in
`clients/tui/CLAUDE.md`).

`build-macos` and `build-windows` each run an **auto-updater e2e** step
(`infra/scripts/test-updater.{sh,ps1}`) after the build and before
signing/upload: it builds a second `99999`-versioned client, has the
just-built (unsigned) release client self-update to it headlessly, and asserts
the new version relaunches — gating the release on a working self-updater
(issue #303). It runs on a scratch copy so the shipped artifact is untouched,
and needs `oven-sh/setup-bun` (the harness drives the game via `clients/cli`).
`build-linux` has no such step (Linux isn't a shipped self-update platform).

The three `build-launcher-*` jobs build `clients/launcher/` into
`build-launcher/`, never `build/`, so a launcher job and a game job
cannot collide on artifacts.

- `build-launcher-macos` gates on its own self-update e2e
  (`infra/scripts/test-launcher-updater.sh`, before signing, on a
  scratch copy) and then runs the same sign → notarize → staple →
  `create-dmg` → sign/notarize/staple-the-DMG sequence as
  `build-macos`, on the same Apple secrets. It also signs the
  launcher's nested `Contents/Helpers/updater-stage-2`. arm64 only, on
  purpose (`clients/launcher/CLAUDE.md` has the reasoning).
- `build-launcher-windows` uses its **own** vcpkg cache path and key —
  `clients/launcher/vcpkg.json` is a different dependency set from the
  game's, so sharing `build-silencer-windows`'s cache would thrash it.
  Produces a portable zip and an Inno Setup installer.
- `build-launcher-linux` bundles SDL3 with `patchelf --set-rpath
  '$ORIGIN'` and tars it.

Each launcher release job has a PR CI counterpart
(`ci-build-launcher-*.yml`) running the same composite action, so the
build path is exercised on every relevant PR. The signing, notarization
and DMG steps are still tag-only — CI has no Apple secrets.

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
