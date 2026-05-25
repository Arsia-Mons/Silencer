variable "aws_region" {
  description = "AWS region for the retired staging stack."
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
  description = "DNS name formerly used by the staging lobby. Empty = no DNS record."
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
  description = "Tailscale MagicDNS hostname formerly used by the staging box."
  type        = string
  default     = "silencer-staging"
}

variable "admin_image_admin_api" {
  description = "Retired initial OCI image ref for silencer-admin-api."
  type        = string
  default     = ""
}

variable "admin_image_admin_web" {
  description = "Retired initial OCI image ref for silencer-admin-web."
  type        = string
  default     = ""
}
