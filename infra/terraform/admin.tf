# Silencer admin/data box — runs MongoDB, LavinMQ, the admin-api container,
# the admin-web container, and a Cloudflare tunnel. See
# docs/plans/2026-04-25-production-deployment-architecture.md for the
# rationale (one box, four independent systemd units, two stateful EBS
# volumes, no public ingress except via Cloudflare Tunnel).

resource "aws_security_group" "admin" {
  name        = "${var.project_name}-admin"
  description = "Silencer admin/data box (Mongo, LavinMQ, admin-api, admin-web, cloudflared)"
  vpc_id      = data.aws_vpc.default.id

  # Public-internet break-glass SSH (mirrors the lobby SG). Day-to-day SSH
  # for both humans and GitHub Actions goes through Tailscale.
  ingress {
    description = "SSH (break-glass)"
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = [var.ssh_allowed_cidr]
  }

  # Cloudflare Tunnel is outbound-only — no public ingress for HTTP/S.
  # Cross-SG ingress (Mongo / LavinMQ from lobby) is in separate
  # aws_vpc_security_group_ingress_rule resources below to avoid the
  # circular-dependency hazard of inline cross-SG rules.

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # Mongo and LavinMQ ingress from lobby are managed by separate
  # aws_vpc_security_group_ingress_rule resources to avoid circular deps.
  lifecycle {
    ignore_changes = [ingress]
  }
}

# Cross-SG rules. Defined as separate resources because admin SG <-> lobby
# SG reference each other and inline rules would create a cycle.

resource "aws_vpc_security_group_ingress_rule" "admin_mongo_from_lobby" {
  security_group_id            = aws_security_group.admin.id
  referenced_security_group_id = aws_security_group.lobby.id
  ip_protocol                  = "tcp"
  from_port                    = 27017
  to_port                      = 27017
  description                  = "MongoDB from lobby (mongosync)"
}

resource "aws_vpc_security_group_ingress_rule" "admin_lavinmq_from_lobby" {
  security_group_id            = aws_security_group.admin.id
  referenced_security_group_id = aws_security_group.lobby.id
  ip_protocol                  = "tcp"
  from_port                    = 5672
  to_port                      = 5672
  description                  = "LavinMQ AMQP from lobby (event publisher)"
}

resource "aws_vpc_security_group_ingress_rule" "lobby_playerauth_from_admin" {
  security_group_id            = aws_security_group.lobby.id
  referenced_security_group_id = aws_security_group.admin.id
  ip_protocol                  = "tcp"
  from_port                    = 15171
  to_port                      = 15171
  description                  = "Player-auth HTTP from admin-api (ban/delete + credential validation)"
}

resource "aws_eip" "admin" {
  domain = "vpc"
  tags = {
    Name = "${var.project_name}-admin"
  }
}

resource "aws_instance" "admin" {
  ami                    = data.aws_ami.ubuntu_arm64.id
  instance_type          = var.admin_instance_type
  subnet_id              = data.aws_subnets.default.ids[0]
  key_name               = aws_key_pair.admin.key_name
  vpc_security_group_ids = [aws_security_group.admin.id]
  iam_instance_profile   = aws_iam_instance_profile.admin.name

  user_data = templatefile("${path.module}/cloud-init-admin.yaml.tftpl", {
    aws_region               = var.aws_region
    deploy_ssh_public_key    = data.aws_ssm_parameter.deploy_ssh_pubkey.value
    admin_tailscale_auth_key = data.aws_ssm_parameter.admin_tailscale_auth_key.value
    admin_tailscale_hostname = var.admin_tailscale_hostname
    internal_zone_name       = var.internal_zone_name
    github_backup_repo       = var.github_backup_repo
    cloudflare_tunnel_token  = data.aws_ssm_parameter.cloudflare_tunnel_token.value
    admin_image_admin_api    = var.admin_image_admin_api
    admin_image_admin_web    = var.admin_image_admin_web
  })

  root_block_device {
    volume_size = var.admin_root_volume_size
    volume_type = "gp3"
    encrypted   = true
  }

  tags = {
    Name = "${var.project_name}-admin"
  }

  # Re-running terraform apply must not pick up a newer AMI and force a
  # replace — instance replacement re-runs cloud-init, and we want that
  # to happen explicitly via `terraform taint` only.
  lifecycle {
    ignore_changes = [ami, user_data]
  }
}

resource "aws_eip_association" "admin" {
  instance_id   = aws_instance.admin.id
  allocation_id = aws_eip.admin.id
}

# /var/lib/mongodb — independent EBS, prevent_destroy so a tainted
# instance leaves the data behind.
resource "aws_ebs_volume" "admin_mongo" {
  availability_zone = data.aws_subnet.selected.availability_zone
  size              = var.admin_mongo_volume_size
  type              = "gp3"
  encrypted         = true

  tags = {
    Name = "${var.project_name}-mongo"
  }

  lifecycle {
    prevent_destroy = true
  }
}

resource "aws_volume_attachment" "admin_mongo" {
  device_name = "/dev/sdf"
  volume_id   = aws_ebs_volume.admin_mongo.id
  instance_id = aws_instance.admin.id
}

# /var/lib/lavinmq — independent EBS, prevent_destroy.
resource "aws_ebs_volume" "admin_lavinmq" {
  availability_zone = data.aws_subnet.selected.availability_zone
  size              = var.admin_lavinmq_volume_size
  type              = "gp3"
  encrypted         = true

  tags = {
    Name = "${var.project_name}-lavinmq"
  }

  lifecycle {
    prevent_destroy = true
  }
}

resource "aws_volume_attachment" "admin_lavinmq" {
  device_name = "/dev/sdg"
  volume_id   = aws_ebs_volume.admin_lavinmq.id
  instance_id = aws_instance.admin.id
}
