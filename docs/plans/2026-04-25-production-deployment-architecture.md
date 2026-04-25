# Production Deployment Architecture

**Status:** Proposed
**Date:** 2026-04-25

> **Depends on:** Monorepo restructure
> ([2026-04-25-monorepo-restructure.md](./2026-04-25-monorepo-restructure.md)).
> Path references in this plan use the post-restructure layout
> (`services/lobby/`, `services/admin-api/`, `web/admin/`). If this
> plan is implemented before the restructure lands, mentally translate
> back to today's paths.

## Goal

Define how Silencer's services land in production: the lobby,
MongoDB, RabbitMQ, the admin API, and the admin web app deployed to
AWS via Terraform and GitHub Actions, with each service on its own
deploy lifecycle.

The lobby's existing production deploy keeps its shape (binary
build → scp → symlink swap → restart) and gains data-tier
connection settings via cloud-init. The four other services move
into production for the first time on the same
per-component-deploy footing.

## Principle

Each of the four components has its own deployment lifecycle:

1. **Lobby** — game-critical, deploys per gameplay PR
2. **Admin API** — deploys per dashboard feature
3. **Admin Web** — deploys per UI change
4. **Data tier** (MongoDB + RabbitMQ) — provisioned once, upgraded rarely

Day-to-day deploys must be able to update one component without
touching the others. A bad admin-web rebuild must not be able to
take gameplay down. A Mongo upgrade must not require a lobby
restart. Each component's "deploy" verb is independently invocable
from GitHub Actions.

A standalone provisioning workflow orchestrates the per-service
workflows when bootstrapping a fresh environment. Day-to-day
deploys run one workflow per component.

## Topology

Two AWS EC2 instances in the same region, VPC, subnet, and AZ:

- **Lobby box** — existing instance. Runs the Go lobby as a
  systemd service. GitHub Actions builds the ARM64 binary, scps
  it in over Tailscale, swaps a symlink, restarts the unit.

- **Admin/data box** — new instance. Runs four independent
  systemd units:
  - `mongod` (apt-installed, data on dedicated EBS)
  - `rabbitmq-server` (apt-installed, data on dedicated EBS)
  - `silencer-admin-api` (container, image pulled from GHCR)
  - `silencer-admin-web` (container, image pulled from GHCR)

Box-to-box traffic uses VPC private IPs, gated by security groups
that reference the peer instance's SG as source. SSH to either
box is reached over Tailscale by GitHub Actions; no SSH port is
open to the public internet. Same-AZ placement keeps box-to-box
latency in single-digit milliseconds and avoids inter-AZ data
transfer charges.

The split exists because Mongo and RabbitMQ are stateful
infrastructure with rare upgrade cadences, while admin-api and
admin-web are stateless apps with frequent deploys. Co-locating
them on one host keeps cost low (~$5/mo extra); separating them
from the lobby keeps the gameplay path's blast radius small.

## Deployment Model Per Component

| Component   | Artifact                  | Deploy verb                                    | Trigger                        |
| ----------- | ------------------------- | ---------------------------------------------- | ------------------------------ |
| Lobby       | ARM64 binary              | scp + symlink swap + `systemctl restart`       | Git tag `v*`                   |
| Admin API   | OCI image on GHCR         | `docker pull` + symlink swap + `systemctl restart` | Path filter on its source dir  |
| Admin Web   | OCI image on GHCR         | `docker pull` + symlink swap + `systemctl restart` | Path filter on its source dir  |
| Data tier   | Terraform-managed EC2 + EBS | `terraform apply`                            | Path filter on `infra/terraform/` |

Each lives in its own GitHub Actions workflow file. None imports
or calls another. A separate "provision-environment" workflow
exists for fresh-environment bootstrap and calls the per-service
workflows in dependency order; this is exercised quarterly, not
per PR.

The lobby's tag-based trigger is deliberate: clients pin to a
specific lobby protocol version, so a lobby ship is a coordinated
event. Admin-api and admin-web have no external pinners — nothing
breaks if they ship continuously — so they trigger on path filter.

New helper scripts these workflows call (deploy steps, health
checks, secret-fetch helpers) live in `infra/scripts/` alongside
the existing `install-linux-server.sh` and `fastdeploy.sh`. Per
the repo-wide Bun+TS rule, any new JS-based helpers are `.ts`
files invoked under `bun`.

## Data Persistence

Every stateful service's data dir lives on a dedicated EBS volume,
attached to the instance but managed as an independent Terraform
resource with `prevent_destroy`. Tainting or replacing the
instance leaves the volume untouched; the new instance attaches
the existing volume on first boot.

