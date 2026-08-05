#!/usr/bin/env bash
# Automated end-to-end launcher self-update test (headless; runs in CI).
#
# Proves the FULL launcher self-update path: an installed launcher fetches its
# own manifest over loopback HTTP, sees a newer build_id, downloads the zip,
# verifies its sha256 (and the Developer ID requirement when the running build
# is signed), self-replaces in place via the bundled Contents/Helpers/
# updater-stage-2, and the NEW build relaunches AUTOMATICALLY — confirmed by
# the "[launcher] build <id>" banner the relaunched process writes to the
# inherited stderr, plus the swapped bundle's Info.plist.
#
# This gate matters more than the game's: a broken game updater can be
# repaired by the launcher, but a broken launcher updater leaves the user
# with no in-app path back (docs/plans/2026-08-04-launcher-self-update.md).
#
# Observability: stage-2 relaunches the new launcher with NO argv, but the
# environment IS inherited across the exec — so SILENCER_LAUNCHER_SHOT makes
# the relaunched build render offscreen and exit on its own, and its stderr
# lands in the same captured log as the OLD build's. The inherited
# SILENCER_LAUNCHER_TEST_SELF_UPDATE is harmless on the NEW build: its
# build_id matches the manifest, so the flow stops at "already up to date".
#
# Env knobs (all optional):
#   OLD_VER (default 00023)  NEW_VER (default 99999)  — distinct so the build
#                                                        banner is unambiguous
#   OLD_BUILD_DIR / NEW_BUILD_DIR — reuse a prebuilt launcher dir (skip the
#                    build). CI passes the release build-launcher/ as OLD.
#   KEEP_WORK=1    — leave the scratch dir for inspection
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OLD_VER="${OLD_VER:-00023}"
NEW_VER="${NEW_VER:-99999}"
ZIP_NAME="silencer-launcher-macos-arm64.zip"
APP_NAME="Silencer Launcher.app"
BIN_REL="Contents/MacOS/Silencer Launcher"

# macOS only, like the shipped self-update itself: update-launcher.json
# carries no Windows/Linux URL on purpose.
case "$(uname)" in
  Darwin) ;;
  *) echo "test-launcher-updater.sh: macOS only" >&2; exit 1 ;;
esac

ncpu()      { sysctl -n hw.ncpu; }
sha256_of() { shasum -a 256 "$1" | awk '{print $1}'; }
pick_port() {
  python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()'
}

build_version() {  # <dir> <version>
  local dir="$1" ver="$2"
  echo "=== building launcher $ver -> $dir ===" >&2
  local extra=()
  if [ "${SCCACHE_GHA_ENABLED:-}" = "true" ] && command -v sccache >/dev/null 2>&1; then
    extra+=(-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache)
  fi
  # SDL3_ROOT (exported by setup-sdl in CI) outranks the Homebrew prefix in
  # find_package's search order, so this one configure works in both places.
  # Default generator, same as the build-launcher-macos composite — the CI
  # image is not guaranteed a ninja.
  cmake -B "$dir" -S "$REPO_ROOT/clients/launcher" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/homebrew \
    -DSILENCER_LAUNCHER_VERSION="$ver" \
    ${extra[@]+"${extra[@]}"} >/dev/null
  cmake --build "$dir" -j"$(ncpu)" >/dev/null
}

OLD_BUILD_DIR="${OLD_BUILD_DIR:-$REPO_ROOT/e2e-build-launcher-old}"
NEW_BUILD_DIR="${NEW_BUILD_DIR:-$REPO_ROOT/e2e-build-launcher-new}"
[ -d "$OLD_BUILD_DIR/$APP_NAME" ] || build_version "$OLD_BUILD_DIR" "$OLD_VER"
[ -d "$NEW_BUILD_DIR/$APP_NAME" ] || build_version "$NEW_BUILD_DIR" "$NEW_VER"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/silencer-launcher-updater-e2e.XXXXXX")"
HOST_DIR="$WORK/host"        # served over HTTP
INSTALL_DIR="$WORK/install"  # OLD launcher runs here; stage-2 swaps it in place
mkdir -p "$HOST_DIR" "$INSTALL_DIR"

