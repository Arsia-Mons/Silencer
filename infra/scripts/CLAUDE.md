# infra/scripts — deploy + dev-loop scripts

Standalone bash/PowerShell scripts that don't fit any one component.
All of them assume they're run from the repo root (or `cd` themselves
there); the path-discovery hack in each is `cd "$(dirname "$0")/../.."`
because this directory is two levels deep.

## What's here

- `install-linux-server.sh` — one-shot bootstrap for a fresh Ubuntu
  22.04+ VM. Installs Docker, then `docker compose -f infra/docker-compose.yml up -d`.
  Used by self-hosters following the README quick-start.
- `fastdeploy.sh` — bypass CI: rsync the working tree to the AWS lobby
  host, build the C++ dedicated-server binary on the box (ARM64), swap
  it into `/opt/silencer/current/`, restart `silencer-lobby`. Debug-only;
  prod releases go through `.github/workflows/deploy.yml`.
- `build-mac-local.sh` — local macOS client build pointed at a local
  lobby (default `127.0.0.1:15170`, override with `LOBBY_HOST`/`LOBBY_PORT`
  env vars). Builds via Homebrew + cmake.
- `test-updater.sh` / `test-updater.ps1` — end-to-end auto-updater
  smoke test. Builds two client versions, packages the new one into a
  zip the way `release.yml` does, starts a local HTTP server + lobby,
  launches the old client and watches it self-update.

## Gotchas

- `fastdeploy.sh` `--exclude=infra` is wide enough to cover everything
  in this directory plus the terraform module. If you add new excluded
  paths there, prefer extending the `infra` exclude rather than listing
  individual subdirs.
- `install-linux-server.sh` runs `sg docker -c ...` so the user it
  added to the docker group can run compose without re-login. The
  script must keep using that wrapper for the build/up calls.
- The auto-updater test scripts hardcode `127.0.0.1:15170` and
  `:8000` for the manifest server; collisions on dev boxes are
  unlikely but worth knowing.
