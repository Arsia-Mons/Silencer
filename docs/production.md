# Running your own zSILENCER lobby

This guide walks through setting up a zSILENCER lobby server — the
matchmaking service that lets players find each other and start
games. The original `lobby.zsilencer.com` is gone, so if you want to
play zSILENCER with other people today, you (or someone) has to run
one of these.

When you're done you'll have:

- One small AWS VM running the lobby 24/7 (around \$15/month).
- macOS and Windows client builds that connect to it.
- A pipeline that deploys new versions when you push a git tag.

## Before you start: clients are tied to one lobby

This is the one non-obvious thing about zSILENCER's setup, and it
shapes everything else. Worth reading before you commit 30 minutes to
the Terraform part.

A zSILENCER client has its lobby's address **hardcoded into the
binary at build time**. There's no setting or config file to point an
existing client at a different server. That means:

- Running your own lobby implies shipping your own client builds.
  You can't hand your players a release from the upstream repo — it'll
  try to talk to a server that doesn't exist anymore.
- If you later move the lobby to a different address, you have to
  build and distribute new clients.

The GitHub Actions setup below wires this together: you set one repo
variable (`LOBBY_HOST`) to your server's address, and every release
tag produces both the server-side binary *and* matching client builds
that point at it. So in practice you manage it as one unit — infra
plus clients, from the same commit.

A consequence worth planning for: put the lobby behind a domain name
(e.g. `lobby.example.com`) rather than a raw IP. That way you can
rebuild the server and get a new IP without having to re-issue client
binaries.

## What you need

- **An AWS account.** One region is enough. The Terraform defaults to
  `us-west-1`.
- **A Tailscale account.** Free tier is fine. Tailscale is what lets
  the GitHub Actions deploy job reach your server without you having
  to open SSH to the public internet.
- **A fork of this repo** on GitHub, with admin access (you'll be
  setting Secrets and Variables).
- **Optional but recommended:** a domain name you can point at the
  server. Route 53 integrates cleanly; anywhere else works too, you'll
  just manage the DNS record yourself.
- On your laptop: Terraform 1.6+, AWS CLI v2, and `ssh`.

## Setup

Six steps, about half an hour end-to-end. High level: set up
Terraform, build the server, hand GitHub Actions the credentials it
needs, then push a tag to deploy.

### 1. Set up Terraform's state storage (one-time)

Terraform keeps its state in an S3 bucket with a DynamoDB lock table.
There's a small `bootstrap/` module that creates those — run it once
per AWS account, then forget about it.

```bash
cd terraform/bootstrap
cp terraform.tfvars.example terraform.tfvars
# edit: pick a globally unique S3 bucket name
terraform init && terraform apply

cd ..
cp backend.hcl.example backend.hcl
# paste the bucket + table names from the bootstrap output
terraform init -backend-config=backend.hcl
```

### 2. Make two SSH keypairs

One for you, one for GitHub Actions. Ed25519 is fine.

```bash
ssh-keygen -t ed25519 -f ~/.ssh/zsilencer-admin   # yours
ssh-keygen -t ed25519 -f ~/.ssh/zsilencer-deploy  # GH Actions
```

### 3. Get a Tailscale auth key

In the Tailscale admin console, generate an **auth key** with:

- Reusable: no
- Ephemeral: no
- Pre-approved: yes
- Tag: `tag:server`

You'll also need `tag:server` to exist in your ACL. If you haven't
set one up, this minimal policy is enough — it lets machines tagged
`tag:server` (the server itself, and later the Actions runner) SSH
to each other over Tailscale:

```jsonc
{
  "tagOwners": { "tag:server": ["autogroup:admin"] },
  "acls": [
    { "action": "accept", "src": ["tag:server"], "dst": ["tag:server:22"] }
  ]
}
```

Copy the key. It's single-use — cloud-init consumes it when the
server first boots.

### 4. Create the server

```bash
cd terraform
cp terraform.tfvars.example terraform.tfvars
```

Fill in `terraform.tfvars`. The ones you must set:

- `ssh_public_key` — contents of `~/.ssh/zsilencer-admin.pub`.
- `deploy_ssh_public_key` — contents of `~/.ssh/zsilencer-deploy.pub`.
- `ssh_allowed_cidr` — your laptop's IP, as a `/32`. This is just for
  emergency access; normal admin goes through Tailscale.
