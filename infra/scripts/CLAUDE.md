# infra/scripts — deploy + dev-loop scripts

Standalone bash/PowerShell scripts that don't fit any one component.
All of them assume they're run from the repo root (or `cd` themselves
there); the path-discovery hack in each is `cd "$(dirname "$0")/../.."`
because this directory is two levels deep.

## What's here

- `install-linux-server.sh` — one-shot bootstrap for a fresh Ubuntu
  22.04+ VM. Installs Docker, then `docker compose -f infra/docker-compose.yml up -d`.
  Used by self-hosters following the README quick-start.
- `seed-ssm.sh` — interactive one-shot that puts every Silencer secret
  into AWS SSM Parameter Store under `/silencer/*`. Run once per AWS
  account before `terraform apply`; teammates with IAM read access
  pull from the same source. Idempotent — skips existing params
  unless `--overwrite` is passed (rotation flow).
- `fastdeploy.sh` — bypass CI: rsync the working tree to the AWS lobby
  host, build the C++ dedicated-server binary on the box (ARM64), swap
  it into `/opt/silencer/current/`, restart `silencer-lobby`. Debug-only;
  prod releases go through `.github/workflows/deploy.yml`.
- `build-mac-local.sh` — local macOS client build pointed at a local
  lobby (default `127.0.0.1:15170`, override with `LOBBY_HOST`/`LOBBY_PORT`
  env vars). Builds via Homebrew + cmake.
- `test-updater.sh` (macOS) / `test-updater.ps1` (Windows) — **automated**
  end-to-end auto-updater test, run as a step in `release.yml` (issue #303).
  Builds an OLD + NEW client (distinct `SILENCER_VERSION`), packages NEW into a
  zip the way `release.yml` does, serves it over a local HTTP server, launches
  OLD **headless** with a control port, drives the real cppx update flow
  (`show_update_screen --url --sha256` → click `UpdateConsent`), and after
  stage-2 swaps + relaunches, pings the auto-relaunched process and asserts its
  version is NEW. Exits non-zero on any failure. No lobby — the lobby's role
  (handing the client a download URL + sha256) is injected via the
  `show_update_screen` control op, which keeps the test uniform and lets it run
  fully headless. Reuse a prebuilt client with `OLD_BUILD_DIR` / `NEW_BUILD_DIR`
  (CI passes the release `build/` as OLD). Linux is intentionally excluded (not
  a shipped self-update platform; its cwd-relative asset paths break an
  in-place self-replace).
- `test-launcher-updater.sh` (macOS) — the same shape for the **launcher's**
  self-update, run as a gate in `release.yml`'s `build-launcher-macos`
  (issue #343). Builds an OLD + NEW launcher (distinct
  `SILENCER_LAUNCHER_VERSION`), serves `update-launcher.json` + the zip over
  loopback, launches OLD headlessly (`SDL_VIDEODRIVER=dummy`, shot mode)
  with `SILENCER_LAUNCHER_TEST_SELF_UPDATE=1`, and asserts the
  auto-relaunched process prints the NEW build id — the environment,
  including the captured stderr fd, survives stage-2's exec, so the banner
  lands in the same log. It waits for its own HTTP server to answer before
  driving the launcher (the #341/#342 lesson), and the relaunched build
  stops at "already up to date", so the inherited trigger cannot loop.
  Reuse prebuilt launchers with `OLD_BUILD_DIR` / `NEW_BUILD_DIR` (CI
  passes the release `build-launcher/` as OLD).

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