The lobby already does this for `lobby.json` on its own EBS
volume — the admin/data box replicates the pattern, one volume
per stateful service:

- `/var/lib/mongodb` on its own EBS volume
- `/var/lib/rabbitmq` on its own EBS volume

Cloud-init must mount these volumes **before** apt-installing
MongoDB or RabbitMQ. If the install runs first, the package
postinst writes its initial state into the root volume; the later
mount hides it and the service won't start. Mount → install →
chown is the correct order.

Three independent backup layers stack on top:

1. **AWS Data Lifecycle Manager** snapshots both EBS volumes on a
   daily schedule, retains a rolling window.
2. **MongoDB → GitHub Actions backup** (already designed by
   kristiandelay): admin-api dumps Mongo every 6h and commits to
   a separate backup repo. Git history is rollback history.
3. **The lobby's `lobby.json`** remains the source of truth for
   account data; MongoDB is a read mirror. A total data-tier
   loss does not lose account state.

Data is lost only if all three layers and `lobby.json` fail
simultaneously.

## Cross-Service Configuration

A private Route 53 hosted zone (`silencer.internal`) holds A
records for each box (`lobby.silencer.internal`,
`admin.silencer.internal`), keeping cross-service hostnames stable
across instance replacement.

Each service reads its peers from Terraform-templated
`/etc/silencer/<service>.env` files (mode 0600):

- Lobby: `MONGO_URL` and `RABBITMQ_URL` pointing at
  `admin.silencer.internal`
- Admin-api: `MONGO_URL` at localhost,
  `LOBBY_PLAYER_AUTH_URL` at `lobby.silencer.internal`
- Admin-web: `NEXT_PUBLIC_API_URL` (see Open Decisions for the
  externally-reachable form)

`mongod` and `rabbitmq-server` configure through their distro
locations (`/etc/mongod.conf`, `/etc/rabbitmq/`). Bind addresses
are set to the instance's primary private IP. Application
credentials — mongod SCRAM users, RabbitMQ users, the admin-api's
JWT signing key, the GitHub backup-repo PAT — are managed by
Terraform variables and provisioned by cloud-init from values in
`/etc/silencer/*.env`. Rotation is a Terraform variable change
+ apply.

Security groups gate access at the network layer; service-level
credentials gate access at the application layer. Box-to-box
ingress rules:

- Lobby SG: 15171/tcp from admin/data SG (admin-api → lobby
  playerauth)
- Admin/data SG: 27017/tcp from lobby SG (lobby → MongoDB),
  5672/tcp from lobby SG (lobby → RabbitMQ)

## Implementation Phases

Phases are sequenced by risk: data tier first (slowest to fix
if wrong, longest-lived), then per-service deploy plumbing, then
wiring it all up.

Each phase's PR also refreshes the matching section of
`docs/production.md` so the operational reference stays current
with the deploy verbs as they land.

### Phase 1 — Provision the admin/data box

Terraform delta only. New EC2 instance in the lobby's VPC and AZ,
two EBS volumes, a security group with ingress rules sourced from
the lobby SG (27017/tcp, 5672/tcp), the private Route 53 hosted
zone with A records for both boxes, Tailscale enrollment for
GitHub Actions SSH access, and cloud-init that installs Mongo +
RabbitMQ bound to the private interface with auth enabled. GitHub
Actions does not yet deploy app workloads to this box.

Includes an `infra/terraform/CLAUDE.md` update describing the new
admin/data box module, the EBS-volume-per-stateful-service
pattern, and the cloud-init mount-before-install ordering gotcha.

Done when: SSH from a tailnet peer works, `mongod` and
`rabbitmq-server` accept authenticated connections from the
lobby's private IP, and `terraform taint && apply` the instance
preserves data on both EBS volumes.

### Phase 2 — Admin API deploy workflow

GitHub Actions workflow that builds an ARM64 OCI image, pushes to
GHCR, and updates the systemd unit on the admin/data box via SSH.
Workflow is path-filtered to the admin-api source directory. Image
base is `oven/bun` (per the repo-wide Bun+TS rule); the Dockerfile
contains no `node` invocation. The systemd unit's `ExecStart` runs
`docker run --rm --name silencer-admin-api …` against the
currently-symlinked image tag, so a deploy is `docker pull` +
symlink swap + `systemctl restart silencer-admin-api`.