- `tailscale_auth_key` — from step 3.
- `tailscale_hostname` — whatever name you want on the tailnet (e.g.
  `silencer`).
- `domain_name` and `route53_zone_id` — if you have a domain.

Then:

```bash
terraform apply
```

This takes a few minutes and creates the VM, a static IP, a separate
data disk for user accounts, and (optionally) a DNS record. The VM
installs and configures itself using cloud-init — give it another
2–3 minutes after Terraform finishes before expecting the lobby
service to come up. It won't actually work yet because there are no
binaries on it; that's what step 6 fixes.

### 5. Hand credentials to GitHub Actions

In your fork: **Settings → Secrets and variables → Actions.**

Variables (these are visible, not secret):

| Name          | Value                                                            |
|---------------|------------------------------------------------------------------|
| `LOBBY_HOST`  | The address clients connect to. **Gets baked into every client binary.** Use your domain if you have one, otherwise the static IP Terraform printed. |
| `DEPLOY_HOST` | The Tailscale hostname you picked in step 4 (e.g. `silencer`).   |

Secrets:

| Name             | Value                                                                       |
|------------------|-----------------------------------------------------------------------------|
| `DEPLOY_SSH_KEY` | Private half of `~/.ssh/zsilencer-deploy` (entire file, including headers). |
| `TS_AUTHKEY`     | A **second** Tailscale auth key, generated the same way as step 3 except set reusable=yes and ephemeral=yes. Actions runners are short-lived, so they need their own throwaway keys. |

### 6. Deploy

```bash
git tag v0.0.1
git push --tags
```

Two GitHub Actions workflows run, independently:

- **Deploy** (`deploy.yml`) — builds the Linux-side binaries (lobby
  server + dedicated game server), scps them onto your VM over
  Tailscale, and restarts the service.
- **Release** (`release.yml`) — builds the macOS and Windows clients
  (pointed at `LOBBY_HOST`) and attaches them to a GitHub Release.

Watch both under the Actions tab. Once Deploy finishes, check that
the lobby is listening:

```bash
nc -vz <your-lobby-host> 517
```

Download the client from the GitHub Release, launch it, register an
account — you should be looking at the lobby.

## Day-to-day

**Logging into the server.** Over Tailscale if you have it installed
locally; otherwise the static IP works because you allowlisted your
laptop.

```bash
ssh ubuntu@<tailscale-hostname>
ssh ubuntu@<static-ip>
```

**Checking the service.**

```bash
sudo systemctl status zsilencer-lobby
sudo journalctl -u zsilencer-lobby -f           # tail the logs
sudo journalctl -u zsilencer-lobby --since "1 hour ago"
```

**Restarting.** Note that this kills any games in progress — their
dedicated-server subprocesses are children of the lobby, so they die
with it.

```bash
sudo systemctl restart zsilencer-lobby
```

**Rolling back a bad deploy.** Every release is kept in
`/opt/zsilencer/releases/<sha>/`, and `current` is a symlink to the
one that's running. The last three releases stay on disk. To roll
back, point the symlink at an older one and restart:

```bash
ls -lt /opt/zsilencer/releases
sudo ln -sfn /opt/zsilencer/releases/<older-sha> /opt/zsilencer/current
sudo systemctl restart zsilencer-lobby
```

**Iterating fast while debugging.** `scripts/fastdeploy.sh` rsyncs
your working tree onto the server, compiles there, swaps the binary,
restarts. Useful when chasing something that only reproduces on the
real server. Don't use it for real releases — it bypasses CI.

## State and backups

### Primary store — lobby.json

The authoritative player store is a single file:
`/var/lib/zsilencer/lobby.json`. It holds user accounts (username +
SHA-1-hashed password) and per-agency stats. No chat logs, no game
history, nothing else. All reads and writes go through the lobby's
in-memory store, which is flushed here atomically on every change.

That file lives on a separate EBS volume from the VM itself, so you
can rebuild the VM (e.g. `terraform taint aws_instance.lobby &&
terraform apply`) without losing account data.

Restoring from a copy of `lobby.json` is still a simple file replace:

```bash
sudo systemctl stop zsilencer-lobby
sudo cp lobby-<date>.json /var/lib/zsilencer/lobby.json
sudo chown zsilencer:zsilencer /var/lib/zsilencer/lobby.json
sudo systemctl start zsilencer-lobby
```

### MongoDB mirror

