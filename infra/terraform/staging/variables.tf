variable "aws_region" {
  description = "AWS region. Defaults to the same region as prod so the SSM params seeded once are reachable from both stacks."
  type        = string
  default     = "us-west-1"
}

variable "project_name" {
  description = "Resource-name prefix. Distinct from prod's `silencer` so SGs / EIPs / IAM roles don't collide."
  type        = string
  default     = "silencer-staging"
}

variable "instance_type" {
  description = "EC2 instance type. t4g.small is the floor — t4g.micro is too tight for mongod + lavinmq + lobby + dedicated-server + admin-api + admin-web on one box."
  type        = string
  default     = "t4g.small"
}

variable "ssh_allowed_cidr" {
  description = "CIDR block allowed to SSH (break-glass). Day-to-day SSH for humans + GH Actions goes via Tailscale."
  type        = string
  default     = "0.0.0.0/0"
}

variable "domain_name" {
  description = "DNS name for the staging lobby (e.g. staging.example.com). Empty = use the EIP directly. Devs build clients with cmake -DSILENCER_LOBBY_HOST=<this>."
  type        = string
  default     = ""
}

variable "route53_zone_id" {
  description = "Route 53 hosted zone ID for domain_name. Empty = don't manage DNS here."
  type        = string
  default     = ""
}

variable "root_volume_size" {
  description = "Root volume in GB. Holds OS, docker engine, container images, app binaries, mongod data, lavinmq data, lobby.json, and shared/assets — staging has no separate stateful EBS."
  type        = number
  default     = 16
}

variable "tailscale_hostname" {
  description = "Tailscale MagicDNS hostname for the staging box. GitHub Actions deploys to ubuntu@<this>; devs hit http://<this>:24000 for the admin dashboard."
  type        = string
  default     = "silencer-staging"
}

variable "admin_image_admin_api" {
  description = "Initial OCI image ref for silencer-admin-api on first boot. Empty = unit crash-loops quietly until the deploy workflow writes /etc/silencer/admin-api.image."
  type        = string
  default     = ""
}

variable "admin_image_admin_web" {
  description = "Initial OCI image ref for silencer-admin-web on first boot. See admin_image_admin_api."
  type        = string
  default     = ""
}
