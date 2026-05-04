#!/usr/bin/env bash
# Seed AWS SSM Parameter Store with the secrets the Silencer Terraform
# module reads. Run once per AWS account (or whenever you add a teammate
# / rotate a value); subsequent runs are idempotent and skip params that
# already exist.
#
# Why this exists: secrets used to live in terraform.tfvars on a single
# laptop. SSM is the durable, share-with-teammates source of truth — TF
# either fetches via data sources at apply time, or grants the EC2
# instances IAM read access so cloud-init pulls them at runtime. Either
# way, no secret values land in tfvars.
#
# Usage:
#   ./infra/scripts/seed-ssm.sh                # interactive, skip existing
#   ./infra/scripts/seed-ssm.sh --overwrite    # interactive, replace existing
#   AWS_REGION=us-east-2 ./infra/scripts/seed-ssm.sh
#
# Requires: aws cli v2, openssl, configured AWS creds with
# ssm:PutParameter on /silencer/* (default-region KMS key is used for
# SecureString encryption — no extra setup).

set -euo pipefail

REGION="${AWS_REGION:-us-west-1}"
OVERWRITE=0

for arg in "$@"; do
  case "$arg" in
    --overwrite) OVERWRITE=1 ;;
    -h|--help)
      sed -n '2,/^set -euo/p' "$0" | sed 's/^# \{0,1\}//;$d'
      exit 0
      ;;
    *)
      echo "unknown arg: $arg" >&2
      exit 2
      ;;
  esac
done

command -v aws >/dev/null || { echo "aws cli not found" >&2; exit 1; }
command -v openssl >/dev/null || { echo "openssl not found" >&2; exit 1; }

# --- helpers ----------------------------------------------------------

# put_param NAME TYPE VALUE
#  - TYPE: SecureString | String
#  - Skips if param exists unless --overwrite was passed.
put_param() {
  local name="$1" type="$2" value="$3"
  if aws ssm get-parameter --region "$REGION" --name "$name" >/dev/null 2>&1; then
    if [ "$OVERWRITE" -eq 0 ]; then
      echo "  exists, skipping: $name"
      return 0
    fi
    echo "  overwriting:      $name"
    aws ssm put-parameter --region "$REGION" --name "$name" --type "$type" \
      --value "$value" --overwrite >/dev/null
  else
    echo "  creating:         $name"
    aws ssm put-parameter --region "$REGION" --name "$name" --type "$type" \
      --value "$value" >/dev/null
  fi
}

# Generate a 32-byte url-safe random secret. Used for password-shaped
# values where any high-entropy string works. Output is base64url
# (RFC 4648 §5) — `+` → `-`, `/` → `_`, padding stripped — because
# these passwords end up inside `mongodb://` and `amqp://` URLs and a
# raw `/` makes Go's `net/url.Parse` see "invalid port after host".
gen_secret() { openssl rand -base64 32 | tr -d '\n=' | tr '/+' '_-'; }

# Prompt for a value, hiding input. Echoes the result.
prompt_secret() {
  local label="$1" val=""
  while [ -z "$val" ]; do
    printf '  %s: ' "$label" >&2
    stty -echo
    IFS= read -r val
    stty echo
    printf '\n' >&2
  done
  printf '%s' "$val"
}

# Prompt for the contents of a public SSH key (file path or paste).
prompt_pubkey() {
  local label="$1" val=""
  printf '  %s\n    file path or paste pubkey, then Enter: ' "$label" >&2
  IFS= read -r val
  if [ -f "$val" ]; then
    val=$(cat "$val")
  fi
  printf '%s' "$val"
}

# --- shared (used by both boxes) --------------------------------------

echo "==> /silencer/shared/* (used by both boxes)"

if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer/shared/ssh_admin_pubkey >/dev/null 2>&1 \
   || [ "$OVERWRITE" -eq 1 ]; then
  put_param /silencer/shared/ssh_admin_pubkey String \
    "$(prompt_pubkey 'admin SSH pubkey (your laptop, e.g. ~/.ssh/silencer-admin.pub)')"
else
  put_param /silencer/shared/ssh_admin_pubkey String "(unused)"
fi

