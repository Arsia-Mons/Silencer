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
- `STAGING` — `UpdaterStage2::Launch` was called; UI must tear down SDL before main returns.
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

// Called by the game loop after stage-2 spawned:
if (updater.IsStage2Spawned()) return false; // exits Game::Loop
```

## Security

- `UpdaterDownload::IsAllowed(url)` rejects everything except `https://` and `http://` on loopback. Call before any network operation.
- SHA-256 of the downloaded zip is verified by `Updater::VerifyFile` (also exposed as a static for unit tests) before extraction begins.

## Stage-2 flow

1. Normal client (`--self-update-stage2` absent): reaches STAGING, calls `UpdaterStage2::Launch(zippath)`, marks spawned, returns from game loop so `~Game()` tears down SDL cleanly.
2. Stage-2 process (invoked with `--self-update-stage2`): `UpdaterStage2::Run` overwrites the binary, then `exec`-replaces itself with the new client.

## Rules

- Never call `Updater` methods from the SDL event thread except `GetState`/`GetProgress`/`GetErrorMessage` — those are atomic-safe.
- Do not add certificate pinning or custom CA logic here; delegate to the OS TLS stack.
