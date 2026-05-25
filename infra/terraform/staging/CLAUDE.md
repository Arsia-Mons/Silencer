# infra/terraform/staging — retired smoke-test stack

This module is not an active environment anymore. It is retained only
as the canonical Terraform destroy target for the old single-box
smoke-test stack in `silencer/staging.tfstate`.

Do not add new resources here. Do not run `terraform apply` or
`terraform destroy` here without explicit operator approval.

## Decommission Flow

Do the cleanup in this order so the Terraform state can still drive
the AWS resource deletion:

```sh
terraform init -backend-config=backend.hcl
terraform plan -destroy
```

Review the destroy plan. Only after explicit approval, run:

```sh
terraform destroy
```

Then verify Terraform has no retained resources:

```sh
terraform state list
```

The command should print nothing. If it still lists resources, stop
and resolve those before deleting any state files.

After the empty state is confirmed, remove the non-Terraform staging
artifacts:

```sh
AWS_REGION="${AWS_REGION:-us-west-1}"
params=$(aws ssm get-parameters-by-path \
  --region "$AWS_REGION" \
  --path /silencer-staging \
  --recursive \
  --query 'Parameters[].Name' \
  --output text)
if [ -n "$params" ] && [ "$params" != "None" ]; then
  printf '%s\n' $params |
    xargs -n10 aws ssm delete-parameters --region "$AWS_REGION" --names
fi
```

Delete the retired backend object only after the empty state is
confirmed. Use the real bucket and key from `backend.hcl`:

```sh
aws s3 rm s3://<backend-bucket>/silencer/staging.tfstate
```

Also remove any stale GitHub repo variables that were only used by
the deleted deploy workflow:

```sh
gh variable delete STAGING_DEPLOY_HOST || true
gh variable delete STAGING_LOBBY_HOST || true
gh variable delete STAGING_LOBBY_PUBLIC_IP || true
```

After those steps are complete, delete this directory in a follow-up
change. The active deployment workflow and SSM seeding for this stack
have already been removed.

## Layout

- `main.tf` — retired EC2, EIP, security group, and optional Route 53
  record definitions.
- `iam.tf` — retired instance role and instance profile definitions.
- `ssm.tf` — apply-time SSM data sources used by the retired stack.
- `cloud-init-staging.yaml.tftpl` — retired single-box bootstrap.
- `backend.tf` + `backend.hcl.example` — remote-state backend shape;
  the state key is `silencer/staging.tfstate`.