if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer/shared/deploy_ssh_pubkey >/dev/null 2>&1 \
   || [ "$OVERWRITE" -eq 1 ]; then
  put_param /silencer/shared/deploy_ssh_pubkey String \
    "$(prompt_pubkey 'deploy SSH pubkey (GH Actions, e.g. ~/.ssh/silencer-deploy.pub)')"
else
  put_param /silencer/shared/deploy_ssh_pubkey String "(unused)"
fi

put_param /silencer/shared/mongo_silencer_password   SecureString "$(gen_secret)"
put_param /silencer/shared/lavinmq_silencer_password SecureString "$(gen_secret)"

# --- lobby ------------------------------------------------------------

echo "==> /silencer/lobby/*"

# One-shot Tailscale auth keys can't be regenerated programmatically —
# user mints them at https://login.tailscale.com/admin/settings/keys.
if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer/lobby/tailscale_auth_key >/dev/null 2>&1 \
   || [ "$OVERWRITE" -eq 1 ]; then
  put_param /silencer/lobby/tailscale_auth_key SecureString \
    "$(prompt_secret 'lobby Tailscale auth key (tag:server, reusable=no, ephemeral=no)')"
else
  put_param /silencer/lobby/tailscale_auth_key SecureString "(unused)"
fi

# --- admin ------------------------------------------------------------

echo "==> /silencer/admin/*"

if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer/admin/tailscale_auth_key >/dev/null 2>&1 \
   || [ "$OVERWRITE" -eq 1 ]; then
  put_param /silencer/admin/tailscale_auth_key SecureString \
    "$(prompt_secret 'admin Tailscale auth key (separate from lobby, same settings)')"
else
  put_param /silencer/admin/tailscale_auth_key SecureString "(unused)"
fi

put_param /silencer/admin/jwt_secret SecureString "$(gen_secret)"

if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer/admin/cloudflare_tunnel_token >/dev/null 2>&1 \
   || [ "$OVERWRITE" -eq 1 ]; then
  put_param /silencer/admin/cloudflare_tunnel_token SecureString \
    "$(prompt_secret 'Cloudflare Tunnel token (Zero Trust > Networks > Tunnels)')"
else
  put_param /silencer/admin/cloudflare_tunnel_token SecureString "(unused)"
fi

# Optional. The admin-api treats empty GITHUB_TOKEN as "skip GitHub
# backup commits" — local archives still get written. Seed an empty
# string if the operator declines so the IAM role can still read it
# without a 404 at fetch time.
if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer/admin/github_backup_token >/dev/null 2>&1; then
  printf '  GitHub PAT for Mongo backup commits (repo scope) — leave blank to disable: ' >&2
  IFS= read -r token
  put_param /silencer/admin/github_backup_token SecureString "${token:-}"
fi

# --- staging ----------------------------------------------------------
# Single-box staging stack. Distinct passwords + JWT secret + Tailscale
# auth key from prod so a leaked staging value can't reach into prod.
# SSH pubkeys (/silencer/shared/*) and the GHCR pull token
# (/silencer/admin/ghcr_pull_token) are reused from prod above.

echo "==> /silencer-staging/*"

put_param /silencer-staging/mongo_silencer_password   SecureString "$(gen_secret)"
put_param /silencer-staging/lavinmq_silencer_password SecureString "$(gen_secret)"
put_param /silencer-staging/jwt_secret                SecureString "$(gen_secret)"

if ! aws ssm get-parameter --region "$REGION" \
       --name /silencer-staging/tailscale_auth_key >/dev/null 2>&1 \
   || [ "$OVERWRITE" -eq 1 ]; then
  put_param /silencer-staging/tailscale_auth_key SecureString \
    "$(prompt_secret 'staging Tailscale auth key (tag:server, reusable=no, ephemeral=no)')"
else
  put_param /silencer-staging/tailscale_auth_key SecureString "(unused)"
fi

echo
echo "Done. Verify with:"
echo "  aws ssm get-parameters-by-path --region $REGION --path /silencer --recursive --query 'Parameters[].Name'"
echo "  aws ssm get-parameters-by-path --region $REGION --path /silencer-staging --recursive --query 'Parameters[].Name'"
