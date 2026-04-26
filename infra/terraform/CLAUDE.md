# infra/terraform — AWS infra

Two EC2 hosts (ARM64 Graviton) in the default VPC, same subnet/AZ:

- **Lobby box** (`aws_instance.lobby`, `t4g.small`) — runs the Go lobby
  server and spawns `silencer -s` dedicated-server subprocesses per
  game. Elastic IP, optional Route 53 A record, dedicated EBS data
  volume mounted at `/var/lib/silencer`.
- **Admin/data box** (`aws_instance.admin`, `t4g.small`) — runs
  MongoDB + LavinMQ (apt-installed, each on its own EBS volume),
  containerised admin-api + admin-web pulled from GHCR, and a
  Cloudflare Tunnel daemon for public ingress to `admin.arsiamons.com`
  with no public ports open on the SG.

The two boxes talk over VPC private IPs via a shared private Route 53
zone (`silencer.internal`), so cross-service hostnames stay stable
across instance replacement.

> For the stand-up-from-scratch walkthrough and day-2 ops, see
> `docs/production.md`. This file covers the *why* of the Terraform
> code; `docs/production.md` covers the *how* of running it.

## File layout

- `bootstrap/` — creates the S3 bucket + DynamoDB lock table that hold
  the main module's remote state. **Uses LOCAL state itself**
  (chicken-and-egg). Apply once per AWS account, then paste the
  `backend_hcl` output into `../backend.hcl` (gitignored).
- `main.tf` — provider, shared data sources (VPC, subnet, AMI, key
  pair), lobby SG + EC2 + EIP + EBS + lobby Route 53.
- `admin.tf` — admin/data box SG + EC2 + EIP + Mongo & LavinMQ EBS
  volumes + cross-SG ingress rules to/from the lobby SG.
- `dns.tf` — private `silencer.internal` Route 53 zone with A records
  for both boxes. Resolves only inside the VPC.
- `dlm.tf` — Data Lifecycle Manager: daily snapshots of the admin/data
  box's two stateful EBS volumes (Mongo + LavinMQ), 7-day retention.
- `cloud-init.yaml.tftpl` — lobby's bootstrap.
- `cloud-init-admin.yaml.tftpl` — admin/data box's bootstrap. The
  substantive one: mounts EBS, installs Mongo + LavinMQ, writes
  systemd units for the containerised app workloads, joins Tailscale,
  installs cloudflared.

Day-to-day you only touch the root module. `bootstrap/` is fire-and-forget.

## Tailscale model

The lobby host joins the tailnet as `tag:server`. GitHub Actions
runners join as `tag:server` too (reusing the same ACL) and SSH over
WireGuard — **port 22 is not open to the public internet**, only on
the tailnet. See `ssh_allowed_cidr` variable; the public SG rule is
there for emergency break-glass access from the admin's IP.

`cloud-init.yaml.tftpl` runs `tailscale up --accept-dns=false` on
purpose: we don't want MagicDNS overriding AWS's internal resolver.

## Cross-SG inline rule cycles

Lobby SG and admin SG reference each other (lobby allows :15171 from
admin; admin allows :27017 + :5672 from lobby). Inline `ingress`
blocks in both SGs would create a circular terraform dependency.
**Always declare cross-SG rules as `aws_vpc_security_group_ingress_rule`
resources** (see `admin.tf`) — not inline. Inline rules are reserved
for CIDR-based ingress (SSH break-glass, public lobby ports).

## cloud-init mount-before-install (admin box)

`cloud-init-admin.yaml.tftpl` MUST mount `/var/lib/mongodb` and
`/var/lib/lavinmq` from their EBS volumes BEFORE apt installs
`mongodb-org` / `lavinmq`. The package postinst writes initial state
into those paths; if it runs before the mount, the postinst output
lands on the root volume and the later mount hides it — service won't
start. Order in runcmd: wait-for-device → mkfs-if-missing → mount -a
→ apt install → chown to package user → start service.

## cloud-init quirks (`cloud-init.yaml.tftpl`)

- **EBS volume wait lives in `runcmd`, not `bootcmd`.**
  `aws_volume_attachment` fires after instance boot, so `bootcmd`
  would race. The `for i in $(seq 1 60)` loop polls `/dev/nvme1n1`.
- **Format is idempotent.** `mkfs.ext4` only runs if the
  `silencer-data` label is missing — safe across reboots and
  instance replaces that reuse the volume.
