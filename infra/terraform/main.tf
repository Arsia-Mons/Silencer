terraform {
  required_version = ">= 1.6.0"
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

resource "aws_key_pair" "admin" {
  key_name   = "${var.project_name}-admin"
  public_key = var.ssh_public_key
}

resource "aws_security_group" "lobby" {
  name        = "${var.project_name}-lobby"
  description = "Silencer lobby + dedicated servers"
  vpc_id      = data.aws_vpc.default.id

  ingress {
    description = "SSH"
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

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

resource "aws_eip" "lobby" {
  domain = "vpc"
  tags = {
    Name = "${var.project_name}-lobby"
  }
}

resource "aws_instance" "lobby" {
  ami                    = data.aws_ami.ubuntu_arm64.id
  instance_type          = var.instance_type
  subnet_id              = data.aws_subnets.default.ids[0]
  key_name               = aws_key_pair.admin.key_name
  vpc_security_group_ids = [aws_security_group.lobby.id]

  user_data = templatefile("${path.module}/cloud-init.yaml.tftpl", {
    project_name              = var.project_name
    lobby_version             = var.lobby_version_string
    public_hostname           = var.domain_name != "" ? var.domain_name : aws_eip.lobby.public_ip
    tailscale_auth_key        = var.tailscale_auth_key
    tailscale_hostname        = var.tailscale_hostname
    deploy_ssh_public_key     = var.deploy_ssh_public_key
    internal_zone_name        = var.internal_zone_name
    mongo_silencer_password   = var.mongo_silencer_password
    lavinmq_silencer_password = var.lavinmq_silencer_password
  })

  root_block_device {
    volume_size = 20
    volume_type = "gp3"
    encrypted   = true
  }

  tags = {
    Name = "${var.project_name}-lobby"
  }

  # Re-running terraform apply must not pick up a newer AMI or notice
  # credential changes in user_data — both would force-replace the lobby
  # instance and kill active games. Explicit `terraform taint` is the
  # only path to a re-bootstrap. Same posture as the admin instance.
  lifecycle {
    ignore_changes = [ami, user_data]
  }
}

resource "aws_eip_association" "lobby" {
  instance_id   = aws_instance.lobby.id
  allocation_id = aws_eip.lobby.id
}

resource "aws_ebs_volume" "data" {
  availability_zone = data.aws_subnet.selected.availability_zone
  size              = var.ebs_volume_size
  type              = "gp3"
  encrypted         = true

  tags = {
    Name = "${var.project_name}-data"
  }
}

resource "aws_volume_attachment" "data" {
  device_name = "/dev/sdf"
  volume_id   = aws_ebs_volume.data.id
  instance_id = aws_instance.lobby.id
}

resource "aws_route53_record" "lobby" {
  count   = var.route53_zone_id != "" && var.domain_name != "" ? 1 : 0
  zone_id = var.route53_zone_id
  name    = var.domain_name
  type    = "A"
  ttl     = 300
  records = [aws_eip.lobby.public_ip]
}