# The identities actually baked into the two bundles — OLD_BUILD_DIR may hold
# a release build whose version is not the OLD_VER default.
NEW_BUILD_ID="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' \
  "$NEW_BUILD_DIR/$APP_NAME/Contents/Info.plist")"
OLD_BUILD_ID="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' \
  "$OLD_BUILD_DIR/$APP_NAME/Contents/Info.plist")"
[ "$OLD_BUILD_ID" != "$NEW_BUILD_ID" ] || {
  echo "FAIL: OLD and NEW share version $NEW_BUILD_ID; the banner would be ambiguous" >&2; exit 1; }

OLD_PID=""; HTTP_PID=""
cleanup() {
  [ -n "$OLD_PID" ] && kill "$OLD_PID" >/dev/null 2>&1 || true
  # The relaunched NEW build exits on its own (shot mode); sweep stragglers by
  # their scratch install path so nothing outside this run is touched.
  pkill -f "$INSTALL_DIR" >/dev/null 2>&1 || true
  [ -n "$HTTP_PID" ] && kill "$HTTP_PID" >/dev/null 2>&1 || true
  if [ "${KEEP_WORK:-0}" = 1 ]; then echo "kept scratch: $WORK" >&2; else rm -rf "$WORK"; fi
}
trap cleanup EXIT

echo "=== staging NEW ($NEW_BUILD_ID) update zip ==="
( cd "$NEW_BUILD_DIR" && ditto -ck --sequesterRsrc --keepParent "$APP_NAME" "$HOST_DIR/$ZIP_NAME" )
SHA="$(sha256_of "$HOST_DIR/$ZIP_NAME")"
echo "NEW zip: $HOST_DIR/$ZIP_NAME  sha256=$SHA"

echo "=== staging OLD ($OLD_BUILD_ID) install (stage-2 will swap this in place) ==="
ditto "$OLD_BUILD_DIR/$APP_NAME" "$INSTALL_DIR/$APP_NAME"
OLD_BIN="$INSTALL_DIR/$APP_NAME/$BIN_REL"
[ -x "$INSTALL_DIR/$APP_NAME/Contents/Helpers/updater-stage-2" ] || {
  echo "FAIL: OLD bundle has no Contents/Helpers/updater-stage-2" >&2; exit 1; }

HTTP_PORT="$(pick_port)"
HTTP_LOG="$WORK/http.log"

# The launcher takes the NEW build's identity from the manifest, exactly the
# shape release-launcher.yml's update-launcher.json step emits.
cat > "$HOST_DIR/update-launcher.json" <<EOF
{
  "version": "$NEW_BUILD_ID",
  "build_id": "$NEW_BUILD_ID",
  "channel": "stable",
  "macos_url": "http://127.0.0.1:$HTTP_PORT/$ZIP_NAME",
  "macos_sha256": "$SHA"
}
EOF

echo "=== HTTP server on :$HTTP_PORT serving $HOST_DIR ==="
( cd "$HOST_DIR" && python3 -m http.server "$HTTP_PORT" >"$HTTP_LOG" 2>&1 ) &
HTTP_PID=$!
disown "$HTTP_PID" # keep bash from narrating the cleanup kill

# The server has to be ANSWERING before the launcher is told to fetch from it.
# The game's e2e skipped this and lost the race on CI runners, where it read
# as a broken self-updater for weeks (#341/#342). Gate first, always.
MANIFEST_URL="http://127.0.0.1:$HTTP_PORT/update-launcher.json"
echo "=== waiting for the HTTP server to answer ==="
SERVED=0
for _ in $(seq 1 300); do
  if curl -fs -o /dev/null --max-time 2 "$MANIFEST_URL" 2>/dev/null; then SERVED=1; break; fi
  kill -0 "$HTTP_PID" >/dev/null 2>&1 || break
  sleep 0.1
