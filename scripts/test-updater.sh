#!/usr/bin/env bash
# Failure-case testing:
#   corrupt sha:       edit update.json → flip a hex digit → ./scripts/test-updater.sh
#   network drop:      kill the python server mid-download (ps ax | grep http.server)
#   http 404:          rm test-update-host/*.zip before launching client
#   read-only install: chmod -w build-old/ before launching client
set -euo pipefail

OLD_VER=${OLD_VER:-00023}
NEW_VER=${NEW_VER:-00024}
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

case "$(uname)" in
    Darwin) PLATFORM_ZIP=silencer-macos-arm64.zip ;;
    Linux)  PLATFORM_ZIP=silencer-linux-x64.zip ;;
    *) echo "unsupported uname: $(uname)"; exit 1 ;;
esac

echo "=== Building NEW version ($NEW_VER) ==="
cmake -B build-new -S clients/silencer \
    -DSILENCER_VERSION="$NEW_VER" \
    -DSILENCER_LOBBY_HOST=127.0.0.1 \
    -DSILENCER_LOBBY_PORT=15170
cmake --build build-new -j

mkdir -p test-update-host
rm -f "test-update-host/$PLATFORM_ZIP"

# Package to match production release.yml's zip layout so stage-2's
# extract + wrapper-unwrap + atomic-rename path runs against the real
# zip shape (not a dev-only variant that hides bugs).
case "$(uname)" in
    Darwin)
        # release.yml uses `ditto -ck --sequesterRsrc --keepParent Silencer.app <zip>`.
        # --keepParent puts `Silencer.app/` at the zip root. Stage-2 detects
        # this single-top-dir wrapper and hoists it into place.
        (cd build-new && ditto -ck --sequesterRsrc --keepParent Silencer.app "../test-update-host/$PLATFORM_ZIP")
        ;;
    Linux)
        # Linux isn't a shipped platform; bare binary at zip root is fine.
        (cd build-new && zip -r "../test-update-host/$PLATFORM_ZIP" silencer)
        ;;
esac

SHA=$(shasum -a 256 "test-update-host/$PLATFORM_ZIP" | awk '{print $1}')
echo "NEW zip sha256=$SHA"

echo "=== Building OLD version ($OLD_VER) ==="
cmake -B build-old -S clients/silencer \
    -DSILENCER_VERSION="$OLD_VER" \
    -DSILENCER_LOBBY_HOST=127.0.0.1 \
    -DSILENCER_LOBBY_PORT=15170
cmake --build build-old -j

cat > update.json <<EOF
{
  "version":        "$NEW_VER",
  "macos_url":      "http://127.0.0.1:8000/$PLATFORM_ZIP",
  "macos_sha256":   "$SHA",
  "windows_url":    "http://127.0.0.1:8000/$PLATFORM_ZIP",
  "windows_sha256": "$SHA"
}
EOF

echo "=== Starting HTTP server on :8000 ==="
( cd test-update-host && python3 -m http.server 8000 ) &
HTTP_PID=$!

echo "=== Starting lobby on :15170 ==="
( cd server && go build )
./server/zsilencer-lobby -addr :15170 -version "$NEW_VER" -update-manifest "$REPO_ROOT/update.json" &
LOBBY_PID=$!

cleanup() {
    kill $HTTP_PID $LOBBY_PID 2>/dev/null || true
}
trap cleanup EXIT

sleep 1
echo
echo "=== Launching OLD client — expect update modal ==="
case "$(uname)" in
    Darwin)
        # MACOSX_BUNDLE target produces build-old/Silencer.app, not a bare binary.
        # Invoke the binary directly so stderr/stdout stream into this terminal
        # (`open` detaches and hides logs).
        ./build-old/Silencer.app/Contents/MacOS/Silencer
        ;;
    Linux)
        ./build-old/silencer
        ;;
esac
