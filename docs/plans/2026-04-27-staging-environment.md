# Staging Environment

**Status:** Proposed
**Date:** 2026-04-27

## Goal

A staging environment that mirrors the production stack (lobby +
dedicated game servers + admin-api + admin-web + Mongo + LavinMQ) on one
cheap box, redeployed automatically on every push to `main`. Validates
that the full stack still builds, boots, and serves traffic — independent
of tag-driven prod releases.

## Non-goals

- **Not load-tested.** 1 concurrent game is the assumed ceiling.
- **Not durable.** Data is disposable; no backups, no DLM snapshots, no
  separate EBS data volumes. A bad deploy is recovered by `terraform
  taint`, not by restoring state.
- **Not a merge gate.** Staging deploy failures don't block PRs; the
  five existing CI builds remain the only required checks on `main`.
- **Not for end users.** Game clients still point at prod's `LOBBY_HOST`.

## Architecture

Single `t4g.micro` ARM64 box (`silencer-staging`) running every
component as a systemd unit. No containers, no Cloudflare Tunnel, no
separate stateful EBS volumes.

| Component | Unit | Notes |
|---|---|---|
| `silencer-lobby` (Go) | systemd | Same binary as prod; spawns `silencer -s` per game |
| `mongod` | apt | Default `cacheSizeGB: 0.25`, localhost-only |
| `lavinmq` | apt | localhost-only |
| `silencer-admin-api` | systemd (`bun run …`) | Native Bun, no Docker |
| `silencer-admin-web` | systemd (`bun run …`) | Next.js standalone under Bun, no Docker |
| `tailscaled` | apt | SSH for humans + GH Actions runner |

Mongo + LavinMQ data dirs live on the root volume — wiped on instance
replacement, which is the intended recovery path. Realistic peak RAM
with one active game ≈ ~650 MB on ~850 MB usable; no special tuning
required (analysis in `docs/plans/2026-04-25-production-deployment-architecture.md`
§ Sizing decisions, applied at staging traffic levels).

An EIP is **required** (not optional) — see Developer access below.
The lobby's `-public-addr` flag, which it hands clients during the
dedicated-server handoff, must be a dotted-decimal IP because the C++
join path uses `inet_addr()` not `getaddrinfo()` (`deploy.yml:223-225`
documents the constraint). Without a stable IP, every instance
replacement re-bakes every client binary built against staging.

Estimated monthly cost: **~$10.60** (t4g.micro $6 + ~16 GB gp3 root
$1.30 + EIP $3.60, no snapshots, no second box).

## Deploy flow

One new workflow: `.github/workflows/deploy-staging.yml`.

```yaml
on:
  push:
    branches: [main]
  workflow_dispatch:

concurrency:
  group: deploy-staging
  cancel-in-progress: false
```

That single concurrency block satisfies all three requirements:

| Requirement | How it's met |
|---|---|
| Push to `main` triggers a deploy | `on.push.branches: [main]` |
| New commits don't cancel an active deploy | `cancel-in-progress: false` |
| Only the newest pending commit deploys after the active run | Built into GH Actions: when a run is queued behind an in-progress run, any *previously pending* run in the same group is canceled |

Resulting behavior:

- Commit A starts deploying.
- Commit B lands → queued behind A.
- Commit C lands → B is canceled in queue, C is now pending.
- Commit D lands → C is canceled, D is pending.
- A finishes → D starts. B and C are skipped entirely.

No polling, no debounce script, no external queue — declared concurrency
group is the entire mechanism.

### What the workflow does

Mirrors the shape of `deploy.yml` + `deploy-admin-api.yml` +
`deploy-admin-web.yml`, collapsed into one job since staging has all
four artifacts on one box:

1. Build lobby (Go) and dedicated server (C++ / ARM64) — same as
   prod's `deploy.yml`, but with `LOBBY_HOST=staging.<domain>` (or the
   staging IP) baked into the dedicated-server build.
2. Build admin-api and admin-web — `bun install --frozen-lockfile`,
   `bun run build` for the Next.js app, tar the output trees. No
   GHCR push (no Docker on staging).
3. Tailscale up, SSH to `silencer-staging`, scp tarballs into
   `/opt/silencer-staging/releases/<sha>/`, swap `current` symlink,
   `systemctl restart silencer-lobby silencer-admin-api silencer-admin-web`.
4. Prune to last 3 releases.

No macOS / Windows client build — staging doesn't ship to end users.
Tag-driven `release.yml` + `deploy.yml` remain prod's release path,
unchanged.

Total runtime estimate: ~5–8 min per deploy.

## Terraform

Add a `single_box = true` mode to the existing `infra/terraform/`
module that:

- Creates one `t4g.micro` instance instead of the prod two-box pair.
- Skips `dlm.tf` (no snapshots).
- Skips the separate Mongo / LavinMQ EBS volumes (root volume only).
- Skips Cloudflare Tunnel ingress and the `cloudflared` install in
  cloud-init.
- Skips the GitHub-backed Mongo backup wiring.

State separated via `terraform workspace new staging`, same module.

A new `cloud-init-staging.yaml.tftpl` borrows from the prod admin
template but: installs everything on root, runs admin-api / admin-web
as native `bun run` systemd units (not Docker), and puts the lobby on
the same box.

Secrets namespaced under `/silencer-staging/*` in SSM — JWT secret,
Mongo / LavinMQ passwords, Tailscale auth key all distinct from prod.
SSH pubkeys can be shared.

## Data lifecycle

Staging starts fresh on every instance replacement. `mongod`, `lavinmq`,
and `lobby.json` all live on the root volume. Default admin seed
(`admin/admin`) is recreated each rebuild. No backups.

## Developer access

`LOBBY_HOST` is baked into the client binary at build time
(`clients/silencer` cmake variable `SILENCER_LOBBY_HOST`), so a stock
prod client cannot reach staging. Two paths:

**Default — local build.** Devs who already have the toolchain run:

```sh
cd clients/silencer
cmake -B build-staging -DSILENCER_LOBBY_HOST=staging.<domain>
cmake --build build-staging -j
```

The `staging.<domain>` form resolves to the EIP via Route 53. The
dedicated-server handoff still flows through the EIP directly (per the
`inet_addr()` constraint in Architecture above), so DNS only matters
for the lobby connection itself.

**Opt-in — CI-built artifacts.** `deploy-staging.yml` accepts a
`build_clients: true` `workflow_dispatch` input that adds Mac + Windows
build jobs (mirroring `release.yml`'s shape but with
`LOBBY_HOST=staging.<domain>`) and uploads the zips via
`actions/upload-artifact`. Useful when a non-builder needs to playtest
staging — adds ~10–15 min to the run, so it's off by default. macOS
zips are unsigned; users `xattr -d com.apple.quarantine` before
launching.

The day-to-day staging loop (push → "did the lobby boot, do admin
routes still work") is covered by Tailscale-only access to the admin
dashboard plus the deploy workflow's own health checks; rebuilding the
client only matters when validating real protocol or game-logic
changes.

## Required checks / branch protection

`deploy-staging.yml` is **not** added to required status checks. The
existing five (`build-macos`, `build-windows`, `build-admin-api`,
`build-admin-web`, `build-lobby-docker`) remain the merge gates.

## Open questions

1. **`lobby_version_string` for staging?** Setting a distinct value
   (e.g. `"staging-<sha>"`) prevents a prod client from accidentally
   connecting to the staging lobby. Worth doing if the staging lobby
   is publicly reachable.