Includes a `services/admin-api/CLAUDE.md` update covering the
container build, the systemd unit shape, and the deploy verb (per
the monorepo restructure's per-component CLAUDE.md rule).

Done when: a touch-only commit under that directory triggers a
new image, the box pulls it, and the unit restarts without
affecting any other service.

### Phase 3 — Admin Web deploy workflow

Same shape as Phase 2 for the Next.js app, also `oven/bun`-based
(Next.js runs fine under Bun). Path-filtered to the admin-web
source directory. Includes a `web/admin/CLAUDE.md` update mirroring
Phase 2's documentation deliverable.

Done when: same as Phase 2 for the web service.

### Phase 4 — Wire lobby ↔ data tier

Update the lobby's cloud-init to pass `MONGO_URL` and `RABBITMQ_URL`
pointing at `admin.silencer.internal` with the lobby's service
credentials. Open 15171/tcp on the lobby's SG to the admin/data
box's SG and set the admin-api's `LOBBY_PLAYER_AUTH_URL` to
`lobby.silencer.internal`. Requires lobby instance replacement
(cloud-init runs once).

Done when: lobby logs show successful Mongo sync on player
mutations and successful RabbitMQ publishes on match end, and
admin-api validates a player credential against the lobby over
the private network.

### Phase 5 — Snapshot policy

Add the DLM lifecycle policy that snapshots both EBS volumes on
the admin/data box daily, with a rolling retention window.
Independent of any ingress decision, so it can ship as soon as
Phase 1 is in.

Done when: a fresh DLM-created snapshot exists for each volume,
and the retention window prunes correctly after one cycle.

### Phase 6 — Public ingress for admin-web and admin-api

Resolve the public-ingress Open Decision (Tailscale-only,
Cloudflare, ALB+ACM, or admin-web-proxies-admin-api) and
implement the chosen option. Until this lands, both services are
reachable only over the tailnet.

Done when: the chosen URL pattern resolves end-to-end from a
non-developer browser (or, for the Tailscale-only path, the
decision is recorded and the tailnet ACL grants the intended
audience).

## Open Decisions

Each of these is owned by a specific phase, not a Phase 1
prerequisite. Flagged here so they're visible early and don't
get rediscovered mid-implementation:

- **Public ingress for admin-web and admin-api.** Both need
  externally-reachable URLs because `NEXT_PUBLIC_API_URL` bakes
  the API origin into the browser bundle, so the browser — not
  admin-web — calls admin-api directly. Three options for the
  pair, in increasing cost/complexity: (a) Tailscale-only — only
  developers on the tailnet reach either; (b) Cloudflare in front
  of public SG rules on both ports — free TLS, hides the origin
  IP; (c) ALB + ACM with two listener rules — AWS-native,
  ~$18/mo, cleanest separation. Default through Phases 2–5 is
  (a); the choice is the substance of Phase 6. A fourth path is
  to make admin-web proxy API calls server-side and drop
  `NEXT_PUBLIC_API_URL`, leaving only admin-web to expose.

- **Admin-API ↔ admin-web separation.** Phase 2/3 colocate them
  on the same box. If admin-web traffic ever competes with
  admin-api for resources, they can split onto separate boxes
  with no code change — the only coupling is the
  `NEXT_PUBLIC_API_URL` env var pointing at the API's hostname.
  Decision deferred until observed contention.

- **Lobby version-string handshake.** The current lobby's
  `-version` flag default needs to track the client constant.
  This is unrelated to the admin tier but the version-bump
  workflow should be revisited in the same window since both
  flow through cloud-init. Possibly factor out.

## Out of Scope

- Multi-region or HA topology. Single AZ, single box per role.
- Migrating account storage off `lobby.json`. The Go lobby's
  flat-file store remains primary; MongoDB stays a read mirror.
- Horizontal scaling of admin-api or admin-web. Single instance
  of each is sufficient at hobby scale.
- A separate staging environment. Production is the only
  environment until traffic justifies a second.
- Observability stack (Prometheus, Loki, etc.). Adding a
  consumer for the existing RabbitMQ event stream is a
  follow-up, not part of this work.

## Success Criteria

The plan is done when all of the following hold simultaneously:

1. Re-deploying the lobby does not touch the admin/data box.
2. Re-deploying admin-api does not restart admin-web, Mongo,
   RabbitMQ, or the lobby.
3. Re-deploying admin-web does not restart admin-api, Mongo,
   RabbitMQ, or the lobby.
4. `terraform taint aws_instance.<admin>` followed by
   `terraform apply` rebuilds the admin/data instance with zero
   loss of MongoDB or RabbitMQ state.
5. The lobby's MongoDB mirror and RabbitMQ event stream are
   live in production — no `[mongosync] connect failed` or
   `[events] amqp connect failed` log lines under steady state.
6. Each of the four components is deployable from GitHub Actions
   with no manual SSH steps.
