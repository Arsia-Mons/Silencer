#!/usr/bin/env bash
# Headless e2e for the launcher bootstrap stub. Drives the real stub binary
# against a loopback manifest server and asserts the fail-safe contract:
# every failure path still launches the existing version.
#
# Works on Windows (Git Bash), macOS and Linux; needs bun (server) and curl
# (readiness probe). Build first:
#   Windows:  clients/launcher-stub/build.ps1
#   mac/linux: cmake -S clients/launcher-stub -B clients/launcher-stub/build \
#              -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build clients/launcher-stub/build
#
# Usage: tests/e2e/run.sh [build-dir]   (default: ../../build)
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
STUB_DIR="$(cd "$HERE/../.." && pwd)"
BUILD="${1:-$STUB_DIR/build}"

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

[[ -x $STUB ]] || { echo "missing $STUB - build first"; exit 1; }
[[ -x $PAYLOAD_SRC ]] || { echo "missing $PAYLOAD_SRC - build first"; exit 1; }

TMP="$(mktemp -d)"
WWW="$TMP/www"
OUT="$TMP/out"
mkdir -p "$WWW" "$OUT"
PORT=$(( (RANDOM % 20000) + 20000 ))
FAILED=0

cleanup() {
  [[ -n ${SERVER_PID:-} ]] && kill "$SERVER_PID" 2>/dev/null
  rm -rf "$TMP"
}
trap cleanup EXIT

sha() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# make_version <dir> <id> [exit-code] - a fake payload version: the test
# payload binary + the control files it reads from beside itself.
make_version() {
  local dir=$1 id=$2 code=${3:-}
  local exe="$dir/$PAYLOAD_NAME"
  mkdir -p "$(dirname "$exe")"
  cp "$PAYLOAD_SRC" "$exe"
  chmod +x "$exe"
  echo "$id" > "$(dirname "$exe")/build-id.txt"
  if [[ -n $code ]]; then echo "$code" > "$(dirname "$exe")/exit-code.txt"; fi
}

# make_archive <version-dir> <out-file> - archive whose root is the version
# dir's contents, in the format the stub extracts on this OS.
make_archive() {
  local dir=$1 out=$2
  if [[ $OS == windows ]]; then
    "/c/Windows/System32/tar.exe" -a -c -f "$(native "$out")" -C "$(native "$dir")" .
  elif [[ $OS == macos ]]; then
    ditto -c -k "$dir" "$out"
  else
    tar -czf "$out" -C "$dir" .
  fi
}

# write_manifest <build-id> [archive-file] [sha-override]
write_manifest() {
  local id=$1 file=${2:-} shaov=${3:-}
  local url="" s=""
  if [[ -n $file ]]; then
    cp "$file" "$WWW/payload.$ARCHIVE_EXT"
    url="http://127.0.0.1:$PORT/payload.$ARCHIVE_EXT"
    s=${shaov:-$(sha "$file")}
  fi
  local plat=$OS # windows|macos|linux, matching the manifest keys
  printf '{"build_id": "%s", "%s_url": "%s", "%s_sha256": "%s"}\n' \
    "$id" "$plat" "$url" "$plat" "$s" > "$WWW/manifest.json"
}

# run_stub <store> [manifest-url]
run_stub() {
  local store=$1 url=${2:-"http://127.0.0.1:$PORT/manifest.json"}
  SILENCER_STUB_STORE="$(native "$store")" \
  SILENCER_STUB_MANIFEST_URL="$url" \
  SILENCER_STUB_NO_GUI=1 \
  SILENCER_STUB_TEST_OUT="$(native "$OUT")" \
  "$STUB"
}

ran() { tr -d '\r' < "$OUT/ran.txt" 2>/dev/null | tr '\n' ' ' | sed 's/ *$//'; }
cur() { head -1 "$1/current.txt" 2>/dev/null | tr -d '\r'; }
reset_out() { rm -f "$OUT/ran.txt"; }

assert_eq() { # <desc> <expected> <actual>
  if [[ "$2" == "$3" ]]; then echo "  ok: $1"
  else echo "  FAIL: $1 - expected [$2] got [$3]"; FAILED=1; fi
}
assert_absent() { # <desc> <path>
  if [[ ! -e "$2" ]]; then echo "  ok: $1"
  else echo "  FAIL: $1 - $2 exists"; FAILED=1; fi
}

