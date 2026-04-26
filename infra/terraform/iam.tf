# Instance IAM roles + profiles. Grant each box read access to its own
# subset of /silencer/* SSM parameters. Cloud-init (and operators
# rotating values) shell out to `aws ssm get-parameter --with-decryption`
# using the role's creds.
#
# Resource ARNs are constructed from the parameter path rather than from
# `data "aws_ssm_parameter"` so the secret values themselves never enter
# tfstate. Only ARNs do, which are non-sensitive.
#
# KMS: no explicit `kms:Decrypt` statement is needed because the params
# are encrypted with the AWS-managed `alias/aws/ssm` key, which grants
# implicit decrypt to anyone with `ssm:GetParameter` on the param. If
# you migrate to a customer-managed key you'll need to add a kms:Decrypt
# statement scoped to that key's ARN.

data "aws_caller_identity" "current" {}

locals {
  ssm_arn_prefix = "arn:aws:ssm:${var.aws_region}:${data.aws_caller_identity.current.account_id}:parameter"

  # Lobby reads only Mongo + LavinMQ shared creds at runtime. Tailscale
  # auth key is templated into user_data via a data source (one-shot,
  # consumed by cloud-init at first boot).
  lobby_runtime_param_arns = [
    "${local.ssm_arn_prefix}/silencer/shared/mongo_silencer_password",
    "${local.ssm_arn_prefix}/silencer/shared/lavinmq_silencer_password",
  ]

  # Admin needs the shared creds for its app + the bootstrap-time user
  # creates against mongod / lavinmq, plus its own admin-only secrets.
  admin_runtime_param_arns = [
    "${local.ssm_arn_prefix}/silencer/shared/mongo_silencer_password",
    "${local.ssm_arn_prefix}/silencer/shared/lavinmq_silencer_password",
    "${local.ssm_arn_prefix}/silencer/admin/jwt_secret",
    "${local.ssm_arn_prefix}/silencer/admin/github_backup_token",
  ]
}

# --- Lobby --------------------------------------------------------------

resource "aws_iam_role" "lobby" {
  name = "${var.project_name}-lobby"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect    = "Allow"
      Action    = "sts:AssumeRole"
      Principal = { Service = "ec2.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy" "lobby_ssm_read" {
  name = "ssm-read"
  role = aws_iam_role.lobby.id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect   = "Allow"
      Action   = ["ssm:GetParameter", "ssm:GetParameters"]
      Resource = local.lobby_runtime_param_arns
    }]
  })
}

resource "aws_iam_instance_profile" "lobby" {
  name = "${var.project_name}-lobby"
  role = aws_iam_role.lobby.name
}

# --- Admin --------------------------------------------------------------

resource "aws_iam_role" "admin" {
  name = "${var.project_name}-admin"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect    = "Allow"
      Action    = "sts:AssumeRole"
      Principal = { Service = "ec2.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy" "admin_ssm_read" {
  name = "ssm-read"
  role = aws_iam_role.admin.id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect   = "Allow"
      Action   = ["ssm:GetParameter", "ssm:GetParameters"]
      Resource = local.admin_runtime_param_arns
    }]
  })
}

resource "aws_iam_instance_profile" "admin" {
  name = "${var.project_name}-admin"
  role = aws_iam_role.admin.name
}
