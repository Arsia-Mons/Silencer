# Production Deployment Architecture

**Status:** Proposed
**Date:** 2026-04-25

> **Built on:** Monorepo restructure
> ([2026-04-25-monorepo-restructure.md](./2026-04-25-monorepo-restructure.md)),
> Phases 1–4 merged. Path references use the post-restructure layout
> (`services/lobby/`, `services/admin-api/`, `web/admin/`).

## Goal

Define how Silencer's services land in production: the lobby,
MongoDB, LavinMQ, the admin API, and the admin web app deployed to
AWS via Terraform and GitHub Actions, with each service on its own
deploy lifecycle.

The lobby's existing production deploy keeps its shape (binary
build → scp → symlink swap → restart) and gains data-tier
connection settings via cloud-init. The new admin-tier workloads
(admin-api, admin-web, plus the data tier they depend on) move
into production for the first time, each on the same
per-component-deploy footing as the lobby — see Principle below
for the four-component grouping.

## Principle

Each of the four components has its own deployment lifecycle:

1. **Lobby** — game-critical, deploys per gameplay PR
2. **Admin API** — deploys per dashboard feature
3. **Admin Web** — deploys per UI change
4. **Data tier** (MongoDB + LavinMQ) — provisioned once, upgraded rarely

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
  - `lavinmq` (apt-installed, data on dedicated EBS) — AMQP 0.9.1
    broker, drop-in for RabbitMQ at the wire-protocol level. Chosen
    over RabbitMQ because its idle/peak RAM is ~5x lower (~40MB vs
    ~200MB+ BEAM idle), and existing `amqp091-go`/`amqplib` clients
    connect with no code changes. See Resource Profile.
  - `silencer-admin-api` (container, image pulled from GHCR)
  - `silencer-admin-web` (container, image pulled from GHCR)

Box-to-box traffic uses VPC private IPs, gated by security groups
that reference the peer instance's SG as source. SSH to either
box is reached over Tailscale by GitHub Actions; no SSH port is
open to the public internet. Same-AZ placement keeps box-to-box
latency in single-digit milliseconds and avoids inter-AZ data
transfer charges.

The split exists because Mongo and LavinMQ are stateful
infrastructure with rare upgrade cadences, while admin-api and
admin-web are stateless apps with frequent deploys. Co-locating
them on one host keeps cost low (~$5/mo extra); separating them
from the lobby keeps the gameplay path's blast radius small.

## Resource Profile

Activity tiers are concrete game-state rather than abstract RPS —
Silencer is a niche multiplayer 2D action game and "scale" should
be calibrated to that, not generic web-app heuristics:

| Tier | Concurrent players | Active games | Lobby events/min | Note |
| ---- | ------------------ | ------------ | ---------------- | ---- |
| Idle   | 0       | 0     | 0     | box up, nothing happening |
| Light  | 5–10    | 1–2   | ~50   | typical evening |
| Active | 30–50   | 5–10  | ~500  | busy weekend |
| Peak   | 100–200 | 20+   | ~2000 | hobby ceiling — Silencer's all-time-high range |

Numbers below mix grounded baselines (distro defaults, daemon
idle RSS, MongoDB cache config) and estimates for application
memory and per-tier growth. Where labeled "estimated", the
number deserves re-checking against live traffic.

### Lobby box

Processes:

| Process | Role |
| ------- | ---- |
| `silencer-lobby` (Go) | TCP lobby, UDP heartbeats, player-auth HTTP on `:15171` (`services/lobby/main.go`) |
| `tailscaled` | Admin-plane SSH only |
| OS + systemd + sshd + journald | base |

RAM profile:

| Process        | Idle    | Light   | Active  | Peak    |
| -------------- | ------- | ------- | ------- | ------- |
| silencer-lobby | 15MB    | 25MB    | 50MB    | 120MB   |
| tailscaled     | 30MB    | 30MB    | 30MB    | 40MB    |
| OS + base      | 150MB   | 150MB   | 150MB   | 150MB   |
| **Total**      | **~195MB** | **~205MB** | **~230MB** | **~310MB** |

CPU peaks well under 10% of 1 vCPU. Disk: ~3GB OS root +
`lobby.json` (~1KB per player) + uploaded maps directory.

**Sizing:** t4g.nano (512MB) is sufficient at peak. The current
instance is over-provisioned relative to the lobby's actual
footprint; no action needed.

### Admin/data box

