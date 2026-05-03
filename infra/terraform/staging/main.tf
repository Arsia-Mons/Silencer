terraform {
  required_version = ">= 1.14.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
  default_tags {
    tags = {
      Project   = var.project_name
      ManagedBy = "terraform"
      Stage     = "staging"
    }
  }
}

data "aws_vpc" "default" {
  default = true
}

data "aws_subnets" "default" {
  filter {
    name   = "vpc-id"
    values = [data.aws_vpc.default.id]
  }
}

data "aws_subnet" "selected" {
  id = data.aws_subnets.default.ids[0]
}

data "aws_ami" "ubuntu_arm64" {
  most_recent = true
  owners      = ["099720109477"] # Canonical

  filter {
    name   = "name"
    values = ["ubuntu/images/hvm-ssd-gp3/ubuntu-noble-24.04-arm64-server-*"]
  }

  filter {
    name   = "virtualization-type"
    values = ["hvm"]
  }
}

# Reuse the same SSH admin pubkey prod uses — staging is a single-box
# variant of the same fleet, not a separate ownership domain.
resource "aws_key_pair" "admin" {
  key_name   = "${var.project_name}-admin"
  public_key = data.aws_ssm_parameter.ssh_admin_pubkey.value
}

resource "aws_security_group" "staging" {
  name        = var.project_name
  description = "Silencer staging single-box (lobby + dedicated servers + admin-api + admin-web + Mongo + LavinMQ)"
  vpc_id      = data.aws_vpc.default.id

  ingress {
    description = "SSH (break-glass; day-to-day via Tailscale)"
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = [var.ssh_allowed_cidr]
  }

  ingress {
    description = "Lobby TCP"
    from_port   = 517
    to_port     = 517
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  ingress {
    description = "Lobby UDP (dedicated-server heartbeats)"
    from_port   = 517
    to_port     = 517
    protocol    = "udp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  ingress {
    description = "Dedicated servers (client-to-server UDP, ephemeral range)"
    from_port   = 30000
    to_port     = 61000
    protocol    = "udp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # admin-api (:24080) and admin-web (:24000) are NOT exposed to the
  # public internet. They bind 0.0.0.0 inside the box, but the SG only
  # opens the lobby ports. Reach them via Tailscale.

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

resource "aws_eip" "staging" {
  domain = "vpc"
  tags = {
    Name = var.project_name
  }
}

resource "aws_instance" "staging" {
  ami                    = data.aws_ami.ubuntu_arm64.id
  instance_type          = var.instance_type
  subnet_id              = data.aws_subnets.default.ids[0]
  key_name               = aws_key_pair.admin.key_name
  vpc_security_group_ids = [aws_security_group.staging.id]
  iam_instance_profile   = aws_iam_instance_profile.staging.name

  user_data = templatefile("${path.module}/cloud-init-staging.yaml.tftpl", {
    aws_region            = var.aws_region
    public_ip             = aws_eip.staging.public_ip
    tailscale_hostname    = var.tailscale_hostname
    deploy_ssh_public_key = data.aws_ssm_parameter.deploy_ssh_pubkey.value
    tailscale_auth_key    = data.aws_ssm_parameter.tailscale_auth_key.value
    admin_image_admin_api = var.admin_image_admin_api
    admin_image_admin_web = var.admin_image_admin_web
  })

  root_block_device {
    volume_size = var.root_volume_size
    volume_type = "gp3"
    encrypted   = true
  }

  tags = {
    Name = var.project_name
  }

  # Same posture as prod — re-running terraform apply must not pick up
  # a newer AMI or notice user_data drift and force-replace the box.
  # Explicit `terraform taint` is the only path to a re-bootstrap.
  lifecycle {
    ignore_changes = [ami, user_data]
  }
}

resource "aws_eip_association" "staging" {
  instance_id   = aws_instance.staging.id
  allocation_id = aws_eip.staging.id
}

resource "aws_route53_record" "staging" {
  count   = var.route53_zone_id != "" && var.domain_name != "" ? 1 : 0
  zone_id = var.route53_zone_id
  name    = var.domain_name
  type    = "A"
  ttl     = 300
  records = [aws_eip.staging.public_ip]
}
