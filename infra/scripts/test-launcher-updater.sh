#!/usr/bin/env bash
# Automated end-to-end launcher self-update test (headless; runs in CI).
#
# Proves the FULL stub-based launcher self-update path (issue #347): the
# shipped "Silencer Launcher.app" is the bootstrap stub's bundle; run it and
# it fetches its manifest over loopback HTTP, sees a newer build_id, downloads
# the payload zip, verifies its sha256 (the Developer ID check skips on an
# unsigned build), extracts into its versioned store, flips current.txt, and
# launches the NEW payload — confirmed by the "[launcher] build <id>" banner
# the payload writes to the inherited stderr.
#
# Then the fail-safe half, which is the property this architecture exists
# for: a hostile payload archive (../ members, no payload app) must neither
# escape the staging dir nor break the install — the previously-updated
# version must still launch.
#
# This gate matters more than the game's: a broken game updater can be
# repaired by the launcher, but a broken launcher self-update leaves the user
# re-downloading the DMG by hand.
#
# Env knobs (all optional):
#   NEW_VER (default 99999)       — the payload build the update installs
#   OLD_BUILD_DIR — a prebuilt COMPOSED stub-first bundle dir (CI passes the
#                   release build-launcher/). Without it, launcher + stub are
#                   built and composed here.
#   NEW_BUILD_DIR — reuse a prebuilt NEW-payload launcher build dir.
#   KEEP_WORK=1   — leave the scratch dir for inspection
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NEW_VER="${NEW_VER:-99999}"
APP_NAME="Silencer Launcher.app"
BIN_REL="Contents/MacOS/Silencer Launcher"
PAYLOAD_REL="Contents/Resources/payload/$APP_NAME"

case "$(uname)" in
  Darwin) ;;
  *) echo "test-launcher-updater.sh: macOS only" >&2; exit 1 ;;
esac

ncpu()      { sysctl -n hw.ncpu; }
sha256_of() { shasum -a 256 "$1" | awk '{print $1}'; }
pick_port() {
  python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()'
}

sccache_args() {
  if [ "${SCCACHE_GHA_ENABLED:-}" = "true" ] && command -v sccache >/dev/null 2>&1; then
    echo "-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache"
  fi
}

build_launcher() {  # <dir> <version>
  echo "=== building launcher $2 -> $1 ===" >&2
  # SDL3_ROOT (exported by setup-sdl in CI) outranks the Homebrew prefix in
  # find_package's search order, so this one configure works in both places.
  cmake -B "$1" -S "$REPO_ROOT/clients/launcher" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/homebrew \
    -DSILENCER_LAUNCHER_VERSION="$2" \
    $(sccache_args) >/dev/null
  cmake --build "$1" -j"$(ncpu)" >/dev/null
}

# Local fallback when no composed bundle was handed in: build launcher + stub
# and compose the stub-first bundle the way build-launcher-macos does.
compose_old() {  # <dir>
  build_launcher "$1" "00023"
  echo "=== building stub -> $1-stub ===" >&2
  cmake -B "$1-stub" -S "$REPO_ROOT/clients/launcher-stub" \
    -DCMAKE_BUILD_TYPE=Release $(sccache_args) >/dev/null
  cmake --build "$1-stub" -j"$(ncpu)" >/dev/null
  mkdir -p "$1/payload-stage"
  mv "$1/$APP_NAME" "$1/payload-stage/$APP_NAME"
  ditto "$1-stub/$APP_NAME" "$1/$APP_NAME"
  mkdir -p "$1/$APP_NAME/Contents/Resources/payload"
  ditto "$1/payload-stage/$APP_NAME" "$1/$APP_NAME/$PAYLOAD_REL"
}

OLD_BUILD_DIR="${OLD_BUILD_DIR:-$REPO_ROOT/e2e-build-launcher-old}"
NEW_BUILD_DIR="${NEW_BUILD_DIR:-$REPO_ROOT/e2e-build-launcher-new}"
[ -x "$OLD_BUILD_DIR/$APP_NAME/$PAYLOAD_REL/$BIN_REL" ] || compose_old "$OLD_BUILD_DIR"
[ -d "$NEW_BUILD_DIR/$APP_NAME" ] || build_launcher "$NEW_BUILD_DIR" "$NEW_VER"

NEW_BUILD_ID="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' \
  "$NEW_BUILD_DIR/$APP_NAME/Contents/Info.plist")"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/silencer-launcher-updater-e2e.XXXXXX")"
HOST_DIR="$WORK/host"        # served over HTTP
INSTALL_DIR="$WORK/install"  # the composed stub-first bundle runs from here
STORE_DIR="$WORK/store"      # the stub's versioned store (env-pinned)
mkdir -p "$HOST_DIR" "$INSTALL_DIR" "$STORE_DIR"

HTTP_PID=""
cleanup() {
  pkill -f "$WORK" >/dev/null 2>&1 || true
  [ -n "$HTTP_PID" ] && kill "$HTTP_PID" >/dev/null 2>&1 || true
  if [ "${KEEP_WORK:-0}" = 1 ]; then echo "kept scratch: $WORK" >&2; else rm -rf "$WORK"; fi
}
trap cleanup EXIT

echo "=== staging install (composed stub-first bundle) ==="
ditto "$OLD_BUILD_DIR/$APP_NAME" "$INSTALL_DIR/$APP_NAME"
STUB_BIN="$INSTALL_DIR/$APP_NAME/$BIN_REL"
[ -x "$STUB_BIN" ] || { echo "FAIL: no stub binary at $STUB_BIN" >&2; exit 1; }
[ -x "$INSTALL_DIR/$APP_NAME/$PAYLOAD_REL/$BIN_REL" ] || {
  echo "FAIL: no seed payload inside the bundle" >&2; exit 1; }

