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
