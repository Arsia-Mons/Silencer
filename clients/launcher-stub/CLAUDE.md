# clients/launcher-stub — bootstrap stub

The tiny always-works updater the user actually launches (issue #347).
It shows a native progress window immediately, checks for a launcher
update (~3s budget), applies it into a versioned store, and runs the
current payload — **falling back to the existing version on ANY
failure**. The cppx launcher in `clients/launcher/` is the *payload* it
manages; the stub is what shortcuts and the dock point at.

Two rules define this directory:

1. **Own code only.** No SDL, no cppx, no vendored TLS/zip/network code,
   and NOTHING compiled in from `clients/silencer/` or
   `clients/launcher/`. OS facilities do the heavy lifting (table
   below). The one vendored algorithm is a ~150-line SHA-256.
2. **The stub stays frozen.** Its safety comes from (almost) never
   changing — every volatile feature belongs in the payload. Refuse
   feature creep here.

## How an update commits

Payload versions live in `<store>/<build_id>/`; `current.txt` names the
one that runs. An update stages next to the live version and the atomic
`current.txt` flip is the **only commit point** — server down, partial
download, bad checksum, failed extract all leave the working install
untouched. A freshly-updated payload that exits nonzero within ~5s —
including Windows crash codes, which are *negative* NTSTATUS values, so
never test `rc > 0` — is rolled back automatically. Launch tries
`current.txt` → `previous.txt` → the seed payload, whichever spawns
first. `main.cpp` owns this flow;
platform files only implement the `stub.h` interface (HTTP, GUI,
processes, signature check). Store location: `<exe dir>/versions` when
writable, else the per-user data dir; always the per-user dir on macOS
(the bundle is signed and often read-only).

The manifest is the existing `update-launcher.json` /
`/api/launcher/manifest/self`: `build_id` + `<platform>_payload_url` +
`<platform>_payload_sha256` (platform = `windows` | `macos` | `linux`; a
missing platform key just means "no update"). The `_payload_` infix is
load-bearing: the plain `macos_url` key carries the FULL bundle zip that
old in-the-wild launchers feed to their stage-2 swap (that is their
migration path to this architecture) — pointing the stub at it would
install a stub as its own payload. Payload archives contain exactly a
version dir's contents. URLs must be https, or http on loopback. When
no version is installed the stub bootstraps from the manifest, else it
launches the seed payload shipped beside it (`payload/`, macOS
`Contents/Resources/payload/`).

## Per-platform mechanics

| | HTTP | GUI | Extract | Verify |
|---|---|---|---|---|
| Windows | WinHTTP | `TaskDialogIndirect` (marquee → percent) | `System32\tar.exe` (bsdtar reads zip) | sha256 |
| macOS | libcurl via dlopen | AppKit window + `NSProgressIndicator` | `ditto -x -k` | sha256 + `codesign -R` Developer ID |
| Linux | libcurl via dlopen | `zenity --progress`, else silent | `tar` (payload ships `.tar.gz` — GNU tar can't read zip) | sha256 |

libcurl is dlopen'd (not linked) so a machine without it still launches
the payload — the check just degrades to "no update". The macOS
Developer ID requirement's team id comes from the running stub's own
signature; an unsigned stub (dev, e2e) has nothing to enforce and skips
it.

## Build

Windows: `./build.ps1` (resolves MSVC via vswhere like the game's
wrapper; no vcpkg — there are no dependencies). macOS/Linux:

```bash
cmake -S clients/launcher-stub -B clients/launcher-stub/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build clients/launcher-stub/build
```

## Test

```bash
bash tests/e2e/run.sh   # needs bun (loopback server) + curl
```

Drives the real stub binary headlessly through: up-to-date, update
applies, server down (fast fallback), checksum mismatch (discard),
rollback (fresh update dies at startup), first-run bootstrap. The
update case also cross-checks the vendored SHA-256 against the platform
sha256 tool. `tests/payload/` is a stand-in payload whose identity and
exit code come from files beside its binary.

Env overrides (all read by the stub): `SILENCER_STUB_STORE`,
`SILENCER_STUB_MANIFEST_URL`, `SILENCER_STUB_NO_GUI=1`, and
`SILENCER_STUB_SLOW_MS=N` (dev/demo: hold the checking phase N ms and
throttle the download so the GUI is watchable). The manifest URL
otherwise comes from `launcher.json`'s `manifest_url_launcher`,
defaulting to the GitHub `update-launcher.json`.

To *see* the GUI end-to-end, `bash tests/e2e/demo.sh [slow-ms]` stages
a fake ~10MB update on loopback and runs the stub with its window:
marquee "Checking..." → determinate "Updating... N%" → the test payload
starts.

## Gotchas

- **Every `std::string` crossing `stub.h` is UTF-8**; platform files
  convert at the OS boundary (`widen`/`narrow` on Windows). Portable
  code goes through `fs::u8path`, never `fs::path(std::string)`.
- **`src/stub.manifest` (comctl32 v6) is load-bearing** —
  `TaskDialogIndirect` fails without it. CMake embeds `.manifest`
  sources itself. A failure logs "TaskDialogIndirect failed" and
  degrades to windowless, so check the log if no window appears.
- The Windows stub is a **GUI-subsystem** binary (`wWinMain` in
  `platform_win.cpp`): stderr only goes somewhere when redirected. The
  log is `<store>/stub.log` either way.
- **zenity can vanish mid-write**: the Linux GUI ignores SIGPIPE and
  drops to silent on any write failure. No GTK linkage, ever — a GUI
  dependency in the stub contradicts its reason to exist.
- The stub closes its window ~300ms after spawning the payload so
  there's no gap between the two windows (no dead air, and no window
  overlap either).
- **The worker thread has exception barriers** (`stub_main`): an
  escaping exception would `std::terminate` before anything launches.
  New code in the update path either uses `std::error_code` overloads
  or stays inside those barriers.
- **macOS extracts before it codesign-verifies** (it has to — the check
  runs on the extracted bundle). Zip-slip protection during that
  extract rests on `ditto`; phase 2's mac e2e must include a hostile
  archive (`../evil` entry) before the macOS path ships.

## How this ships

The composite actions (`.github/actions/build-launcher-*`) build the
stub next to the launcher and stage the stub-first layouts: Windows and
Linux put the stub at the package root named `silencer-launcher(.exe)`
with the payload under `payload/`; macOS builds the stub's own
"Silencer Launcher.app" (`SilencerLauncherStub-Info.plist`) and nests
the self-contained launcher app at `Contents/Resources/payload/`.
`release.yml` publishes the per-platform payload archives, writes both
key families into `update-launcher.json`, and gates on
`infra/scripts/test-launcher-updater.sh` (macOS: real-payload update +
hostile-archive case) and `tests/e2e/run.sh` (Windows/Linux jobs).
Existing macOS installs migrate automatically: their old in-app
stage-2 reads `macos_url`, swaps the whole bundle, and wakes up
stub-first; a Windows installer upgrade clears the pre-stub root files
(`[InstallDelete]`).
