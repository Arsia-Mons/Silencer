output "staging_lobby_ip" {
  description = "Elastic IP of the retired staging box."
  value       = aws_eip.staging.public_ip
}

output "staging_host" {
  description = "Retired staging host: domain_name if set, otherwise the EIP."
  value       = var.domain_name != "" ? var.domain_name : aws_eip.staging.public_ip
}

output "staging_ssh_command" {
  description = "Break-glass SSH for decommission checks."
  value       = "ssh ubuntu@${aws_eip.staging.public_ip}"
}

output "staging_instance_id" {
  value = aws_instance.staging.id
}

output "staging_tailscale_host" {
  description = "Retired Tailscale MagicDNS name."
  value       = var.tailscale_hostname
}
