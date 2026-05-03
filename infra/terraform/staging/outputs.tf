output "staging_lobby_ip" {
  description = "Elastic IP of the staging box. Set this as `vars.STAGING_LOBBY_PUBLIC_IP` in GitHub repo settings — deploy-staging.yml bakes it into the dedicated server's -public-addr (mandatory; the C++ join path resolves with inet_addr())."
  value       = aws_eip.staging.public_ip
}

output "staging_host" {
  description = "Host devs build the client against — domain_name if set, otherwise the EIP. Use with: cmake -DSILENCER_LOBBY_HOST=<this>"
  value       = var.domain_name != "" ? var.domain_name : aws_eip.staging.public_ip
}

output "staging_ssh_command" {
  description = "Break-glass SSH (Tailscale-down). Day-to-day: ssh ubuntu@<tailscale_hostname>"
  value       = "ssh ubuntu@${aws_eip.staging.public_ip}"
}

output "staging_instance_id" {
  value = aws_instance.staging.id
}

output "staging_tailscale_host" {
  description = "Tailscale MagicDNS name. Devs hit http://<this>:24000 for the admin dashboard."
  value       = var.tailscale_hostname
}
