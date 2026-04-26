# 1.5 — Data Lifecycle Manager: daily snapshots of the admin/data box's
# stateful EBS volumes, with a rolling retention window. Independent of
# any in-application backup (Mongo's own backup-to-GitHub still ships).
# This is the AWS-side defense-in-depth layer: even if the VM is
# replaced by mistake, the snapshots remain.
#
# DLM targets volumes by tag — both Mongo + LavinMQ EBS resources carry
# Name = "<project>-mongo" / "<project>-lavinmq", and we tag them with a
# DLM-specific selector here so the policy only sees the volumes it should
# back up (not the lobby's data volume, which has its own snapshot story
# managed elsewhere).

resource "aws_iam_role" "dlm_lifecycle" {
  name = "${var.project_name}-dlm-lifecycle"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect    = "Allow"
      Principal = { Service = "dlm.amazonaws.com" }
      Action    = "sts:AssumeRole"
    }]
  })
}

resource "aws_iam_role_policy_attachment" "dlm_lifecycle" {
  role       = aws_iam_role.dlm_lifecycle.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSDataLifecycleManagerServiceRole"
}

# Tag the two volumes with a DLM selector. Done here so the policy can
# match on a single tag without enumerating volumes.
resource "aws_ec2_tag" "dlm_admin_mongo" {
  resource_id = aws_ebs_volume.admin_mongo.id
  key         = "snapshot-policy"
  value       = "${var.project_name}-admin-daily"
}

resource "aws_ec2_tag" "dlm_admin_lavinmq" {
  resource_id = aws_ebs_volume.admin_lavinmq.id
  key         = "snapshot-policy"
  value       = "${var.project_name}-admin-daily"
}

resource "aws_dlm_lifecycle_policy" "admin_daily" {
  description        = "Silencer admin data box daily snapshots"
  execution_role_arn = aws_iam_role.dlm_lifecycle.arn
  state              = "ENABLED"

  policy_details {
    resource_types = ["VOLUME"]

    target_tags = {
      "snapshot-policy" = "${var.project_name}-admin-daily"
    }

    schedule {
      name = "daily-7d-retention"

      create_rule {
        interval      = 24
        interval_unit = "HOURS"
        times         = ["07:00"] # 07:00 UTC = quiet pre-EU/US window
      }

      retain_rule {
        count = 7
      }

      tags_to_add = {
        SnapshotCreator = "${var.project_name}-dlm"
      }

      copy_tags = true
    }
  }
}
