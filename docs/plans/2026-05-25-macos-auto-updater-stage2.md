# macOS Auto-Updater Stage-2

**Status:** Proposed  
**Date:** 2026-05-25  
**Issue:** #243

## Goal

Fix the macOS auto-updater failure where Gatekeeper reports
`updater-stage-2` as damaged and prompts the user to move it to Trash.

The correct macOS shape is not to create an updater app bundle at
runtime. The updater executable must be shipped as nested signed code
inside `Silencer.app`, notarized as part of the release artifact, and
launched from that trusted bundle.

## Decision

Keep the existing cross-platform lobby-driven updater, but make macOS
stage-2 a real helper executable:

- Build a dedicated `updater-stage-2` command-line helper target on
  macOS.
- Embed it at `Silencer.app/Contents/Helpers/updater-stage-2`.
- Sign the helper first, then sign `Silencer.app`, then notarize and
  staple the app and DMG.
- Launch the embedded helper directly from `Contents/Helpers`; do not
  copy the main game executable to `/tmp`, and do not synthesize a
  `silencer-stage2.app`.

Sparkle 2 is the macOS-native long-term option and should be preferred
if we want to replace the custom updater entirely. For this issue, a
signed helper is the narrow fix because the lobby protocol, update
manifest, SHA-256 verification, Windows updater, and release zip format
already exist.

## Why the Current Design Fails

The current macOS launcher builds a temporary bundle under
`/tmp/silencer-stage2.app`, copies the main executable into it as
`silencer-stage2`, copies `Info.plist`, and mirrors `Frameworks/`.
That tries to recreate enough of a signed app after distribution.

That is fragile on macOS:

- The app's outer resource envelope is signed. Runtime-created bundle
  contents are not the notarized artifact Apple assessed.
- The copied executable no longer lives in the bundle path and resource
  layout it was signed with.
- A helper named differently from the bundle's `CFBundleExecutable`
  makes the temporary bundle look malformed.
- If quarantine is present, Gatekeeper assesses this temporary helper
  as user-downloaded code and can reject it before our logging code runs.

Apple's signing model expects nested code to live in standard bundle
locations and be signed before the outer app is signed. `Contents/Helpers`
is a standard location for helper tools.

## Implementation Plan

1. Extract the stage-2 implementation from the game executable.

   Move the code behind `UpdaterStage2::Run` into a small reusable module
   that can be linked by both the game and a helper target. Keep the
   game-facing `UpdaterStage2::Launch(zippath)` API so the UI and state
   machine do not change. Add a tiny helper-only `main` that calls the
   shared runner when `--self-update-stage2` is present.

   Keep the boundary explicit: the helper runner owns argument parsing
   and replacement work, while the game-side launcher owns only locating
   and spawning the helper. Do not move more updater orchestration into
   `main.cpp`.

2. Add a macOS-only helper target.

   In `clients/silencer/CMakeLists.txt`, add an executable target named
   `updater-stage-2` for Apple builds. It should link only the stage-2
   runner and minimal updater zip/extraction code. Avoid SDL, rendering,
   audio, networking, UI, and game systems. The helper should depend only
   on system libraries on macOS, because extraction uses `/usr/bin/ditto`.

   Install/copy the built helper into:

   ```text
   Silencer.app/Contents/Helpers/updater-stage-2
   ```

3. Change macOS launch behavior.

   On Apple builds, `UpdaterStage2::Launch` should resolve the installed
   app bundle from the running game path, then spawn:

   ```text
   <Silencer.app>/Contents/Helpers/updater-stage-2
   ```

   Pass the existing arguments:

   ```text
   --self-update-stage2
   --zip=<downloaded zip>
   --install-dir=<Silencer.app>
   --pid=<parent pid>
   --relaunch=<Silencer.app/Contents/MacOS/Silencer>
   ```

   The helper may live inside the bundle it later renames to `.old`.
   POSIX permits an already-running executable to continue after its
   containing directory is renamed. Keep the helper's current working
   directory outside the app bundle.

4. Remove the macOS temporary bundle path.

   Delete the macOS code that creates `/tmp/silencer-stage2.app`, copies
   `Info.plist`, copies `Frameworks/`, and renames the main executable to
   a helper name. Keep the Windows and Linux paths unchanged.

5. Sign inside-out in release CI.

   Update `.github/workflows/release.yml` so macOS release signing signs
   in this order:

   ```text
   Silencer.app/Contents/Frameworks/*.dylib
   Silencer.app/Contents/Helpers/updater-stage-2
   Silencer.app/Contents/MacOS/Silencer
   Silencer.app
   ```

   Use the same Developer ID Application identity, hardened runtime, and
   timestamp options already used for the main app.

6. Strengthen bundle checks.

   Extend `tests/e2e/check-bundle-macos.sh` to verify:

   - `Contents/Helpers/updater-stage-2` exists and is executable.
   - The helper has no unbundled non-system dylib references. Prefer no
     non-system dylib references at all.
   - `codesign --verify --deep --strict` covers the final app in release
     CI after helper signing.

7. Update the local updater harness.

   Update `infra/scripts/test-updater.sh` so the macOS path exercises the
   embedded helper. The local unsigned/ad-hoc build can validate process
   flow, extraction, rename, rollback, and relaunch. Gatekeeper behavior
   still requires a Developer ID signed, notarized release artifact.

8. Add release-only verification for Gatekeeper.

   After notarization and stapling in release CI, verify the app and DMG:

   ```bash
   codesign --verify --deep --strict --verbose=2 Silencer.app
   xcrun stapler validate Silencer.app
   spctl -a -vv -t execute Silencer.app
   spctl -a -vv -t open --context context:primary-signature silencer-macos-arm64.dmg
   ```

   Add a manual release checklist item to install an older notarized DMG,
   connect to the lobby, accept the update, and confirm the helper runs
   without the damaged-app prompt.

## Out of Scope

- Replacing the custom updater with Sparkle 2.
- Changing the lobby update protocol.
- Changing the release artifact shape consumed by the updater
  (`silencer-macos-arm64.zip` remains a `ditto --keepParent` archive of
  `Silencer.app`).
- Changing Windows updater behavior.
- Adding privileged installation into locations the user cannot write.

## Rollout Note

This cannot retroactively fix already-shipped macOS builds that do not
contain the nested helper. Those users may need one manual DMG install to
get onto the helper-bearing version. Once installed, future auto-updates
use the signed helper path.

## References

- Apple Code Signing Guide: nested code must be signed before the outer
  app, and `Contents/Helpers` is a standard helper location.
- Apple notarization guidance: distributed Developer ID software should
  be signed, notarized, and stapled before distribution.
- Sparkle 2: the standard macOS updater framework if we later decide to
  replace the custom updater.
