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

> **Staging** is a separate sibling module at `staging/` — one
> `t4g.small` running everything as a single-box smoke-test
> environment, redeployed on every push to `main`. See `staging/CLAUDE.md`
> and `docs/plans/2026-04-27-staging-environment.md`. The two modules
> share the same S3 state bucket / DynamoDB lock table (only the
> state-file `key` differs) and the same SSH pubkeys + GHCR pull token;
> everything else (Mongo / LavinMQ / JWT / Tailscale auth keys) is
> separately namespaced under `/silencer-staging/*`.

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
- `iam.tf` — instance IAM roles + profiles. Each box gets read access
  to its own subset of `/silencer/*` SSM parameters; cloud-init shells
  out to `aws ssm get-parameter --with-decryption` to materialise
  runtime secrets into `/etc/silencer/*.env`.
- `ssm.tf` — `data "aws_ssm_parameter"` blocks for *apply-time*
  values (SSH pubkeys, Tailscale auth keys, Cloudflare tunnel token)
  that have to be templated into resources at plan time. Their values
  end up in tfstate; rotation is `aws ssm put-parameter` + `terraform
  taint <instance>`. Anything that rotates more often is fetched at
  runtime via the IAM role instead.

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

Tag-driven release via `.github/workflows/deploy.yml` (ARM64): builds
the Go lobby + C++ dedicated server, scps to
`/opt/silencer/releases/<sha>/`, swaps the `current` symlink, restarts
`silencer-lobby`, keeps last 3 releases. Debug shortcut:
`infra/scripts/fastdeploy.sh`.

## `lobby_version_string = ""` is intentional

Default is empty (accept-any) so release-tag-driven client version
bumps don't need an infrastructure rebuild. Set it to a specific
string (e.g. `"00024"`) only if you need to lock out older clients —
and note that takes effect via cloud-init, which requires an instance
replace.

## Admin app systemd units

`silencer-admin-api.service` and `silencer-admin-web.service` are
written by cloud-init: each reads its image ref from
`/etc/silencer/<svc>.image` and `docker run`s `--network host
--env-file /etc/silencer/<svc>.env`. Deploy workflows update the
`.image` file then `systemctl restart`. Pre-first-deploy units
crash-loop quietly (`Restart=always`).

## Cloudflare Tunnel (no public ingress on admin SG)

The admin SG opens NO public HTTP/S ports. `cloudflared` dials out to
Cloudflare; public-hostname routing for `admin.arsiamons.com` is
configured in the Cloudflare dashboard, not here. The path-based
routing rules (`/api/*` and `/socket.io/*` → admin-api, catch-all →
admin-web) are documented in `services/admin-api/CLAUDE.md` and
`web/admin/CLAUDE.md`.

## Secrets — all in SSM Parameter Store

Nothing sensitive lives in `terraform.tfvars` anymore. Every secret is
an SSM `SecureString` (or plain `String` for SSH pubkeys) under
`/silencer/*`. Seed them once with `infra/scripts/seed-ssm.sh`;
teammates with IAM read access pull from the same source.

### Param inventory

| Param                                          | Type         | Consumer                                     | Mechanism                       |
|------------------------------------------------|--------------|----------------------------------------------|---------------------------------|
| `/silencer/shared/ssh_admin_pubkey`            | String       | `aws_key_pair.admin`                         | TF data → resource              |
| `/silencer/shared/deploy_ssh_pubkey`           | String       | both boxes' `ssh_authorized_keys`            | TF data → user_data             |
| `/silencer/shared/mongo_silencer_password`     | SecureString | mongod user create + admin-api + lobby       | **Runtime fetch (IAM role)**    |
| `/silencer/shared/lavinmq_silencer_password`   | SecureString | lavinmq user create + admin-api + lobby      | **Runtime fetch (IAM role)**    |
| `/silencer/lobby/tailscale_auth_key`           | SecureString | lobby cloud-init (one-shot)                  | TF data → user_data             |
| `/silencer/admin/tailscale_auth_key`           | SecureString | admin cloud-init (one-shot)                  | TF data → user_data             |
| `/silencer/admin/jwt_secret`                   | SecureString | admin-api                                    | **Runtime fetch (IAM role)**    |
| `/silencer/admin/cloudflare_tunnel_token`      | SecureString | `cloudflared service install` (one-shot)     | TF data → user_data             |
| `/silencer/admin/github_backup_token`          | SecureString | admin-api (optional; empty = backups off)    | **Runtime fetch (IAM role)**    |

**Two mechanisms because of two different needs**:
*TF data* values get baked into resources at apply time and end up in
tfstate (encrypted at rest in the S3 backend). That's fine for one-shot
values which are inert post-boot anyway. *Runtime fetch* values are
read by the EC2 instance itself via its IAM role — they never enter
tfstate, and rotation is `put-parameter` + ssh in + `silencer-fetch-secrets`
+ `systemctl restart`, no `terraform apply` needed.

### Adding a new secret

1. Add a `put_param` line to `infra/scripts/seed-ssm.sh`.
2. **Runtime-fetched?** Add the ARN to `local.lobby_runtime_param_arns`
   or `local.admin_runtime_param_arns` in `iam.tf`, then add a `get`
   call in the relevant `silencer-fetch-secrets` block in cloud-init.
3. **Apply-time?** Add a `data "aws_ssm_parameter"` to `ssm.tf`, then
   reference `.value` in the `templatefile()` map.

### Rotation

See `docs/production.md` § *Day-to-day → Rotating secrets* for the
end-to-end flow. The mechanism column above tells you which path
applies: runtime-fetched values rotate without `terraform apply`;
apply-time values need `terraform taint <instance>` + apply.

### GitHub Actions

Repo vars: `LOBBY_HOST`, `DEPLOY_HOST`, `ADMIN_DEPLOY_HOST`.
Repo secrets: `DEPLOY_SSH_KEY` (paired with `/silencer/shared/deploy_ssh_pubkey`),
`TS_AUTHKEY` (Tailscale auth for the GH Actions runner — separate
from the box auth keys, reusable+ephemeral).