Processes:

| Process | Role |
| ------- | ---- |
| `mongod` | Player/Session/MatchStat/Event collections (apt install, `/etc/mongod.conf`) |
| `lavinmq` | `silencer.events` topic exchange + `admin-dashboard` durable queue (AMQP 0.9.1, Crystal runtime, disk-first message store) |
| `silencer-admin-api` (container) | AMQP consumer → Mongo upserts → WS push to dashboard (`services/admin-api/`) |
| `silencer-admin-web` (container) | Next.js dashboard (`web/admin/`) |
| `dockerd` + `containerd` | Container runtime |
| `tailscaled` | Admin-plane SSH only |
| OS + systemd + sshd + journald | base |

RAM profile:

| Process                            | Idle   | Light   | Active  | Peak    | Source |
| ---------------------------------- | ------ | ------- | ------- | ------- | ------ |
| OS + base                          | 150MB  | 150MB   | 150MB   | 150MB   | grounded |
| tailscaled                         | 30MB   | 30MB    | 30MB    | 40MB    | grounded |
| dockerd + containerd               | 100MB  | 100MB   | 100MB   | 110MB   | grounded |
| mongod (with `cacheSizeGB: 0.25`)  | 250MB  | 400MB   | 600MB   | 800MB   | cache floor grounded; growth estimated |
| lavinmq                            | 40MB   | 50MB    | 60MB    | 80MB    | CloudAMQP-published numbers: 8K conns ≈ 400MB, 10M enqueued msgs ≈ 80MB; our workload is 2-3 conns and a near-empty queue |
| admin-api container                | 60MB   | 100MB   | 150MB   | 220MB   | estimated |
| admin-web container                | 100MB  | 150MB   | 200MB   | 300MB   | estimated |
| **Total**                          | **~730MB** | **~980MB** | **~1.29GB** | **~1.70GB** | |

CPU is not the binding constraint at any tier on 2 vCPU.
Estimates: <2% idle, ~3–5% light, ~10–20% active, ~30–50% peak.
Hot paths under load are `admin-api` (per-event upserts + WS
fanout) and `mongod` index touches. LavinMQ's CPU is negligible
at our event rate (peak workload is ~33 events/sec; LavinMQ
benchmarks at 578K msgs/sec on a t4g.micro).

