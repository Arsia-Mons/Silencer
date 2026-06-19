# src/updater

Self-update pipeline for the Silencer client. Runs entirely on a background thread; the UI polls state and progress atomically.

## Files

| File | Purpose |
|---|---|
| `updater.h/cpp` | State machine orchestrator. Owns the worker thread, exposes progress/state to the UI. |
| `updaterdownload.h/cpp` | Blocking HTTPS downloader. Progress callback, cancel support. |
| `updaterstage2.h/cpp` | Stage-2 launcher and runner. Handles the self-replace step. |
| `updatersha256.h/cpp` | Streaming SHA-256 (no external crypto dep). |
| `updaterzip.h/cpp` | Minizip-backed zip extractor. |

## State machine (`Updater::State`)

```
IDLE → PROMPTING → DOWNLOADING → VERIFYING → STAGING → DONE
                                                      ↘ FAILED → (Retry) → DOWNLOADING
```

- `PROMPTING` — lobby rejected connection and sent an update URL; waiting for user consent.
- `STAGING` — worker finished download + verify. `Game::Loop` drives `Updater::PumpStage2()`, which spawns the stage-2 child on the main thread (the worker can't fork itself) and tears SDL down before main returns.
- `DONE` — stage-2 is running; `Game::Loop` should return `false` so `~Game()` runs and releases audio/video before the new client opens the device.

## Key APIs

```cpp
// Called by lobby code on a reject-with-update response:
updater.PresentUpdate(url, sha256);   // IDLE → PROMPTING

// Called by UI buttons:
updater.Consent();   // PROMPTING → DOWNLOADING
updater.Cancel();
updater.Retry();     // FAILED → DOWNLOADING

// Polling (safe from any thread):
updater.GetState();
updater.GetProgress();      // 0.0 – 1.0
updater.GetErrorMessage();

// Called every frame by the game loop (main thread). At STAGING it spawns
// stage-2 once, then latches IsStage2Spawned():
updater.PumpStage2();
if (updater.IsStage2Spawned()) return false; // exits Game::Loop → ~Game() teardown
```

## Security

- `UpdaterDownload::IsAllowed(url)` rejects everything except `https://` and `http://` on loopback. Call before any network operation.
- SHA-256 of the downloaded zip is verified by `Updater::VerifyFile` (also exposed as a static for unit tests) before extraction begins.

## Stage-2 flow

1. Normal client (`--self-update-stage2` absent): reaches STAGING; `Game::Loop` calls `Updater::PumpStage2()`, which calls `UpdaterStage2::Launch(zippath)` (one-shot) and latches `IsStage2Spawned()`; the loop then returns so `~Game()` tears down SDL cleanly. A failed spawn transitions the state machine to FAILED. `PumpStage2()` is gated on `!world.dedicatedserver.active` (a dedicated `-s` server must never fork a GUI client) — NOT on `!headless`, so a headless windowed client (the e2e below) is a valid self-update host.
2. Stage-2 process (invoked with `--self-update-stage2`): `UpdaterStage2::Run` overwrites the binary, then `exec`-replaces itself with the new client.
   - macOS launches the nested signed helper at `Silencer.app/Contents/Helpers/updater-stage-2`. Do not recreate a temporary `.app` bundle at runtime.
   - Windows/Linux still copy the current executable to a temp path for the handoff.

## End-to-end test

`infra/scripts/test-updater.{sh,ps1}` exercise the WHOLE path headlessly
(download → verify → STAGING → `PumpStage2` → stage-2 swap → auto-relaunch),
run as a step in `release.yml` on macOS + Windows. They drive the real cppx
`UpdateConsent` button and assert the auto-relaunched process reports the new
version over the control socket — closing the "STAGING→spawn needs manual/e2e
verification" gap (#301/#302/#303). This is what the unit tests deliberately
don't cover (`updater_sm_test.cpp` runs a real process only here). Design:
[`../../../../docs/plans/2026-06-19-e2e-auto-updater-test.md`](../../../../docs/plans/2026-06-19-e2e-auto-updater-test.md).

## Rules

- Never call `Updater` methods from the SDL event thread except `GetState`/`GetProgress`/`GetErrorMessage` — those are atomic-safe.
- Do not add certificate pinning or custom CA logic here; delegate to the OS TLS stack.
