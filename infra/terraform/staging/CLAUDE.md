# infra/terraform/staging — single-box staging environment

One `t4g.small` ARM64 box (`silencer-staging`) running every Silencer
component on the same host. Redeploys on every push to `main` via
`.github/workflows/deploy-staging.yml`. **Disposable** — no backups,
no DLM snapshots, no separate stateful EBS volumes.

Design and rationale: `docs/plans/2026-04-27-staging-environment.md`.
Prod sibling: `../` (the parent directory).

## Why a separate module instead of a workspace flag

Two reasons. The cloud-init template is meaningfully different from
prod's two-box layout (everything talks to `127.0.0.1`, no separate
EBS volumes to wait for, no Cloudflare Tunnel) — squeezing it into the
prod template behind a `single_box ?` ternary makes every prod edit
reason about a code path that doesn't apply to it. And prod's
`aws_ebs_volume.admin_{mongo,lavinmq}` have `prevent_destroy = true`,
so a workspace flip would refuse to apply. Separate modules → zero
shared state → zero cross-contamination risk.

## Layout

- `main.tf` — provider + VPC/subnet/AMI data + EC2 + EIP + SG + Route 53
  A record (when `domain_name` + `route53_zone_id` set).
- `iam.tf` — single instance role with read access to
  `/silencer-staging/*` (Mongo / LavinMQ / JWT) and the prod-shared
  `/silencer/admin/ghcr_pull_token`.
- `ssm.tf` — apply-time data sources for the SSH pubkeys (shared with
  prod) and the staging-specific Tailscale auth key.
- `outputs.tf` — `staging_lobby_ip` (paste into `vars.STAGING_LOBBY_PUBLIC_IP`).
- `cloud-init-staging.yaml.tftpl` — the substantive file. Installs
  Mongo + LavinMQ + Docker, writes the three systemd units (lobby,
  admin-api, admin-web), bootstraps the Mongo + LavinMQ users,
  fetches secrets from SSM, joins Tailscale.
- `backend.tf` + `backend.hcl.example` — uses the same S3 bucket /
  DynamoDB lock table that prod's `bootstrap/` created; only the
  state-file `key` differs (`silencer/staging.tfstate`).

## First-time setup

```sh
cd infra/terraform/staging
cp backend.hcl.example backend.hcl   # edit bucket name → matches prod
cp terraform.tfvars.example terraform.tfvars  # set ssh_allowed_cidr
terraform init -backend-config=backend.hcl
terraform apply
```

After apply:

1. Copy the `staging_lobby_ip` output into the GitHub repo variable
   `STAGING_LOBBY_PUBLIC_IP` (the dedicated-server cmake build needs
   it baked in; `inet_addr()` constraint on the C++ join path).
2. Set `STAGING_DEPLOY_HOST` to the Tailscale name (default
   `silencer-staging`).
3. Optionally set `STAGING_LOBBY_HOST` to a DNS name pointing at the
   EIP (the value devs will use with `cmake -DSILENCER_LOBBY_HOST=`).
   If empty, the deploy workflow uses the EIP directly.
4. First deploy: push to `main` (or run `deploy-staging.yml` manually).
   Until it lands, the three units crash-loop quietly — same pattern
   as prod's pre-first-deploy state.

## Sharing with prod

| Resource | Shared? | Why |
|---|---|---|
| S3 state bucket + DynamoDB lock table | yes | Bootstrapped once; only the state key differs |
| `aws_key_pair.admin` | per-module | Same SSH pubkey, but distinct AWS key pair (different name prefix) so a `terraform destroy` here can't unmake the prod one |
| SSH pubkeys (`/silencer/shared/{ssh_admin,deploy_ssh}_pubkey`) | yes | Same humans, same GH Actions deploy key |
| Mongo / LavinMQ passwords | per-stage | Distinct so a leaked staging password can't authenticate to prod |
| JWT secret | per-stage | Distinct so a forged staging token isn't accepted by prod |
| Tailscale auth key | per-stage | Distinct so a leaked staging key can't enroll into prod's tailnet |
| GHCR pull token (`/silencer/admin/ghcr_pull_token`) | yes | Read-only, repo-scoped — staging pulls the same images prod pulls (just a different tag prefix) |
| Route 53 zones | independent | Staging gets `staging.<domain>` if configured; no shared internal zone |

## What's intentionally NOT here

- **No separate EBS volumes.** mongod, lavinmq, lobby.json, and
  shared/assets all live on the root volume. Recovery from data
  corruption = `terraform taint aws_instance.staging && terraform apply`.
- **No DLM.** Staging has no snapshots — wiping is the recovery path.
- **No Cloudflare Tunnel / public HTTPS.** admin-api/admin-web bind
  `:24080`/`:24000` and are reachable from the tailnet only.
- **No private Route 53 zone.** Single box, everything is `127.0.0.1`.

## Day-to-day

`terraform plan` after editing the cloud-init template will show
`user_data` drift; that's expected (`ignore_changes = [user_data]` on
the instance). To re-bootstrap, `terraform taint aws_instance.staging`
+ apply. The EIP is preserved across taints, so the
`STAGING_LOBBY_PUBLIC_IP` repo variable doesn't need updating.

## Adding a new SSM-backed secret

1. `infra/scripts/seed-ssm.sh` — add a `put_param` line under
   `/silencer-staging/*`.
2. **Runtime-fetched?** Append the ARN to `local.staging_runtime_param_arns`
   in `iam.tf`, then add a `get` call in `silencer-fetch-secrets`
   inside the cloud-init template.
3. **Apply-time?** Add a `data "aws_ssm_parameter"` to `ssm.tf`,
   reference `.value` in the `templatefile()` map.
