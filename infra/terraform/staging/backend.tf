terraform {
  backend "s3" {
    # Values supplied via: terraform init -backend-config=backend.hcl
    # See backend.hcl.example for the shape. backend.hcl is gitignored.
    # Uses the same S3 bucket + DynamoDB lock table that prod's bootstrap
    # created — only the state-file `key` differs (silencer-staging.tfstate).
    encrypt = true
  }
}
