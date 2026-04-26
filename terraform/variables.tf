variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "us-west-1"
}

variable "project_name" {
  description = "Used for naming and tagging"
  type        = string
  default     = "zsilencer"
}

variable "instance_type" {
  description = "EC2 instance type. t4g.* is ARM64 (Graviton), cheaper per CPU than x86."
  type        = string
  default     = "t4g.small"
}

variable "ssh_public_key" {
  description = "SSH public key granted admin access. Also used by GitHub Actions for deploys."
  type        = string
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

variable "tailscale_auth_key" {
  description = "One-time pre-authorized Tailscale auth key tagged tag:server. Generate at https://login.tailscale.com/admin/settings/keys (reusable=no, ephemeral=no, pre-approved, tag:server). Needed only on instance create/replace; cloud-init consumes it once."
  type        = string
  sensitive   = true
}

variable "tailscale_hostname" {
  description = "Tailscale MagicDNS hostname for the lobby. GitHub Actions connects to ubuntu@<this>."
  type        = string
  default     = "silencer"
}

variable "deploy_ssh_public_key" {
  description = "Public half of the SSH keypair GitHub Actions uses to deploy. The private half lives in the DEPLOY_SSH_KEY repo secret. Appended to ubuntu's authorized_keys on top of ssh_public_key."
  type        = string
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

variable "admin_tailscale_auth_key" {
  description = "One-time pre-authorized Tailscale auth key tagged tag:server, separate from the lobby's. Generate at https://login.tailscale.com/admin/settings/keys (reusable=no, ephemeral=no, pre-approved, tag:server). Consumed once on cloud-init."
  type        = string
  sensitive   = true
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

# Application credentials. All sensitive. Provisioned by cloud-init into
# /etc/silencer/*.env (mode 0600). Rotation is a Terraform variable change
# + apply (which replaces /etc/silencer/*.env in place; restart the
# affected systemd unit to pick up the new value).

variable "mongo_silencer_password" {
  description = "Password for the silencer Mongo user (used by lobby's mongosync + admin-api's mongoose connection). Created by cloud-init on first boot."
  type        = string
  sensitive   = true
}

variable "lavinmq_silencer_password" {
  description = "Password for the silencer LavinMQ user (used by lobby's AMQP publisher + admin-api's amqplib consumer). Created by cloud-init on first boot."
  type        = string
  sensitive   = true
}

variable "jwt_secret" {
  description = "Secret for signing admin-api JWTs. Rotation invalidates all existing tokens — every admin and player has to re-log in."
  type        = string
  sensitive   = true
}

variable "github_backup_token" {
  description = "PAT with `repo` scope for committing Mongo backups to the github_backup_repo. Empty disables GitHub-backup commits (local archives still written)."
  type        = string
  sensitive   = true
  default     = ""
}

variable "github_backup_repo" {
  description = "owner/repo for the Mongo backup commits. Default matches the existing kristiandelay design."
  type        = string
  default     = "Arsia-Mons/silencer-mongo-backup"
}

variable "cloudflare_tunnel_token" {
  description = "Cloudflare Tunnel token. Generate by creating a tunnel in Zero Trust → Networks → Tunnels → Cloudflared. The tunnel's Public Hostname config (set in the CF dashboard, not here) routes admin.arsiamons.com paths: /api/* and /socket.io/* → http://localhost:24080, everything else → http://localhost:24000."
  type        = string
  sensitive   = true
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
