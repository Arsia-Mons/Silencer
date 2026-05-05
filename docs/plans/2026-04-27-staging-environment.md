# Staging Environment

**Status:** Approved
**Date:** 2026-04-27 (revised 2026-05-03)

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

Single `t4g.small` ARM64 box (`silencer-staging`) running every
component on one host. **Same deployment shape as prod** — admin-api
and admin-web run as `docker run --network host` from GHCR, lobby runs
as a native systemd unit. The only structural simplifications versus
prod are: one box instead of two, no separate stateful EBS volumes, no
DLM snapshots, no Cloudflare Tunnel.

| Component | Unit | Notes |
|---|---|---|
| `silencer-lobby` (Go) | systemd | Same binary as prod; spawns `silencer -s` per game |
| `mongod` | apt | `cacheSizeGB: 0.25`, `bindIp: 127.0.0.1` (localhost only) |
| `lavinmq` | apt | localhost-only |
| `silencer-admin-api` | systemd (`docker run`) | Same GHCR image flow as prod |
| `silencer-admin-web` | systemd (`docker run`) | Same GHCR image flow as prod |
| `tailscaled` | apt | SSH for humans + GH Actions runner; only path to admin-web :24000 |

Mongo + LavinMQ data dirs live on the root volume — wiped on instance
replacement, which is the intended recovery path.

**Sizing rationale.** Prod's per-component RSS analysis (in
`docs/plans/2026-04-25-production-deployment-architecture.md`) sums to
~1.7 GB across the two prod boxes; collapsing both onto one staging box
plus the lobby + dedicated subprocess lands ~1.0 GB resident, fitting
comfortably on `t4g.small` (2 GB) but not `t4g.micro` (1 GB).

An EIP is **required** (not optional). The lobby's `-public-addr`
flag, which it hands clients during the dedicated-server handoff, must
be a dotted-decimal IP because the C++ join path uses `inet_addr()`
not `getaddrinfo()` (`deploy.yml:222-227` documents the same
constraint for prod). Without a stable IP, every instance replacement
would re-bake every client binary built against staging.

Estimated monthly cost: **~$18.30**
(t4g.small $13.40 + ~16 GB gp3 root $1.30 + EIP $3.60, no snapshots,
no second box).

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

1. **Build lobby** (Go) and **dedicated server** (C++ / ARM64) — same
   as `deploy.yml`, but with `LOBBY_HOST=${{ vars.STAGING_LOBBY_HOST }}`
   baked into the dedicated-server cmake invocation. SDL3 .so bundling
   via `patchelf --set-rpath` is identical to prod.
2. **Build + push admin-api** image to GHCR as
   `ghcr.io/.../silencer-admin-api:staging-<sha>` (separate tag from
   prod's `<sha>` so the registry stays untangled).
3. **Build + push admin-web** image to GHCR as
   `ghcr.io/.../silencer-admin-web:staging-<sha>`.
4. **Deploy.** Tailscale up, SSH to `silencer-staging`, scp lobby +
   dedicated server + assets, write `/etc/silencer/admin-api.image` and
   `/etc/silencer/admin-web.image` with the new tags, restart all three
   units, smoke-test.
5. Prune to last 3 release directories on the box.

No macOS / Windows client build — staging doesn't ship to end users.
Tag-driven `release.yml` + `deploy.yml` remain prod's release path,
unchanged.

**EIP plumbing.** The staging lobby's `-public-addr` is sourced from
`vars.STAGING_LOBBY_PUBLIC_IP` (a GitHub repo variable populated from
the `staging_lobby_ip` Terraform output after first apply). Prod
hardcodes its EIP in `deploy.yml:227`'s systemd drop-in; staging keeps
the value out of YAML so a different account / region can use the same
workflow.

Total runtime estimate: ~6–9 min per deploy (admin-api/web Docker
builds dominate).

## Terraform

Implemented as a **separate module** at `infra/terraform/staging/` —
not a workspace + flag on the existing module. Two reasons:

- The cloud-init template is meaningfully different (single box, all
  services local, env vars point at `127.0.0.1` not internal DNS, no
  Cloudflare Tunnel, no separate EBS mounts). Squeezing this into the
  prod template via conditional logic would make every future prod
  edit reason about staging side-effects.
- Prod has `prevent_destroy = true` on the Mongo and LavinMQ EBS
  volumes; flipping `single_box = true` on the prod state would make
  apply refuse. Separate modules → zero shared state → zero
  cross-contamination risk.

What the staging module contains:

- `main.tf` — provider + VPC/subnet/AMI data sources (mirrors prod) +
  one EC2 + EIP + SG + Route 53 A record (`staging.<domain>`, if
  zone configured).
- `iam.tf` — single instance role with read access to
  `/silencer-staging/*` SSM params.
- `ssm.tf` — apply-time data sources for one-shot values
  (Tailscale auth key, deploy SSH pubkey, GHCR pull token).
- `cloud-init-staging.yaml.tftpl` — bootstraps Mongo, LavinMQ, Docker,
  the two admin systemd units, the lobby unit, and Tailscale.
- `outputs.tf` — `staging_lobby_ip` (the EIP that goes into
  `vars.STAGING_LOBBY_PUBLIC_IP`).
- `backend.tf` + `backend.hcl.example` — its own S3 state key
  (`staging.tfstate` in the same bucket bootstrapped for prod).

Secrets namespaced under `/silencer-staging/*` in SSM. SSH pubkeys
(`/silencer/shared/{ssh_admin,deploy_ssh}_pubkey`) and GHCR pull token
(`/silencer/admin/ghcr_pull_token`) are reused; everything else
(Mongo/LavinMQ passwords, JWT secret, Tailscale auth key) is staging-
specific and seeded by `seed-ssm.sh`.

## Data lifecycle

Staging starts fresh on every instance replacement. `mongod`, `lavinmq`,
`lobby.json`, and the `/var/lib/silencer/assets/` tree all live on the
root volume. Default admin seed (`admin/admin`) is recreated each
rebuild. No backups, no `BACKUP_CRON`.

## Developer access

### Game client

`LOBBY_HOST` is baked into the client binary at build time
(`clients/silencer` cmake variable `SILENCER_LOBBY_HOST`), so a stock
prod client cannot reach staging.

```sh
cd clients/silencer
cmake -B build-staging -DSILENCER_LOBBY_HOST=staging.<domain>
cmake --build build-staging -j
```

The `staging.<domain>` form resolves to the EIP via Route 53. The
dedicated-server handoff still flows through the EIP directly (per the
`inet_addr()` constraint in Architecture above), so DNS only matters
for the lobby connection itself.

### Admin dashboard

admin-web binds `:24000` and admin-api binds `:24080`, both bound to
`0.0.0.0`. Neither is reachable from the public internet — the SG only
opens 22/517/30000-61000 — but both are reachable from anyone on the
tailnet. Devs hit `http://<staging-tailscale-name>:24000` for the
dashboard.

This is plain HTTP. Auth flows that depend on `Secure`-only cookies
will not work on staging; that's acceptable for a smoke-test
environment, and matches how dev loops already work.

## Required checks / branch protection

`deploy-staging.yml` is **not** added to required status checks. The
existing five (`build-macos`, `build-windows`, `build-admin-api`,
`build-admin-web`, `build-lobby-docker`) remain the merge gates.

## Resolved questions

- **`lobby_version_string` for staging?** No. The staging EIP isn't
  published anywhere a prod client would discover (no DNS bootstrap
  pointing at it, no `update.json` referencing it). The lockout adds
  rebuild churn for negligible safety gain.
