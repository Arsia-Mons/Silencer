#!/usr/bin/env bash
# Visual demo of the stub GUI: stages a fake ~10MB update on a loopback
# server and runs the stub WITH its window, slowed by SILENCER_STUB_SLOW_MS
# so both phases are watchable (marquee "Checking..." -> "Updating... N%").
# The "launcher" that starts at the end is the tiny test payload.
#
# Usage: tests/e2e/demo.sh [slow-ms]   (default 2000)
set -uo pipefail

SLOW="${1:-2000}"
HERE="$(cd "$(dirname "$0")" && pwd)"
STUB_DIR="$(cd "$HERE/../.." && pwd)"
BUILD="$STUB_DIR/build"

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) OS=windows ;;
  Darwin) OS=macos ;;
  *) OS=linux ;;
esac

if [[ $OS == windows ]]; then
  STUB="$BUILD/silencer-launcher-stub.exe"
  PAYLOAD_SRC="$BUILD/stub-test-payload.exe"
  PAYLOAD_NAME="silencer-launcher.exe"
  ARCHIVE_EXT=zip
  native() { cygpath -w "$1"; }
elif [[ $OS == macos ]]; then
  STUB="$BUILD/silencer-launcher-stub"
  PAYLOAD_SRC="$BUILD/stub-test-payload"
  PAYLOAD_NAME="Silencer Launcher.app/Contents/MacOS/Silencer Launcher"
  ARCHIVE_EXT=zip
  native() { echo "$1"; }
else
  STUB="$BUILD/silencer-launcher-stub"
  PAYLOAD_SRC="$BUILD/stub-test-payload"
  PAYLOAD_NAME="silencer-launcher"
  ARCHIVE_EXT=tar.gz
  native() { echo "$1"; }
fi

[[ -x $STUB && -x $PAYLOAD_SRC ]] || { echo "build first (see CLAUDE.md)"; exit 1; }

TMP="$(mktemp -d)"
WWW="$TMP/www"
mkdir -p "$WWW"

cleanup() {
  [[ -n ${SERVER_PID:-} ]] && kill "$SERVER_PID" 2>/dev/null
  rm -rf "$TMP"
}
trap cleanup EXIT

sha() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# v1 = what's "installed"; v2 = the update, padded with ~10MB of random
# bytes so the download visibly crawls under the throttle.
make_version() { # <dir> <id>
  local exe="$1/$PAYLOAD_NAME"
  mkdir -p "$(dirname "$exe")"
  cp "$PAYLOAD_SRC" "$exe"
  chmod +x "$exe"
  echo "$2" > "$(dirname "$exe")/build-id.txt"
}
STORE="$TMP/store"
make_version "$STORE/00001" 00001
echo 00001 > "$STORE/current.txt"
V2="$TMP/v2"
make_version "$V2" 00002
head -c 10000000 /dev/urandom > "$V2/pad.bin"

if [[ $OS == windows ]]; then
  "/c/Windows/System32/tar.exe" -a -c -f "$(native "$TMP/v2.zip")" -C "$(native "$V2")" .
elif [[ $OS == macos ]]; then
  ditto -c -k "$V2" "$TMP/v2.zip"
else
  tar -czf "$TMP/v2.tar.gz" -C "$V2" .
fi

for _ in 1 2 3 4 5; do
  PORT=$(( (RANDOM % 20000) + 20000 ))
  bun "$HERE/serve.ts" "$(native "$WWW")" "$PORT" &
  SERVER_PID=$!
  cp "$TMP/v2.$ARCHIVE_EXT" "$WWW/payload.$ARCHIVE_EXT"
  printf '{"build_id": "00002", "%s_payload_url": "http://127.0.0.1:%s/payload.%s", "%s_payload_sha256": "%s"}\n' \
    "$OS" "$PORT" "$ARCHIVE_EXT" "$OS" "$(sha "$TMP/v2.$ARCHIVE_EXT")" > "$WWW/manifest.json"
  ok=""
  for _ in $(seq 1 30); do
    curl -fsS "http://127.0.0.1:$PORT/manifest.json" >/dev/null 2>&1 && { ok=1; break; }
    kill -0 "$SERVER_PID" 2>/dev/null || break
    sleep 0.1
  done
  [[ -n $ok ]] && break
  kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null; SERVER_PID=""
done
[[ -n ${SERVER_PID:-} ]] || { echo "server never came up"; exit 1; }

echo "watch the window: ~$((SLOW / 1000))s of Checking (marquee), then Updating with a live percent bar"
SILENCER_STUB_STORE="$(native "$STORE")" \
SILENCER_STUB_MANIFEST_URL="http://127.0.0.1:$PORT/manifest.json" \
SILENCER_STUB_SLOW_MS="$SLOW" \
"$STUB"
echo "stub exited ($?); store ended at version $(head -1 "$STORE/current.txt" | tr -d '\r')"
