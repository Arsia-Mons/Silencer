# Single instance role for the staging box. Reads runtime secrets from
# /silencer-staging/* (and the shared Mongo/LavinMQ creds — same pattern
# as prod, since seeding two parallel passwords adds no security and one
# more thing to rotate). GHCR pull token is shared with prod (read-only,
# repo-scoped).
#
# Same KMS posture as prod: the AWS-managed `alias/aws/ssm` key grants
# implicit decrypt to anyone with `ssm:GetParameter` on the param.

data "aws_caller_identity" "current" {}

locals {
  ssm_arn_prefix = "arn:aws:ssm:${var.aws_region}:${data.aws_caller_identity.current.account_id}:parameter"

  staging_runtime_param_arns = [
    "${local.ssm_arn_prefix}/silencer-staging/mongo_silencer_password",
    "${local.ssm_arn_prefix}/silencer-staging/lavinmq_silencer_password",
    "${local.ssm_arn_prefix}/silencer-staging/jwt_secret",
    "${local.ssm_arn_prefix}/silencer/admin/ghcr_pull_token",
  ]
}

resource "aws_iam_role" "staging" {
  name = var.project_name
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect    = "Allow"
      Action    = "sts:AssumeRole"
      Principal = { Service = "ec2.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy" "staging_ssm_read" {
  name = "ssm-read"
  role = aws_iam_role.staging.id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect   = "Allow"
      Action   = ["ssm:GetParameter", "ssm:GetParameters"]
      Resource = local.staging_runtime_param_arns
    }]
  })
}

resource "aws_iam_instance_profile" "staging" {
  name = var.project_name
  role = aws_iam_role.staging.name
}
