# Decoupled Admin/Data Tier Deployment

**Status:** Proposed
**Date:** 2026-04-25

> **Depends on:** Monorepo restructure
> ([2026-04-25-monorepo-restructure.md](./2026-04-25-monorepo-restructure.md)).
> Path references in this plan use the post-restructure layout
> (`services/lobby/`, `services/admin-api/`, `web/admin/`). If this
> plan is implemented before the restructure lands, mentally translate
> back to today's paths.

## Goal

Stand up MongoDB, RabbitMQ, the admin API, and the admin web app on
AWS infrastructure managed entirely by Terraform + GitHub Actions —
without coupling their deploys to the lobby or to each other.

Today the admin/data services exist only in `docker-compose.yml`,
which doesn't run anywhere in production. The lobby silently no-ops
its MongoDB sync and RabbitMQ publishes because neither service is
reachable. This plan closes that gap **without** putting the four
services behind a single Compose stack in production.

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

An umbrella workflow that *orchestrates* full-environment
provisioning (e.g. spinning up a fresh staging environment) is
fine. An umbrella workflow that *fuses* their day-to-day deploys
into one job is the antipattern this plan exists to prevent.

## Topology

Two AWS EC2 instances on the same Tailscale tailnet:

- **Lobby box** — existing instance. Runs the Go lobby as a
  systemd service. Deploy model unchanged: GitHub Actions builds
  the ARM64 binary, scps it over Tailscale, swaps a symlink,
  restarts the unit.

- **Admin/data box** — new instance. Runs four independent
  systemd units:
  - `mongod` (apt-installed, data on dedicated EBS)
  - `rabbitmq-server` (apt-installed, data on dedicated EBS)
  - `silencer-admin-api` (container, image pulled from GHCR)
  - `silencer-admin-web` (container, image pulled from GHCR)

  Mongo and RabbitMQ bind to localhost; the box's security group
  exposes only SSH. All cross-service traffic — lobby ↔ admin
  box, GitHub Actions ↔ either box — flows over Tailscale.

The split exists because Mongo and RabbitMQ are stateful
infrastructure with rare upgrade cadences, while admin-api and
admin-web are stateless apps with frequent deploys. Co-locating
them on one host keeps cost low (~$5/mo extra); separating them
from the lobby keeps the gameplay path's blast radius small.

Reasons we are explicitly **not** using:

- **Compose in production** — its unit of operation is the whole
  stack, which violates the deployment-independence principle.
  Compose remains the local-dev integration tool.
- **DocumentDB / Amazon MQ** — managed-service pricing is
  ~10–50× higher than self-hosted at hobby scale, with no real
  reliability gain on a single-region single-box deployment.
- **External managed services** (Atlas, CloudAMQP) — out of scope
  per the AWS-only constraint.
- **Kubernetes / Nomad** — orchestration overhead exceeds the
  problem at this scale.

## Deployment Model Per Component

| Component   | Artifact                  | Deploy verb                                    | Trigger                        |
| ----------- | ------------------------- | ---------------------------------------------- | ------------------------------ |
| Lobby       | ARM64 binary              | scp + symlink swap + `systemctl restart`       | Git tag `v*`                   |
| Admin API   | OCI image on GHCR         | `docker pull` + symlink swap + `systemctl restart` | Path filter on its source dir  |
| Admin Web   | OCI image on GHCR         | `docker pull` + symlink swap + `systemctl restart` | Path filter on its source dir  |
| Data tier   | Terraform-managed EC2 + EBS | `terraform apply`                            | Path filter on `terraform/`    |

Each lives in its own GitHub Actions workflow file. None imports
or calls another. A separate "provision-environment" workflow
exists for fresh-environment bootstrap and calls the per-service
workflows in dependency order; this is exercised quarterly, not
per PR.

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

Ban data loss only if all three layers and `lobby.json` fail
simultaneously.

## Cross-Service Configuration

Hostnames between boxes resolve via Tailscale MagicDNS. Both
sides hardcode the tailnet hostname in their systemd unit
EnvironmentFile. No service discovery, no Consul, no
load balancers — the tailnet is the network.

Secrets (JWT signing key, GitHub backup-repo PAT, RabbitMQ
admin password) are written once by Terraform into a
`/etc/silencer/*.env` file with mode 0600 and sourced by the
relevant systemd unit. Rotation is a Terraform variable change
+ apply, not a deploy.

## Implementation Phases

Phases are sequenced by risk: data tier first (slowest to fix
if wrong, longest-lived), then per-service deploy plumbing, then
wiring it all up.

### Phase 1 — Provision the admin/data box

Terraform delta only. New EC2 instance, two EBS volumes, security
group, Tailscale enrollment, cloud-init that installs and binds
Mongo + RabbitMQ to localhost. GitHub Actions does not yet deploy
to this box.

Done when: SSH to the new box over Tailscale works, `mongod` and
`rabbitmq-server` are running, and `terraform taint && apply`
the instance preserves data on both EBS volumes.

### Phase 2 — Admin API deploy workflow

GitHub Actions workflow that builds an ARM64 OCI image, pushes to
GHCR, and updates the systemd unit on the admin/data box via SSH.
Workflow is path-filtered to the admin-api source directory.

Done when: a touch-only commit under that directory triggers a
new image, the box pulls it, and the unit restarts without
affecting any other service.

### Phase 3 — Admin Web deploy workflow

Same shape as Phase 2 for the Next.js app. Path-filtered to the
admin-web source directory.

Done when: same as Phase 2 for the web service.

### Phase 4 — Wire lobby to the data tier

Update the lobby's cloud-init to pass `MONGO_URL` and the
RabbitMQ URL pointing at the admin/data box's tailnet hostname.
Requires lobby instance replacement (cloud-init runs once).

Done when: lobby logs show successful Mongo sync on player
mutations and successful RabbitMQ publishes on match end.

### Phase 5 — Snapshot policy + access decision

Add the DLM lifecycle policy. Decide and implement public
ingress for admin-web (see Open Decisions).

## Open Decisions

These need answers before Phase 1 lands; flagging now so they
don't block later phases:

- **Public ingress for admin-web.** Three options, in increasing
  cost/complexity: (a) Tailscale-only — only developers reach
  the dashboard from devices on the tailnet; (b) Cloudflare
  proxy in front of a public security-group rule — free TLS,
  hides the origin IP; (c) ALB + ACM — AWS-native, ~$18/mo,
  cleanest separation. Default for Phase 1 is (a). Revisit
  before Phase 5 if non-developers need access.

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

Explicitly **not** in this plan:

- Multi-region or HA topology. Single AZ, single box per role.
- Migrating account storage off `lobby.json`. The Go lobby's
  flat-file store remains primary; MongoDB stays a read mirror.
- Horizontal scaling of admin-api or admin-web. Single instance
  of each is sufficient at hobby scale.
- Replacing GHCR with ECR. GHCR is free, GitHub-native, and
  authenticates via OIDC; ECR introduces an AWS dependency
  and per-region-pull cost with no compensating benefit.
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
