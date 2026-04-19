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
  description = "Required client version. Must match CMakeLists.txt. Empty string to accept any version."
  type        = string
  default     = "00022"
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
