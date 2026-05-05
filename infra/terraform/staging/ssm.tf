# Apply-time secret reads. Templated into resources at plan time, so
# they end up in tfstate (encrypted at rest in the S3 backend). Same
# trade-off as prod: only one-shot values live here; rotating creds are
# fetched at runtime by the EC2 instance via its IAM role.
#
# Seed the params with infra/scripts/seed-ssm.sh.

# Shared with prod — same human, same GH Actions deploy key.
data "aws_ssm_parameter" "ssh_admin_pubkey" {
  name = "/silencer/shared/ssh_admin_pubkey"
}

data "aws_ssm_parameter" "deploy_ssh_pubkey" {
  name = "/silencer/shared/deploy_ssh_pubkey"
}

# Staging-specific Tailscale auth key — distinct one-shot from prod's
# so a leaked staging key can't enroll into prod's tailnet device list.
data "aws_ssm_parameter" "tailscale_auth_key" {
  name = "/silencer-staging/tailscale_auth_key"
}