done
if [ "$SERVED" != 1 ]; then
  echo "FAIL: the test's own HTTP server never served $MANIFEST_URL — this is the harness, not the updater" >&2
  echo "--- http server log ---" >&2; cat "$HTTP_LOG" >&2 || true
  exit 1
fi
echo "HTTP server is serving $MANIFEST_URL"

# Scratch config: every URL on loopback (404s settle instantly), the lobby at
# a refused port so the ping settles without a network.
CFG="$WORK/launcher.json"
cat > "$CFG" <<EOF
{
  "channel": "stable",
  "base_dir": "$WORK/games",
  "manifest_url_stable": "http://127.0.0.1:$HTTP_PORT/absent-stable.json",
  "manifest_url_nightly": "http://127.0.0.1:$HTTP_PORT/absent-nightly.json",
  "manifest_url_launcher": "$MANIFEST_URL",
  "announcements_url": "http://127.0.0.1:$HTTP_PORT/absent-news.json",
  "releases_url": "http://127.0.0.1:$HTTP_PORT/absent-releases.json",
  "lobby_host": "127.0.0.1",
  "lobby_port": 9
}
EOF

OLD_LOG="$WORK/launcher.log"
echo "=== launching OLD launcher (self-update fires on startup) ==="
# stderr is captured, not discarded (#342's second lesson) — and because the
# relaunched NEW build inherits the fd across stage-2's exec, its banner lands
# in the same file. SDL_VIDEODRIVER=dummy keeps the run headless (the
# offscreen driver fails window creation on macOS: it wants a GL library).
env SDL_VIDEODRIVER=dummy \
  SILENCER_LAUNCHER_CONFIG="$CFG" \
  SILENCER_LAUNCHER_SHOT="$WORK/shot.png" \
  SILENCER_LAUNCHER_TEST_SELF_UPDATE=1 \
  "$OLD_BIN" >"$OLD_LOG" 2>&1 &
OLD_PID=$!

echo "=== waiting for OLD launcher to exit (stage-2 spawned) ==="
for _ in $(seq 1 240); do kill -0 "$OLD_PID" >/dev/null 2>&1 || break; sleep 0.5; done
if kill -0 "$OLD_PID" >/dev/null 2>&1; then
  echo "FAIL: OLD launcher did not exit — stage-2 handoff never happened" >&2
  echo "--- launcher log (tail) ---" >&2; tail -40 "$OLD_LOG" >&2
  echo "--- http server log (tail) ---" >&2; tail -10 "$HTTP_LOG" >&2 || true
  exit 1
fi
OLD_PID=""
echo "OLD launcher exited; awaiting auto-relaunched NEW launcher"

echo "=== polling for the relaunched build's banner ==="
FOUND=0
for _ in $(seq 1 120); do
  if grep -q "\[launcher\] build $NEW_BUILD_ID" "$OLD_LOG"; then FOUND=1; break; fi
  sleep 0.5
done

STAGE2_LOG="${HOME:-/tmp}/Library/Logs/Silencer/update.log"
[ -f "$STAGE2_LOG" ] || STAGE2_LOG="/tmp/silencer-update.log"

if [ "$FOUND" != 1 ]; then
  echo "FAIL: relaunched launcher never reported build $NEW_BUILD_ID" >&2
  echo "--- launcher log (tail) ---" >&2; tail -40 "$OLD_LOG" >&2 || true
  echo "--- stage-2 log ($STAGE2_LOG) ---" >&2; tail -30 "$STAGE2_LOG" 2>/dev/null >&2 || true
  exit 1
fi

# The swapped bundle on disk must BE the new build, not just report as it.
SWAPPED_VER="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' \
  "$INSTALL_DIR/$APP_NAME/Contents/Info.plist")"
if [ "$SWAPPED_VER" != "$NEW_BUILD_ID" ]; then
  echo "FAIL: swapped bundle reports $SWAPPED_VER, expected $NEW_BUILD_ID" >&2
  exit 1
fi

echo
echo "PASS test-launcher-updater: $OLD_BUILD_ID self-updated and relaunched as $NEW_BUILD_ID"
exit 0
