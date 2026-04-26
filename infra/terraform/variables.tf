variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "us-west-1"
}

variable "project_name" {
  description = "Used for naming and tagging"
  type        = string
  default     = "silencer"
}

variable "instance_type" {
  description = "EC2 instance type. t4g.* is ARM64 (Graviton), cheaper per CPU than x86."
  type        = string
  default     = "t4g.small"
}

variable "ssh_allowed_cidr" {
  description = "CIDR block allowed to SSH. Set to your IP (e.g. 1.2.3.4/32), not 0.0.0.0/0."
  type        = string
  default     = "0.0.0.0/0"
}

variable "domain_name" {
  description = "DNS name clients use to reach the lobby (e.g. lobby.example.com). Empty = use the EIP directly."
  type        = string
  default     = ""
}

variable "route53_zone_id" {
  description = "Route 53 hosted zone ID for domain_name. Empty = don't manage DNS here."
  type        = string
  default     = ""
}

variable "ebs_volume_size" {
  description = "Size in GB of the data volume mounted at /var/lib/<project_name>. Holds lobby.json."
  type        = number
  default     = 8
}

variable "lobby_version_string" {
  description = "Required client version. Empty string (default) accepts any version — lets Release workflow tags drive client versions without needing an infrastructure rebuild. Set to a specific string like \"00024\" to enforce a pinned version (requires instance rebuild to take effect)."
  type        = string
  default     = ""
}

variable "tailscale_hostname" {
  description = "Tailscale MagicDNS hostname for the lobby. GitHub Actions connects to ubuntu@<this>."
  type        = string
  default     = "silencer"
}

# -------------------------------------------------------------------
# Admin / data box (Phase 1 — production deployment architecture)
# -------------------------------------------------------------------

variable "admin_instance_type" {
  description = "EC2 type for the admin/data box. Sizing rationale in docs/plans/2026-04-25-production-deployment-architecture.md (Phase 1 default t4g.small, ~1.7GB peak on ~1.85GB usable)."
  type        = string
  default     = "t4g.small"
}

variable "admin_root_volume_size" {
  description = "Root volume size in GB. Holds OS, docker engine, container images, app binaries, journald logs."
  type        = number
  default     = 16
}

variable "admin_mongo_volume_size" {
  description = "EBS volume size in GB for /var/lib/mongodb. Sized to absorb ~3-5GB of Event-collection growth at one year of Active traffic."
  type        = number
  default     = 10
}

variable "admin_lavinmq_volume_size" {
  description = "EBS volume size in GB for /var/lib/lavinmq. Sized to absorb a multi-day admin-api outage backlog without filling the volume."
  type        = number
  default     = 5
}

variable "admin_tailscale_hostname" {
  description = "Tailscale MagicDNS hostname for the admin/data box. GitHub Actions connects to ubuntu@<this> for deploys."
  type        = string
  default     = "silencer-admin"
}

variable "internal_zone_name" {
  description = "Private Route 53 zone shared by both boxes. Holds A records like lobby.<zone> and admin.<zone> so cross-service hostnames stay stable across instance replacement."
  type        = string
  default     = "silencer.internal"
}

variable "github_backup_repo" {
  description = "owner/repo for the Mongo backup commits. Default matches the existing kristiandelay design. Empty PAT (in SSM) disables commits, keeping local archives only."
  type        = string
  default     = "Arsia-Mons/silencer-mongo-backup"
}

variable "admin_image_admin_api" {
  description = "Initial OCI image ref for silencer-admin-api on first boot. Empty = unit crash-loops quietly until the deploy workflow writes /etc/silencer/admin-api.image. Subsequent deploys do NOT touch this variable; they update the file directly."
  type        = string
  default     = ""
}

variable "admin_image_admin_web" {
  description = "Initial OCI image ref for silencer-admin-web on first boot. See admin_image_admin_api."
  type        = string
  default     = ""
}

# -------------------------------------------------------------------
# Secrets — sourced from SSM Parameter Store, NOT this file.
# -------------------------------------------------------------------
# All values that used to live in terraform.tfvars now live under
# /silencer/* in SSM. Seed them once per AWS account with
# infra/scripts/seed-ssm.sh; teammates with IAM read access fetch from
# the same source. See infra/terraform/CLAUDE.md and docs/production.md
# for the full parameter list and rotation flow.
