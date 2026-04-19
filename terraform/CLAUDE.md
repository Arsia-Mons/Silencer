# terraform/ — AWS infra

Single EC2 host (ARM64 Graviton, `t4g.small`) that runs the Go lobby
server and spawns `zsilencer -s` dedicated-server subprocesses per
game. Elastic IP, optional Route 53 A record, dedicated EBS data
volume mounted at `/var/lib/zsilencer`.

> For the stand-up-from-scratch walkthrough and day-2 ops, see
> `docs/production.md`. This file covers the *why* of the Terraform
> code; `docs/production.md` covers the *how* of running it.

## Two-module split

- `bootstrap/` — creates the S3 bucket + DynamoDB lock table that hold
  the main module's remote state. **Uses LOCAL state itself**
  (chicken-and-egg). Apply once per AWS account, then paste the
  `backend_hcl` output into `../backend.hcl` (gitignored).
- `.` (root) — everything else: VPC lookup, security group, EC2, EIP,
  EBS data volume, Route 53, cloud-init. Uses the S3 backend.

Day-to-day you only touch the root module. `bootstrap/` is fire-and-forget.

## Tailscale model

The lobby host joins the tailnet as `tag:server`. GitHub Actions
runners join as `tag:server` too (reusing the same ACL) and SSH over
WireGuard — **port 22 is not open to the public internet**, only on
the tailnet. See `ssh_allowed_cidr` variable; the public SG rule is
there for emergency break-glass access from the admin's IP.

`cloud-init.yaml.tftpl` runs `tailscale up --accept-dns=false` on
purpose: we don't want MagicDNS overriding AWS's internal resolver.

## cloud-init quirks (`cloud-init.yaml.tftpl`)

- **EBS volume wait lives in `runcmd`, not `bootcmd`.**
  `aws_volume_attachment` fires after instance boot, so `bootcmd`
  would race. The `for i in $(seq 1 60)` loop polls `/dev/nvme1n1`.
- **Format is idempotent.** `mkfs.ext4` only runs if the
  `zsilencer-data` label is missing — safe across reboots and
  instance replaces that reuse the volume.
- **`HOME=/var/lib/zsilencer` in the systemd unit.** The
  dedicated-server subprocess writes `PALETTECALC*.BIN` under `$HOME`.
  Without the override those writes go to `/home/zsilencer`, which is
  blocked by `ProtectHome=true`. Pointing HOME at the data volume lets
  them land inside `ReadWritePaths`.
- **`/opt/zsilencer` is `ubuntu:ubuntu`** so GitHub Actions can scp
  releases in without sudo. The service reads via world-exec.
- **`CAP_NET_BIND_SERVICE`** lets the service bind port 517 without
  running as root.

## Deploy flow

1. `git tag v0.x && git push --tags` → `.github/workflows/deploy.yml`
   runs on `ubuntu-24.04-arm`.
2. Builds `server/zsilencer-lobby` (Go) and `build/zsilencer` (C++,
   ARM64, with `-DZSILENCER_LOBBY_HOST=<vars.LOBBY_HOST>`).
3. Joins the tailnet, scps both binaries + `data/` to
   `ubuntu@<vars.DEPLOY_HOST>:/opt/zsilencer/releases/<short-sha>/`.
4. Swaps the `/opt/zsilencer/current` symlink, restarts
   `zsilencer-lobby` systemd unit, keeps last 3 releases.

For ad-hoc debug iterations, `scripts/fastdeploy.sh` rsyncs the
working tree, builds on the box, and swaps the binary — skipping the
CI round trip.

## `lobby_version_string = ""` is intentional

Default is empty (accept-any) so release-tag-driven client version
bumps don't need an infrastructure rebuild. Set it to a specific
string (e.g. `"00024"`) only if you need to lock out older clients —
and note that takes effect via cloud-init, which requires an instance
replace.

## Required secrets / vars

- `var.ssh_public_key` — admin key (also lives in AWS key pair)
- `var.deploy_ssh_public_key` — paired private half lives in
  `DEPLOY_SSH_KEY` repo secret; used by GH Actions only
- `var.tailscale_auth_key` — one-time key tagged `tag:server`,
  consumed once by cloud-init; rotate by replacing the instance
- GH repo vars: `LOBBY_HOST`, `DEPLOY_HOST`
- GH repo secrets: `TS_AUTHKEY` (runner's tailnet auth), `DEPLOY_SSH_KEY`
