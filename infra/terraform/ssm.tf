# Apply-time secret reads. These values get templated into other
# resources at `terraform plan` (e.g. into EC2 user_data, the AWS key
# pair material, etc.) so we have to fetch them as data sources here.
#
# Trade-off vs runtime-fetched secrets: data source values DO end up in
# tfstate (encrypted at rest in the S3 backend, but readable to anyone
# with `terraform state pull` rights). We accept that for one-shot
# values — Tailscale auth keys, the Cloudflare tunnel token, and SSH
# pubkeys (which aren't even sensitive). Anything that rotates with
# meaningful frequency (DB passwords, JWT signing key) is read at
# runtime by the EC2 instances themselves via their IAM role; see
# iam.tf and the `silencer-fetch-secrets` script in cloud-init.
#
# Seed the params with infra/scripts/seed-ssm.sh.

data "aws_ssm_parameter" "ssh_admin_pubkey" {
  name = "/silencer/shared/ssh_admin_pubkey"
}

data "aws_ssm_parameter" "deploy_ssh_pubkey" {
  name = "/silencer/shared/deploy_ssh_pubkey"
}

data "aws_ssm_parameter" "lobby_tailscale_auth_key" {
  name = "/silencer/lobby/tailscale_auth_key"
}

data "aws_ssm_parameter" "admin_tailscale_auth_key" {
  name = "/silencer/admin/tailscale_auth_key"
}

data "aws_ssm_parameter" "cloudflare_tunnel_token" {
  name = "/silencer/admin/cloudflare_tunnel_token"
}