Disk (per the plan's three EBS volumes):

| Volume | Initial | +1 month at Active | +1 year at Active | Recommended size |
| ------ | ------- | ------------------ | ----------------- | ---------------- |
| Root (OS + docker engine + container images + binaries + logs) | ~5.5GB | ~6.5GB | ~10GB | **16GB gp3** |
| `/var/lib/mongodb` | ~100MB | ~500MB | ~3–5GB | **10GB gp3** |
| `/var/lib/lavinmq` | ~50MB | ~100MB | ~500MB | **5GB gp3** |

Total ~31GB gp3 ≈ $2.50/month storage. Dominant growth driver
is the `Event` collection (audit log of every AMQP message);
a TTL index on `Event.ts` caps it without code changes.
LavinMQ's disk-first message store pages out to `/var/lib/lavinmq`
under load — sized at 5GB to absorb a multi-day admin-api outage
backlog without filling the volume.

### Sizing decisions (Phase 1)

- **t4g.micro (1GB)** is a non-starter for the Phase 1 stack —
  Mongo's 250-800MB footprint plus admin-api/admin-web's current
  Node-based runtimes leave no room.
- **t4g.small (2GB, ~$12/mo)** survives all tiers including
  Peak (~1.7GB usage on ~1.85GB usable). Headroom at Peak is
  ~150MB. **Recommended default for Phase 1.** Defensible
  because LavinMQ's RAM savings vs RabbitMQ already gave us this
  margin; the conservative t4g.medium would have been required
  with RabbitMQ.
- **t4g.medium (4GB, ~$24/mo)** would give ~57% headroom but
  costs $12/mo more for safety we don't need at this stack's
  measured profile.

Future re-sizing is cheap: stop the instance, change the type
in Terraform, `apply`. EBS volumes for Mongo and LavinMQ detach
and reattach untouched (per Data Persistence).

### Sizing projection (Phase 2)

Phase 2 swaps Mongo → Postgres (~700MB peak savings) and rewrites
admin-api on Bun + Hono with native WebSocket (~140MB peak
savings vs the current Node + Express + socket.io + mongoose
stack). Admin-web stays on Next.js standalone under Bun with an
explicit V8 heap cap. Projected admin/data box profile:

| Process | Idle | Peak | Source |
| ---------------------------------- | ------ | ------- | ------ |
| OS + base                          | 150MB  | 150MB   | grounded |
| tailscaled                         | 30MB   | 40MB    | grounded |
| dockerd + containerd               | 100MB  | 110MB   | grounded |
| postgres (tuned tiny: shared_buffers=64MB, max_connections=10) | 30MB | 100MB | grounded; Postgres scales down well |
| lavinmq                            | 40MB   | 80MB    | unchanged from Phase 1 |
| admin-api (Bun + Hono, native WS, --max-old-space-size=96) | 30MB | 80MB | bounded by V8 heap cap + ~50MB native |
| admin-web (Next.js standalone under Bun, --max-old-space-size=128) | 80MB | ~180MB | measured locally on Windows; Linux RSS ~30% lower; capped |
| **Total**                          | **~460MB** | **~740MB** | |

t4g.micro (1GB, ~850MB usable) fits with **~110MB headroom** at
peak — ~13%. Tighter than Phase 1's t4g.small margin but
enforced by per-process V8 heap caps + systemd `MemoryMax`
limits, so OOMs are predictable application-level events
(systemd restart in ~2s, LavinMQ absorbs any in-flight events)
rather than mystery kernel OOM kills.

**Phase 2 default: t4g.micro (~$6/mo).** Reach for t4g.small if
a month of observed Phase 2 footprint consistently exceeds
~700MB peak.

### Confidence level and follow-up

Phase 1 application-memory numbers (admin-api, admin-web) are
estimates for the existing Node-based stack; the rest are
grounded in distro/daemon defaults. Cheapest way to sharpen them
is to run `docker stats` against the existing
`docker-compose.yml` setup for an evening — same process set,
real workload. Worth doing before 1.1 lands so the instance-type
choice is data-backed rather than estimated.

Phase 2's admin-web peak (~180MB) is measured: built locally and
run with `bun .next/standalone/server.js`, peak working set ~265MB
on Windows after warming all 14 routes; Linux RSS for the same
workload typically ~30% lower, hence the ~180MB projection.
Admin-api Phase 2 peak (~80MB) is bounded by the V8 heap cap.

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
- `/var/lib/lavinmq` on its own EBS volume

Cloud-init must mount these volumes **before** apt-installing
MongoDB or LavinMQ. If the install runs first, the package
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

- Lobby: `MONGO_URL` and `AMQP_URL` pointing at
  `admin.silencer.internal`. The lobby's existing
  `RABBITMQ_URL`/`-rabbitmq-url` env var and flag are renamed
  to `AMQP_URL`/`-amqp-url` to match the protocol rather than
  the implementation; the `amqp091-go` client is unchanged.
- Admin-api: `MONGO_URL` at localhost,
  `LOBBY_PLAYER_AUTH_URL` at `lobby.silencer.internal`
- Admin-web: `NEXT_PUBLIC_API_URL` (see Open Decisions for the
  externally-reachable form)

`mongod` and `lavinmq` configure through their distro
locations (`/etc/mongod.conf`, `/etc/lavinmq/lavinmq.ini`).
Bind addresses are set to the instance's primary private IP.
Application credentials — mongod SCRAM users, LavinMQ users,
the admin-api's JWT signing key, the GitHub backup-repo PAT —
are managed by Terraform variables and provisioned by
cloud-init from values in `/etc/silencer/*.env`. Rotation is
a Terraform variable change + apply.

Security groups gate access at the network layer; service-level
credentials gate access at the application layer. Box-to-box
ingress rules:

- Lobby SG: 15171/tcp from admin/data SG (admin-api → lobby
  playerauth)
- Admin/data SG: 27017/tcp from lobby SG (lobby → MongoDB),
  5672/tcp from lobby SG (lobby → LavinMQ AMQP)

## Implementation Phases

The plan ships in **two top-level phases**:

- **Phase 1** productionizes the existing admin-api and admin-web
  code (Node + Express + socket.io + mongoose, Next.js standalone
  under Bun) on Mongo + LavinMQ on a t4g.small admin/data box.
- **Phase 2** migrates Mongo → Postgres, rewrites admin-api on
  Bun + Hono with native WebSocket, adds V8 heap caps, and
  downsizes to t4g.micro.

The split de-risks by getting deploy infrastructure (Terraform,
GitHub Actions workflows, cloud-init, security groups, backup
cron) working with known-good code first, then migrating code on
a stable deploy substrate. Each sub-phase is its own PR,
sequenced by risk within its phase. Each PR also refreshes the
matching section of `docs/production.md`.

All sections above this one (Topology, Resource Profile's main
table, Data Persistence, Cross-Service Configuration) describe
the **Phase 1** state. Phase 2 deltas are described in their
respective sub-phases below; the Resource Profile section
includes a Phase 2 sizing projection.

### Phase 1 — Productionize current admin tier (Mongo, t4g.small)

Goal: the existing admin-api and admin-web code running in
production on AWS, with each component independently deployable
from GitHub Actions, on a t4g.small admin/data box with Mongo +
LavinMQ.

#### 1.1 — Provision the admin/data box

Terraform delta only. New EC2 instance in the lobby's VPC and AZ
(default `t4g.small` per Resource Profile), two EBS volumes
sized per Resource Profile (10GB Mongo, 5GB LavinMQ, 16GB root),
a security group with ingress rules sourced from the lobby SG
(27017/tcp, 5672/tcp), the private Route 53 hosted zone with A
records for both boxes, Tailscale enrollment for GitHub Actions
SSH access, and cloud-init that installs Mongo + LavinMQ bound
to the private interface with auth enabled (Mongo configured
with `cacheSizeGB: 0.25` per Resource Profile; LavinMQ from the
CloudAMQP apt repository). GitHub Actions does not yet deploy
app workloads to this box.

Includes an `infra/terraform/CLAUDE.md` update describing the new
admin/data box module, the EBS-volume-per-stateful-service
pattern, and the cloud-init mount-before-install ordering gotcha.

Done when: SSH from a tailnet peer works, `mongod` and
`lavinmq` accept authenticated connections from the lobby's
private IP, and `terraform taint && apply` the instance
preserves data on both EBS volumes.

#### 1.2 — Admin API deploy workflow

GitHub Actions workflow that builds an ARM64 OCI image, pushes to
GHCR, and updates the systemd unit on the admin/data box via SSH.
Workflow is path-filtered to the admin-api source directory.

This sub-phase migrates the admin-api Dockerfile from
`node:22-alpine` to `oven/bun:1-alpine` (per the repo-wide Bun+TS
rule: "migrate as you touch") while keeping the existing source
code unchanged — Bun runs Express + socket.io + mongoose code
without rewrites. The deploy verb is `docker pull` + symlink
swap + `systemctl restart silencer-admin-api`.

**Risk to validate before this PR lands:** confirm the existing
Express + socket.io + mongoose code runs cleanly under Bun.
Express and mongoose are reliable; socket.io has had historical
Bun friction. Quick check is to `bun run src/index.js` against
the existing `docker-compose.yml` data services for an evening.

Includes a `services/admin-api/CLAUDE.md` update covering the
container build, the systemd unit shape, and the deploy verb (per
the monorepo restructure's per-component CLAUDE.md rule).

Done when: a touch-only commit under that directory triggers a
new image, the box pulls it, and the unit restarts without
affecting any other service.

#### 1.3 — Admin Web deploy workflow

Same shape as 1.2 for the Next.js app: Dockerfile migrates from
`node:22-alpine` to `oven/bun:1-alpine`, Next.js standalone build
unchanged. Validated locally — `bun .next/standalone/server.js`
runs the standalone server without code changes (~1s startup,
~180MB RSS at peak per measurement). Path-filtered to the
admin-web source directory. Includes a `web/admin/CLAUDE.md`
update mirroring 1.2's documentation deliverable.

Done when: same as 1.2 for the web service.

#### 1.4 — Wire lobby ↔ data tier

Update the lobby's cloud-init to pass `MONGO_URL` and `AMQP_URL`
pointing at `admin.silencer.internal` with the lobby's service
credentials.

Rename `RABBITMQ_URL` → `AMQP_URL` and `-rabbitmq-url` →
`-amqp-url` everywhere it's referenced; the rename ships in one
PR so dev and prod env-var naming stay in sync. Touch sites:

- `services/lobby/main.go` — flag and env var (one-line change each)
- `services/lobby/CLAUDE.md` — two references in build/run docs
- `services/admin-api/src/config.js` — exported constant + env read
- `services/admin-api/src/amqp/consumer.js` — import + use site
- `services/admin-api/CLAUDE.md` — env-var list
- `docker-compose.yml` — both `lobby` and `admin-api` service env blocks

Open 15171/tcp on the lobby's SG to the admin/data box's SG and
set the admin-api's `LOBBY_PLAYER_AUTH_URL` to
`lobby.silencer.internal`. Requires lobby instance replacement
(cloud-init runs once).

Done when: lobby logs show successful Mongo sync on player
mutations and successful AMQP publishes on match end, and
admin-api validates a player credential against the lobby over
the private network.

#### 1.5 — Snapshot policy

Add the DLM lifecycle policy that snapshots both EBS volumes on
the admin/data box daily, with a rolling retention window.
Independent of any ingress decision, so it can ship as soon as
1.1 is in.

Done when: a fresh DLM-created snapshot exists for each volume,
and the retention window prunes correctly after one cycle.

#### 1.6 — Public ingress for admin-web and admin-api

Resolve the public-ingress Open Decision (Tailscale-only,
Cloudflare, ALB+ACM, or admin-web-proxies-admin-api) and
implement the chosen option. Until this lands, both services are
reachable only over the tailnet.

Done when: the chosen URL pattern resolves end-to-end from a
non-developer browser (or, for the Tailscale-only path, the
decision is recorded and the tailnet ACL grants the intended
audience).

### Phase 2 — Optimize: Postgres + Bun-native admin tier (t4g.micro)

Goal: cut peak admin/data box RAM by ~60% and downsize to
t4g.micro by replacing Mongo with Postgres, rewriting admin-api
on Bun + Hono with native WebSocket, and adding V8 heap caps to
both admin processes. Sub-phases sequenced so the storage swap
lands first (largest wins, lowest cross-cutting), then the
admin-api code rewrite, then the heap caps, then the downsize.

#### 2.1 — Migrate admin storage Mongo → Postgres

Replace `mongod` with a tuned-tiny `postgres` systemd unit on the
admin/data box (apt-installed, `shared_buffers=64MB`,
`max_connections=10`, `work_mem=2MB`,
`effective_cache_size=128MB`; data on a new EBS volume mounted
at `/var/lib/postgresql`).

Rewrite admin-api's mongoose models (`Player`, `Session`,
`MatchStat`, `Event`, `AdminUser`) as Drizzle schemas (chosen
over Kysely for the migration tooling) against Postgres. Variable
fields (`Event.data`, `lifetimeStats`, `agencies[]`) become
`jsonb` columns. The `consumer.js` upsert paths translate
near-line-for-line: `findOneAndUpdate({...}, {$set, $inc, $setOnInsert})`
becomes `INSERT ... ON CONFLICT DO UPDATE`.

Delete `services/lobby/mongosync.go` and its 4 call sites in
`services/lobby/store.go`. The lobby becomes a pure AMQP publisher;
admin-api becomes the sole writer to admin storage. This matches
what the original plan asserted ("MongoDB is a read mirror") —
`mongosync.go` was a parallel side-channel that bypassed the
event log. The `player.login` AMQP event already does the upsert
that `SyncPlayer()` was duplicating.

Migrate the existing kristiandelay backup design: the same
GitHub Actions cron, but `pg_dump` instead of `mongodump`,
committing a compressed SQL dump (~5-10MB at year of data) every
6h to the same backup repo. Identical rollback story; smaller
dumps.

One-time data migration: admin-api ships a one-shot
`bun scripts/migrate-mongo-to-pg.ts` that reads from Mongo,
writes to Postgres, verifies row counts. Cutover is run-script →
swap admin-api connection string in
`/etc/silencer/admin-api.env` → restart admin-api → confirm
fresh-event flow lands in Postgres → stop `mongod`. Mongo EBS
volume kept attached for 30 days as a fallback, then deleted with
explicit confirmation.

Remove the lobby's `MONGO_URL` env var/flag entirely (only
`mongosync.go` used it; that's gone).

Done when: admin-api reads/writes against Postgres, lobby
publishes all state changes via AMQP, total Player / Session /
MatchStat / Event row counts in Postgres match the Mongo snapshot
at cutover ±the events that arrived during the cutover window
(should be 0 if cutover is done during a quiet hour), `mongod`
systemd unit is `disabled`, the daily DLM snapshot policy now
covers `/var/lib/postgresql` instead of `/var/lib/mongodb`, and
a `pg_dump`-based backup commits successfully on its first cron
run.

#### 2.2 — Rewrite admin-api on Bun + Hono with native WebSocket

Replace Express → Hono (drop-in router + middleware), socket.io →
`Bun.serve` native WebSocket, mongoose → Drizzle (already done
in 2.1). Remove the deprecated dependencies: `express`, `cors`,
`socket.io`, `mongoose`, `node-cron` (use `setInterval` + a
process-internal scheduler), `bcryptjs` (Bun has `Bun.password`
built in). Container image `ENTRYPOINT` switches from
`bun src/index.js` (Bun running Node code) to native Bun TS.

Source code migrates from `.js` to `.ts` per the universal Bun+TS
rule. Each route file (~7 of them) becomes a Hono route module;
auth middleware uses `hono/jwt`; rate limiting becomes a small
in-process middleware (~30 LOC).

WebSocket hand-off: the existing `admin-dashboard` socket.io
namespace becomes a single `Bun.serve` upgrade handler. The
admin-web client side (`web/admin/lib/socket.js`) drops
`socket.io-client` and uses native `WebSocket` — small
client-side change but it ships in the same PR as the server
change.

Done when: admin-api deploys, all routes return correct responses
against an integration test suite (added if not present), WS
clients reconnect through the native upgrade path, and resident
memory under steady load is ≤80MB.

#### 2.3 — Heap caps and OOM observability

Update both admin services' systemd units with explicit V8 heap
caps and systemd cgroup memory limits as belt-and-suspenders:

```ini
# silencer-admin-web.service
Environment=NODE_OPTIONS=--max-old-space-size=128
MemoryHigh=176M
MemoryMax=192M
Restart=always
RestartSec=2s
StartLimitInterval=60s
StartLimitBurst=5
```

```ini
# silencer-admin-api.service
Environment=NODE_OPTIONS=--max-old-space-size=96
MemoryHigh=128M
MemoryMax=144M
Restart=always
RestartSec=2s
StartLimitInterval=60s
StartLimitBurst=5
```

Add the CloudWatch agent emitting per-process `mem_used_percent`
and a log-pattern alarm on `JavaScript heap out of memory`. Add
server-side pagination caps on admin-api routes that could return
unbounded result sets (`/events`, `/matchstats`: max `limit=200`
regardless of client request).

Done when: an intentional load test that allocates beyond the cap
crashes the offending process cleanly, systemd restarts it within
~5s, no AMQP events are lost (LavinMQ holds them across the
restart, admin-api drains on resume), and the alarm fires.

#### 2.4 — Downsize to t4g.micro

Change the instance type in Terraform from t4g.small to t4g.micro
**after** observing at least two weeks of post-2.3 footprint at
peak hours (busy weekend evening) and confirming memory usage
tracks the Phase 2 sizing projection. Stop instance, change
type, apply.

Done when: admin/data box runs on t4g.micro, peak observed memory
is ≤750MB (≥10% headroom on the 850MB usable), CPU credit balance
remains positive across a full peak weekend, and any admin-api
OOMs trigger predictable ~2s recoveries via systemd.

## Open Decisions

Each of these is owned by a specific sub-phase, not a 1.1
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
  ~$18/mo, cleanest separation. Default through 1.2–1.5 is (a);
  the choice is the substance of 1.6. A fourth path is to make
  admin-web proxy API calls server-side and drop
  `NEXT_PUBLIC_API_URL`, leaving only admin-web to expose.

- **Admin-API ↔ admin-web separation.** 1.2 / 1.3 colocate them
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
  consumer for the existing LavinMQ event stream is a
  follow-up, not part of this work.

## Success Criteria

The plan is done when all of the following hold simultaneously:

1. Re-deploying the lobby does not touch the admin/data box.
2. Re-deploying admin-api does not restart admin-web, Mongo,
   LavinMQ, or the lobby.
3. Re-deploying admin-web does not restart admin-api, Mongo,
   LavinMQ, or the lobby.
4. `terraform taint aws_instance.<admin>` followed by
   `terraform apply` rebuilds the admin/data instance with zero
   loss of MongoDB or LavinMQ state.
5. The lobby's MongoDB mirror and AMQP event stream are
   live in production — no `[mongosync] connect failed` or
   `[events] amqp connect failed` log lines under steady state.
6. Each of the four components is deployable from GitHub Actions
   with no manual SSH steps.