- **`HOME=/var/lib/silencer` in the systemd unit.** The
  dedicated-server subprocess writes `PALETTECALC*.BIN` under `$HOME`.
  Without the override those writes go to `/home/silencer`, which is
  blocked by `ProtectHome=true`. Pointing HOME at the data volume lets
  them land inside `ReadWritePaths`.
- **`/opt/silencer` is `ubuntu:ubuntu`** so GitHub Actions can scp
  releases in without sudo. The service reads via world-exec.
- **`CAP_NET_BIND_SERVICE`** lets the service bind port 517 without
  running as root.

## Deploy flow

1. `git tag v0.x && git push --tags` → `.github/workflows/deploy.yml`
   runs on `ubuntu-24.04-arm`.
2. Builds `services/lobby/silencer-lobby` (Go) and `build/silencer`
   (C++, ARM64, with `-DSILENCER_LOBBY_HOST=<vars.LOBBY_HOST>`).
3. Joins the tailnet, scps both binaries + `shared/assets/` (landed
   as `assets/` on the host) to
   `ubuntu@<vars.DEPLOY_HOST>:/opt/silencer/releases/<short-sha>/`.
4. Swaps the `/opt/silencer/current` symlink, restarts
   `silencer-lobby` systemd unit, keeps last 3 releases.

For ad-hoc debug iterations, `infra/scripts/fastdeploy.sh` rsyncs the
working tree, builds on the box, and swaps the binary — skipping the
CI round trip.

## `lobby_version_string = ""` is intentional

Default is empty (accept-any) so release-tag-driven client version
bumps don't need an infrastructure rebuild. Set it to a specific
string (e.g. `"00024"`) only if you need to lock out older clients —
and note that takes effect via cloud-init, which requires an instance
replace.

## Admin app systemd units

`silencer-admin-api.service` and `silencer-admin-web.service` (cloud-init
write_files) are containerised: each reads its image ref from
`/etc/silencer/<svc>.image` (an `EnvironmentFile`-shape file with
`IMAGE=ghcr.io/...:<sha>`) and `docker run`s with
`--env-file /etc/silencer/<svc>.env`. The deploy workflows update the
`.image` file then `systemctl restart`. Until the first deploy succeeds
the units crash-loop quietly (`Restart=always`, same pattern as the
lobby's binary). `--network host` so admin-api can reach the
host-installed mongod and lavinmq on `127.0.0.1`.

## Cloudflare Tunnel (no public ingress on admin SG)

The admin/data box's SG opens NO public HTTP/S ports. `cloudflared` is
installed via `cloudflared service install $TOKEN` and dials out to
Cloudflare. Public-hostname routing for `admin.arsiamons.com` is
configured in the Cloudflare dashboard, not here:

| Path           | Origin                  |
|----------------|-------------------------|
| `/api/*`       | `http://localhost:24080` |
| `/socket.io/*` | `http://localhost:24080` |
| catch-all      | `http://localhost:24000` |

This routing is what lets a single hostname host both admin-web (the
dashboard) and admin-api (the REST + WS backend) without path
collisions — admin-api mounts under `/api` so its endpoints never
shadow admin-web's pages (`/players`, `/me`, `/health`, `/gamestats`).

## Required secrets / vars

Required for both boxes:

- `var.ssh_public_key` — admin key (also lives in AWS key pair)
- `var.deploy_ssh_public_key` — paired private half lives in
  `DEPLOY_SSH_KEY` repo secret; used by GH Actions only
- `var.tailscale_auth_key` — one-time key tagged `tag:server`,
  consumed once by lobby cloud-init; rotate by replacing the instance

Required for admin box:

- `var.admin_tailscale_auth_key` — one-time `tag:server` key, separate
  from the lobby's; consumed once by admin cloud-init
- `var.mongo_silencer_password`, `var.lavinmq_silencer_password` —
  app-tier credentials provisioned into `/etc/silencer/*.env` (mode
  0600) and used by both lobby and admin-api
- `var.jwt_secret` — admin-api JWT signing key
- `var.cloudflare_tunnel_token` — Cloudflare Tunnel auth token

Optional:

- `var.github_backup_token` — PAT for admin-api's Mongo backup commits
- `var.admin_image_admin_api`, `var.admin_image_admin_web` — initial
  image refs (deploy workflow overwrites these via `/etc/silencer/*.image`)

GH repo vars: `LOBBY_HOST`, `DEPLOY_HOST`, `ADMIN_DEPLOY_HOST` (Tailscale
hostname of admin/data box).
GH repo secrets: `TS_AUTHKEY` (runner tailnet auth), `DEPLOY_SSH_KEY`.
