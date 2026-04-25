variable "aws_region" {
  type    = string
  default = "us-west-1"
}

variable "state_bucket_name" {
  description = "Globally unique S3 bucket name for Terraform remote state"
  type        = string
}

variable "state_lock_table_name" {
  type    = string
  default = "zsilencer-tfstate-lock"
}
