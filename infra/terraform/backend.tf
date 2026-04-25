terraform {
  backend "s3" {
    # Values supplied via: terraform init -backend-config=backend.hcl
    # See backend.hcl.example for the shape. backend.hcl is gitignored.
    encrypt = true
  }
}