# --- server up before anything drives the stub (the #341/#342 lesson) ---
write_manifest 00000
bun "$HERE/serve.ts" "$(native "$WWW")" "$PORT" &
SERVER_PID=$!
for _ in $(seq 1 50); do
  curl -fsS "http://127.0.0.1:$PORT/manifest.json" >/dev/null 2>&1 && break
  sleep 0.1
done
curl -fsS "http://127.0.0.1:$PORT/manifest.json" >/dev/null 2>&1 || {
  echo "server never came up"; exit 1; }

echo "=== case: up to date - no download, current version runs"
S="$TMP/s-uptodate"
make_version "$S/00001" 00001
echo 00001 > "$S/current.txt"
write_manifest 00001
reset_out
run_stub "$S"; sleep 1
assert_eq "payload ran" "00001" "$(ran)"
assert_eq "current unchanged" "00001" "$(cur "$S")"

echo "=== case: update applies - download, verify, flip, new version runs"
S="$TMP/s-update"
make_version "$S/00001" 00001
echo 00001 > "$S/current.txt"
V2="$TMP/v2"
make_version "$V2" 00002
make_archive "$V2" "$TMP/v2.$ARCHIVE_EXT"
write_manifest 00002 "$TMP/v2.$ARCHIVE_EXT"
reset_out
run_stub "$S"; sleep 1
assert_eq "new payload ran" "00002" "$(ran)"
assert_eq "current flipped" "00002" "$(cur "$S")"
assert_eq "previous recorded" "00001" "$(head -1 "$S/previous.txt" | tr -d '\r')"
assert_absent "staging cleaned" "$S/staging"

echo "=== case: server down - existing version still launches, quickly"
S="$TMP/s-down"
make_version "$S/00001" 00001
echo 00001 > "$S/current.txt"
reset_out
t0=$SECONDS
run_stub "$S" "http://127.0.0.1:1/manifest.json"; sleep 1
elapsed=$(( SECONDS - t0 ))
assert_eq "payload ran" "00001" "$(ran)"
assert_eq "current unchanged" "00001" "$(cur "$S")"
if (( elapsed <= 10 )); then echo "  ok: fast fallback (${elapsed}s)"
else echo "  FAIL: fallback took ${elapsed}s"; FAILED=1; fi

echo "=== case: checksum mismatch - download discarded, existing version runs"
S="$TMP/s-badsha"
make_version "$S/00001" 00001
echo 00001 > "$S/current.txt"
write_manifest 00002 "$TMP/v2.$ARCHIVE_EXT" "deadbeef$(printf '0%.0s' $(seq 1 56))"
reset_out
run_stub "$S"; sleep 1
assert_eq "payload ran" "00001" "$(ran)"
assert_eq "current unchanged" "00001" "$(cur "$S")"
assert_absent "bad version not installed" "$S/00002"
assert_absent "staging cleaned" "$S/staging"

echo "=== case: rollback - fresh update dies at startup, previous relaunches"
S="$TMP/s-rollback"
make_version "$S/00001" 00001
echo 00001 > "$S/current.txt"
V2B="$TMP/v2bad"
make_version "$V2B" 00002 1
make_archive "$V2B" "$TMP/v2bad.$ARCHIVE_EXT"
write_manifest 00002 "$TMP/v2bad.$ARCHIVE_EXT"
reset_out
run_stub "$S"; sleep 1
assert_eq "bad version ran then previous relaunched" "00002 00001" "$(ran)"
assert_eq "current rolled back" "00001" "$(cur "$S")"

echo "=== case: first run - empty store bootstraps from the manifest"
S="$TMP/s-firstrun"
mkdir -p "$S"
write_manifest 00002 "$TMP/v2.$ARCHIVE_EXT"
reset_out
run_stub "$S"; sleep 1
assert_eq "payload ran" "00002" "$(ran)"
assert_eq "current set" "00002" "$(cur "$S")"

if (( FAILED )); then echo "E2E: FAILED"; exit 1; fi
echo "E2E: all cases passed"
