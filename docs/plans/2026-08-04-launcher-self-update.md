# Launcher self-update: Sparkle vs. reusing `clients/silencer/src/updater/`

Status: **superseded by the bootstrap stub** (issue #347,
`clients/launcher-stub/`). The in-app mechanism this document chose is
being replaced: the launcher's own update path must be independent code
that still works when the launcher itself is broken, so a tiny stub now
owns the whole loop and the launcher becomes a versioned payload. The
game-side updater analysis below still stands — stage-2 remains correct
for the *game*, whose repair path is the launcher.

Previous status: **built** (Option B, issue #343). This document stays as
the decision record. One prediction did not survive contact with the code
and is corrected below: `updaterstage2.cpp` IS reused verbatim.

The launcher installs and updates the *game*. Nothing updates the
*launcher*. A user who drags `Silencer Launcher.app` out of the DMG keeps
that build until they download a new DMG by hand. This document picks a
mechanism before any of it is written.

## What the launcher already has

The install path the launcher runs today is already most of an updater:

| Step | Where it lives | Reused by the launcher? |
|---|---|---|
| Fetch a manifest with a version + URL + sha256 | `src/app.cpp` | yes |
| HTTPS download with progress + cancel | `updaterdownload.cpp` | yes, compiled in |
| SHA-256 verify | `updatersha256.cpp` | yes, compiled in |
| Zip extract (`ditto` on macOS) | `updaterzip.cpp` | yes, compiled in |
| Replace a **running** bundle and relaunch | `updaterstage2.cpp` (658 lines) + the signed `Contents/Helpers/updater-stage-2` binary | yes (issue #343) |

Only the last row is missing. Everything above it is code the launcher
links today, because installing the game needs exactly the same steps.
The game needed stage-2 because it must replace itself; the launcher has
the same problem for its own bundle and no other.

## Option A — Sparkle

[Sparkle](https://sparkle-project.org/) is the default self-update
framework for Mac apps outside the App Store.

**For**

- **EdDSA signature on the payload.** This is the one real advantage.
  Sparkle verifies the update against a key we hold offline, so a
  compromised manifest host cannot push a malicious build. Our
  manifest+SHA-256 chain trusts whoever serves the manifest.
- Mature handling of the install-on-quit dance, permissions on
  `/Applications`, and delta updates.
- Free update UI, background check scheduling, and a "skip this version"
  model we would otherwise write.

**Against**

- **macOS only.** The launcher targets Windows and Linux too. Sparkle
  covers one of the three; WinSparkle is a separate project with a
  separate feed, and Linux has neither. Choosing Sparkle means owning
  two or three update mechanisms and two or three feed formats.
- **The free UI is not free here.** The launcher is a custom retained
  cppx surface in phosphor green; Sparkle's dialogs are stock AppKit and
  would look like a different application. Making them match means
  implementing `SPUUserDriver` and driving it from the cppx UI — at
  which point the UI benefit is gone and only the download plumbing
  remains, which we already have.
- **New build surface.** An embedded `.framework` to copy into
  `Contents/Frameworks`, sign, and notarize, plus at least one
  Objective-C translation unit in a tree that is C++ and a Python
  transpiler. `package-macos.sh` and the CI action both grow a
  framework-specific branch.
- **A fifth feed URL.** An appcast XML endpoint, in a format nothing
  else in this repo speaks, next to the four `*_url` keys the launcher
  already has.

## Option B — reuse `clients/silencer/src/updater/`

Add a launcher manifest URL and a stage-2 self-replace, mirroring what
the game already does.

**For**

- Roughly four-fifths of it exists and is already compiled into the
  launcher binary. The new work is the self-replace step and a manifest
  endpoint.
- **One mechanism on all three platforms.** `updaterstage2.cpp` already
  has the Windows and Linux branches. The launcher's Windows and Linux
  builds inherit self-update from the same code that gives macOS it.
- **One manifest format.** `services/admin-api` already serves
  `/api/launcher/manifest/{stable,nightly}` for the game; a launcher
  manifest is the same shape at a new path.
- The update UI is the launcher's own UI, in the launcher's own style,
  built from primitives already on screen (the install progress bar).
- CI already knows how to sign and notarize a nested helper binary —
  `build-macos` does it for `Contents/Helpers/updater-stage-2`.

**Against**

- We write and own the self-replace step. It is the part of an updater
  most likely to brick an install, and **the launcher has no fallback**:
  if the game's self-update fails the launcher can repair it, but if the
  launcher's own self-update fails the user is down to re-downloading the
  DMG. The game's version of this is battle-tested by the release e2e;
  the launcher's would need its own.
- No EdDSA. See the mitigation below.
- ~~`updaterstage2.cpp` cannot be reused verbatim — expect an adapted
  copy.~~ **Wrong, and important:** every path stage-2 acts on arrives
  through argv (`--zip`, `--install-dir`, `--pid`, `--relaunch`), and
  `ResolveInstallDir` only strips `/Contents/MacOS`. Nothing in it is
  specific to the game's bundle. The launcher compiles it in unchanged
  and only needed its own `Contents/Helpers/updater-stage-2` binary at
  the path `Launch()` hardcodes. A fork would have been 658 lines of
  the most brick-prone code in the tree, duplicated.

## Recommendation: **Option B**, with a signature check added

Reuse the existing updater. The deciding argument is not effort, it is
**platform coverage**: Sparkle solves one third of the problem and
obliges us to solve the other two thirds separately, while Option B
solves all three with one code path we already ship.

Sparkle's genuine advantage — payload signing — is recoverable without
Sparkle. Before swapping the new bundle in, verify it is *our* build:

```sh
codesign --verify --strict \
  -R '=anchor apple generic and certificate leaf[subject.OU] = "<TEAM_ID>"' \
  "<staged>/Silencer Launcher.app"
```

A tampered payload from a compromised manifest host fails that check,
because the attacker cannot produce a bundle signed by our Developer ID.
That closes the gap the SHA-256-over-TLS chain leaves open, in a few
lines, using the signing we are already doing for Gatekeeper.

## The work, as built (issue #343)

1. `manifest_url_launcher` in `launcher.json` +
   `/api/launcher/manifest/self` in `services/admin-api`. Done earlier.
2. `release.yml` publishes `silencer-launcher-macos-arm64.zip` (zipped
   after stapling) + `update-launcher.json`. Done earlier.
3. The flow in `src/app.cpp`: fetch the manifest, compare its
   `build_id` against the compiled-in `SILENCER_LAUNCHER_BUILD_ID`,
   download, verify sha256, verify the Developer ID requirement above
   on the extracted bundle, then hand the zip to the unmodified
   `UpdaterStage2::Launch`. The team ID for the requirement comes from
   the running bundle's own signature, not a build knob — a signed
   release enforces it automatically, and an unsigned build (dev, the
   e2e) has nothing to enforce and skips it.
4. A `Contents/Helpers/updater-stage-2` target (the exact name
   `Launch()` hardcodes), signed and notarized by the release job the
   same way the game's helper is.
5. `infra/scripts/test-launcher-updater.sh`, a `release.yml` gate on
   `build-launcher-macos`: the just-built launcher self-updates to a
   `99999`-versioned build headlessly, and the test asserts the
   auto-relaunched process reports the new build id. Given the
   no-fallback risk above, this gate is not optional. It waits for its
   own HTTP server before it drives the launcher (the #341/#342
   lesson) and captures the child's stderr.
