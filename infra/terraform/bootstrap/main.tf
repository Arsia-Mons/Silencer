// One-time setup for the remote state backend used by the main Terraform module.
// This module uses LOCAL state itself (chicken-and-egg: you need S3 to exist
// before you can store state in it). Run `terraform apply` here once, then
// configure the parent module to use the bucket/table it creates.

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
      Project   = "silencer"
      ManagedBy = "terraform-bootstrap"
      Purpose   = "remote-state"
    }
  }
}

resource "aws_s3_bucket" "tfstate" {
  bucket = var.state_bucket_name
}

resource "aws_s3_bucket_versioning" "tfstate" {
  bucket = aws_s3_bucket.tfstate.id
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "tfstate" {
  bucket = aws_s3_bucket.tfstate.id
  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

resource "aws_s3_bucket_public_access_block" "tfstate" {
  bucket                  = aws_s3_bucket.tfstate.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_dynamodb_table" "tfstate_lock" {
  name         = var.state_lock_table_name
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "LockID"

  attribute {
    name = "LockID"
    type = "S"
  }
}

output "backend_hcl" {
  description = "Paste into ../backend.hcl (gitignored)"
  value       = <<-EOT
    bucket         = "${aws_s3_bucket.tfstate.bucket}"
    key            = "silencer/terraform.tfstate"
    region         = "${var.aws_region}"
    dynamodb_table = "${aws_dynamodb_table.tfstate_lock.name}"
  EOT
}
