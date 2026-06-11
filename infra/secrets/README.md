# infra/secrets

Encrypted secrets using [Ansible Vault](https://docs.ansible.com/ansible/latest/vault_guide/) (AES-256).

## Setup (one-time per machine)

Get the vault password from a team member (share via 1Password, Signal, etc. — never email/Slack), then:

```bash
echo "THE_VAULT_PASSWORD" > ~/.silencer-vault-pass
chmod 600 ~/.silencer-vault-pass
```

## View secrets

```bash
ansible-vault view infra/secrets/vault.yml --vault-password-file ~/.silencer-vault-pass
```

## Use the GitHub token with gh CLI

```bash
ansible-vault view infra/secrets/vault.yml --vault-password-file ~/.silencer-vault-pass \
  | grep github_token_kristiandelay \
  | awk '{print $2}' \
  | gh auth login --hostname github.com --with-token
```

## Edit / rotate a secret

```bash
ansible-vault edit infra/secrets/vault.yml --vault-password-file ~/.silencer-vault-pass
```

## What's in vault.yml

| Key | Description |
|-----|-------------|
| `github_token_kristiandelay` | GitHub PAT for `kristiandelay` — used to open PRs from the CLI |