MongoDB is a **read mirror** of `lobby.json`, not the primary store.
`mongosync.go` asynchronously upserts every player mutation (register,
ban, upgrade, delete) to the `players` collection. `SyncAll()` runs at
lobby startup to bring the mirror up to date. This lets the admin
dashboard query rich player data without touching `lobby.json` directly.
Password hashes are never written to MongoDB.

If the MongoDB container is unavailable, the lobby continues to serve
clients normally — sync failures are logged and discarded.

### MongoDB backups

Automated database backups are managed through the admin dashboard at
`/health`. No manual configuration is required beyond setting
`GITHUB_TOKEN` and `GITHUB_BACKUP_REPO` in your `.env`.

- **Auto-backup**: runs every 6 hours (configurable via `BACKUP_CRON`).
  Keeps the last 10 local archives in the `backup-data` Docker volume.
- **Manual backup**: click **BACKUP NOW** on the `/health` page; the
  panel polls status and shows the result when done.
- **GitHub backup**: each backup commits `zsilencer.archive.gz` to
  `Arsia-Mons/silencer-mongo-backup`. Git history acts as version history —
  browse or restore any past snapshot from `github.com/<repo>/commits`.
  No GitHub Releases needed.

The AWS Data Lifecycle Manager snapshot approach still works as a
belt-and-suspenders backup for the EBS data volume itself.

If you ever outgrow a hobby-scale player count, swap
`server/store.go` to SQLite or Postgres before worrying about
snapshot frequency.

## Forcing old clients to upgrade

During the lobby handshake, the client tells the server what version
it is. The server can reject clients whose version doesn't match.

By default this check is **off** — `lobby_version_string = ""`
accepts any client. That's intentional: it lets you ship new client
builds without having to rebuild the VM every time.

To actually lock out old clients, bump both:

1. `world.SetVersion("...")` in `src/game.cpp:31` (client side).
2. `lobby_version_string` in `terraform.tfvars` (server side).

Then `terraform apply` and tag a release. The server-side change
only takes effect once the VM is replaced — the version string is
baked into the systemd unit by cloud-init. Terraform will show you
what's about to change before you confirm.

## When things go wrong

**"Could not create game" when a player tries to host.** The lobby
tried to spawn a dedicated-server subprocess for the match, and it
didn't phone home within 30 seconds. Check the logs:

```bash
sudo journalctl -u zsilencer-lobby | grep -i spawn
```

Usual suspects: the `zsilencer` binary is missing or broken after a
bad deploy (roll back), or UDP ports 30000–61000 aren't open in the
security group (check `terraform/main.tf`).

**The VM is up, but clients can't reach the lobby.** First:

```bash
sudo systemctl status zsilencer-lobby
```

If it's crash-looping right after `terraform apply`, remember that
Terraform only creates infrastructure — it doesn't deploy binaries.
You still need to push a tag so the Deploy workflow runs. If the
binary is there but the service fails to bind to port 517, the unit
file is missing `CAP_NET_BIND_SERVICE`; check
`terraform/cloud-init.yaml.tftpl`.

**Dedicated-server subprocess crashes the instant it spawns.**
Almost always because it can't write its palette-cache file. The
game writes to `$HOME` at startup, and the systemd unit sets
`HOME=/var/lib/zsilencer` so the write lands on the data volume
(which is writable) rather than inside `/home` (which is locked down
by `ProtectHome=true`). If someone edited `cloud-init.yaml.tftpl`
and dropped the `HOME` line, this is what it looks like. For actual
segfaults in game code, `git log -- src/game.cpp src/lobbygame.cpp`
has the history of prior fixes to refer to.

**Clients stop being able to connect after a while.** A few things
this can be:

- The static IP got detached from the VM. AWS starts charging for
  detached IPs, and they stop working until reattached. `terraform
  apply` fixes it.
- You changed the DNS record and are still inside the 5-minute TTL.
- Your clients have an old `LOBBY_HOST` baked in and you moved the
  server. The only fix is to ship them a new build.

**User accounts vanished after a VM rebuild.** The data volume
didn't re-attach. The Terraform config keeps the volume as a
separate resource specifically so this doesn't happen, and the
systemd unit has `Requires=var-lib-zsilencer.mount` so the service
won't start until the volume is mounted. If state really looks
empty, check that `aws_volume_attachment` still exists in
`terraform/main.tf` — it's the thing keeping the disk bound across
instance replacements.