echo "=== staging NEW ($NEW_BUILD_ID) payload zip ==="
( cd "$NEW_BUILD_DIR" && ditto -ck --sequesterRsrc --keepParent "$APP_NAME" \
    "$HOST_DIR/payload.zip" )
PAYLOAD_SHA="$(sha256_of "$HOST_DIR/payload.zip")"

HTTP_PORT="$(pick_port)"
HTTP_LOG="$WORK/http.log"

write_manifest() {  # <build-id> <zip-name> <sha>
  cat > "$HOST_DIR/update-launcher.json" <<EOF
{
  "build_id": "$1",
  "macos_payload_url": "http://127.0.0.1:$HTTP_PORT/$2",
  "macos_payload_sha256": "$3"
}
EOF
}
write_manifest "$NEW_BUILD_ID" payload.zip "$PAYLOAD_SHA"

echo "=== HTTP server on :$HTTP_PORT serving $HOST_DIR ==="
( cd "$HOST_DIR" && python3 -m http.server "$HTTP_PORT" >"$HTTP_LOG" 2>&1 ) &
HTTP_PID=$!
disown "$HTTP_PID"

# The server has to be ANSWERING before the stub is told to fetch from it
# (#341/#342). Gate first, always.
MANIFEST_URL="http://127.0.0.1:$HTTP_PORT/update-launcher.json"
SERVED=0
for _ in $(seq 1 300); do
  if curl -fs -o /dev/null --max-time 2 "$MANIFEST_URL" 2>/dev/null; then SERVED=1; break; fi
  kill -0 "$HTTP_PID" >/dev/null 2>&1 || break
  sleep 0.1
done
if [ "$SERVED" != 1 ]; then
  echo "FAIL: the test's own HTTP server never served $MANIFEST_URL — this is the harness, not the updater" >&2
  cat "$HTTP_LOG" >&2 || true
  exit 1
fi

# Scratch launcher config: every URL on loopback (404s settle instantly), the
# lobby at a refused port so the ping settles without a network.
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

run_stub() {  # <log-file>
  # The payload inherits the whole environment across the stub's fork, so the
  # launcher-side knobs ride along: SDL_VIDEODRIVER=dummy keeps it headless,
  # SILENCER_LAUNCHER_SHOT makes it render offscreen and exit on its own —
  # and its stderr lands in the same captured log as the stub's.
  env SILENCER_STUB_NO_GUI=1 \
    SILENCER_STUB_STORE="$STORE_DIR" \
    SILENCER_STUB_MANIFEST_URL="$MANIFEST_URL" \
    SDL_VIDEODRIVER=dummy \
    SILENCER_LAUNCHER_CONFIG="$CFG" \
    SILENCER_LAUNCHER_SHOT="$WORK/shot.png" \
    "$STUB_BIN" >"$1" 2>&1
}

await_banner() {  # <log-file> <build-id>
  for _ in $(seq 1 120); do
    if grep -q "\[launcher\] build $2" "$1"; then return 0; fi
    sleep 0.5
  done
  return 1
}

echo "=== case 1: stub installs and launches the $NEW_BUILD_ID payload ==="
LOG1="$WORK/run1.log"
run_stub "$LOG1"
if ! await_banner "$LOG1" "$NEW_BUILD_ID"; then
  echo "FAIL: payload never reported build $NEW_BUILD_ID" >&2
  tail -40 "$LOG1" >&2 || true
  exit 1
fi
CURRENT="$(head -1 "$STORE_DIR/current.txt" 2>/dev/null || true)"
[ "$CURRENT" = "$NEW_BUILD_ID" ] || {
  echo "FAIL: store current.txt is '$CURRENT', expected $NEW_BUILD_ID" >&2; exit 1; }
[ -x "$STORE_DIR/$NEW_BUILD_ID/$APP_NAME/$BIN_REL" ] || {
  echo "FAIL: installed payload app missing from the store" >&2; exit 1; }
echo "case 1 PASS: updated to $NEW_BUILD_ID and it launched"

echo "=== case 2: hostile archive (../ members) neither escapes nor breaks ==="
python3 - "$HOST_DIR/evil.zip" <<'EOF'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1], "w")
z.writestr("../../evil-escape.txt", "escaped")
z.writestr("../../../evil-escape-deeper.txt", "escaped")
z.writestr("junk/readme.txt", "no payload app in here")
z.close()
EOF
EVIL_SHA="$(sha256_of "$HOST_DIR/evil.zip")"
write_manifest "99998" evil.zip "$EVIL_SHA"
sleep 1  # let case 1's payload finish writing before the next run reuses env
LOG2="$WORK/run2.log"
run_stub "$LOG2"
if ! await_banner "$LOG2" "$NEW_BUILD_ID"; then
  echo "FAIL: after the hostile archive, the existing $NEW_BUILD_ID never launched" >&2
  tail -40 "$LOG2" >&2 || true
  exit 1
fi
CURRENT="$(head -1 "$STORE_DIR/current.txt" 2>/dev/null || true)"
[ "$CURRENT" = "$NEW_BUILD_ID" ] || {
  echo "FAIL: hostile archive moved current.txt to '$CURRENT'" >&2; exit 1; }
ESCAPED="$(find "$WORK" -name 'evil-escape*' -print 2>/dev/null || true)"
if [ -n "$ESCAPED" ]; then
  echo "FAIL: hostile archive escaped the staging dir:" >&2
  echo "$ESCAPED" >&2
  exit 1
fi
echo "case 2 PASS: hostile archive discarded; $NEW_BUILD_ID still launches"

echo
echo "PASS test-launcher-updater: stub updated to $NEW_BUILD_ID and survived a hostile archive"
exit 0
