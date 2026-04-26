output "lobby_ip" {
  description = "Elastic IP of the lobby server"
  value       = aws_eip.lobby.public_ip
}

output "lobby_host" {
  description = "Host clients should connect to (domain_name if set, otherwise the EIP)"
  value       = var.domain_name != "" ? var.domain_name : aws_eip.lobby.public_ip
}

output "ssh_command" {
  description = "SSH into the instance"
  value       = "ssh ubuntu@${aws_eip.lobby.public_ip}"
}

output "instance_id" {
  value = aws_instance.lobby.id
}

# -------------------------------------------------------------------
# Admin / data box outputs
# -------------------------------------------------------------------

output "admin_public_ip" {
  description = "Public EIP of the admin/data box (break-glass SSH only — day-to-day reach is via Tailscale)"
  value       = aws_eip.admin.public_ip
}

output "admin_private_ip" {
  description = "VPC private IP — what lobby's MONGO_URL / AMQP_URL resolve to via the internal Route 53 zone"
  value       = aws_instance.admin.private_ip
}

output "admin_instance_id" {
  value = aws_instance.admin.id
}

output "admin_tailscale_host" {
  description = "Tailscale MagicDNS hostname GitHub Actions deploys to"
  value       = var.admin_tailscale_hostname
}

output "internal_zone_name" {
  description = "Private Route 53 zone holding lobby.<zone> + admin.<zone>"
  value       = aws_route53_zone.internal.name
}

output "internal_zone_id" {
  value = aws_route53_zone.internal.zone_id
}
