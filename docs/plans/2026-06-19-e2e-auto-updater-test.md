# E2E auto-updater test in the release workflow

Issue [#303](https://github.com/Arsia-Mons/Silencer/issues/303). Closes the
gap PR #302 explicitly punted: *"the real cross-platform self-replace
(STAGING → spawn → exec → restart) still needs manual/e2e verification."*
That gap is exactly the class of bug #301 shipped — the cppx migration
dropped the STAGING→stage-2 handoff and nothing caught it.

## Goal

A GitHub Actions step in `release.yml` that, on **macOS + Windows** (the two
shipped self-update platforms), proves the auto-updater works **end to end**:
an installed client downloads a new build, verifies it, self-replaces in
place, and **the new version starts automatically**. The relaunched process
must be observed reporting the new version — not inferred.

### Why not Linux

Linux is intentionally out of scope: (1) it is not a shipped self-update
platform — the lobby's update manifest (`services/lobby/update.go`) has only
macOS + Windows URLs, so a Linux client never receives a reject-with-update;
(2) its stage-2 code is the *union* of paths macOS + Windows already cover —
the POSIX-rename swap (shared with macOS) and the minizip extract (shared with
Windows), with no Linux-unique logic beyond a one-line `Launch` copy; and
(3) Linux resolves core assets **cwd-relative** (`GetResDir()` returns `""`
for a portable build, so `palette.cpp`/`resources.cpp` open `"PALETTE.BIN"`
etc. against the cwd), which an in-place self-replace breaks —
`CleanupPreviousUpdate()` deletes `<install>.old` (the relaunched client's
inherited cwd) on startup. macOS (`CDResDir` via `_NSGetExecutablePath`) and
Windows (`GetResDir` via `GetModuleFileNameA`) both resolve assets from the
executable path, so they survive the swap; Linux structurally does not.

## Why headless (not windowed)

CI runners are effectively headless and the lobby's update manifest only
covers macOS + Windows (Linux platform byte → bare reject). A windowed,
lobby-driven test can't cover Linux and is GPU/display-flaky on hosted
runners. The retained cppx UI renders and is **clickable in headless mode**
(the whole `tests/cli-agent/e2e` suite drives cppx buttons headless), so we
drive the *real* "Update" button headlessly and assert on the *outcome*
(new version running). This reproduces the #301 surface — and because the
assertion is on the end state, it stays mechanism-agnostic.

What this does NOT cover (acceptable, out of scope): the lobby wire format
(reject-with-update parse; macOS/Windows-only, separately unit-tested) and
pure-visual regressions (button present+wired but invisible). No visual
layer per decision on the issue.

## The real path, headless

```
show_update_screen --url <loopback> --sha256 <hex>   # real PresentUpdate -> PROMPTING
  -> wait_for_state UPDATING
  -> click UpdateConsent                              # real Updater::Consent()
  -> worker: download (loopback http) -> verify sha256 -> STAGING
  -> Game::Loop: PumpStage2() -> UpdaterStage2::Launch -> client exits
  -> stage-2: extract -> atomic swap -> relaunch (no args, inherits env)
  -> new client: env headless + env control-port -> ping == NEW version
```

## Production seams (3 small, test-consistent additions)

The shipping binary already carries automation seams (`ForceState`,
`show_update_screen`, the whole control socket). These extend that surface,
they don't introduce a new philosophy.

1. **Tighten the `PumpStage2` gate** — `game_loop.cpp`:
   `if(!headless)` → `if(!world.dedicatedserver.active)`. PR #302's intent
   was "the **dedicated server** must never fork a GUI client"; `!headless`
   was an over-broad proxy that also blocked a headless cli-agent. `-s` sets
   `dedicatedserver.active`; `--headless` does not — so this is *more
   correct* and lets the headless test reach the real spawn. (Load-bearing:
   without it the headless path can't reach STAGING→spawn at all.)

2. **`show_update_screen` accepts a real payload** — `controldispatch.cpp`:
   optional `url` + `sha256` (64 hex) args call
   `Updater::PresentUpdate(url, sha)` (real PROMPTING with a real download
   target) instead of `ForceState`. Absent → existing ForceState behavior
   (unchanged for the modal-render tests). The subsequent real Update-button
   click then downloads for real.

3. **Env fallback for control-port + headless** — `game_init.cpp`: when the
   `--control-port` flag is absent, fall back to `SILENCER_CONTROL_PORT`;
   when `--headless` is absent, fall back to `SILENCER_HEADLESS`. Flags win.
   stage-2 relaunches with **no args** but inherits the environment, so this
   is the only way to make the auto-relaunched process come up headless +
   observable. Two ports avoid ambiguity: the OLD client drives on flag-port
   `P`; the relaunched NEW client binds env-port `Q`; the test pings `Q`.

## The harness

`infra/scripts/test-updater.sh` (interactive smoke test today) becomes
automated + assertive; a `.ps1` mirror covers Windows. Shape:

- Build **OLD** (`SILENCER_VERSION=<tag>`) and **NEW** (`=99999`) — distinct,
  so `ping.version` is an unambiguous OLD→NEW signal. NEW packaged to match
  `release.yml` exactly (macOS `ditto -ck --sequesterRsrc --keepParent
  Silencer.app`; Windows `Compress-Archive` of `silencer/`).
- Serve NEW zip from `http://127.0.0.1:PORT` (loopback http is allow-listed
  by `UpdaterDownload::IsAllowed`). Compute its sha256 for the payload.
- Run OLD from a **copy** of the build (stage-2 mutates the install in place;
  the real artifact must stay pristine for signing/upload) with
  `--headless --control-port P` and env `SILENCER_HEADLESS=1
  SILENCER_CONTROL_PORT=Q`.
- Drive: `show_update_screen --url … --sha256 …` → `wait_for_state UPDATING`
  → `wait_for_label UpdateConsent` → `click UpdateConsent`.
- Wait for OLD to exit (stage-2 spawned), then poll `ping` on `Q` until
  `version==99999` (timeout → fail). Assert + cleanup.

No lobby and no second HTTP-vs-lobby coupling — `show_update_screen`'s real
payload replaces the lobby's role, the same way on macOS and Windows.

`infra/scripts/test-updater.sh` (macOS, bash) and `test-updater.ps1`
(Windows, PowerShell) are mirror implementations of the same flow.

## CI wiring (`release.yml`)

Add an "auto-updater e2e" step to `build-macos` and `build-windows`, after the
build and **before** signing/upload, operating on a scratch copy so the
shipped artifact is untouched. The harness self-builds OLD
(`SILENCER_VERSION=00023`) + NEW (`=99999`) into `e2e-build-{old,new}`
(sccache-warm off the release build: only the few version-embedding TUs
recompile), decoupled from the signed artifact — the updater's
download/verify/swap/relaunch logic is identical regardless of signing, and
Gatekeeper/notarization is validated separately by the existing
`spctl`/`stapler` steps (an unsigned, non-quarantined local build self-updates
fine on the runner). Bump job timeouts for the extra build.

## Observability summary

`ping` returns `{version, build, frame, paused}` — the only seam exposing the
running build's identity. After relaunch the NEW process answers `ping` on
env-port `Q` with `version=99999`: direct proof the new version started
automatically.
