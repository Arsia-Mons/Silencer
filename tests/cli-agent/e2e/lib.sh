#!/usr/bin/env bash
# Sourced by every E2E scenario.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

candidate_mtime() {
  local path="$1"
  stat -f %m "$path" 2>/dev/null || stat -c %Y "$path" 2>/dev/null || echo 0
}

silencer_binary_candidates() {
  printf '%s|%s\n' \
    "$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer" "$REPO_ROOT/build" \
    "$REPO_ROOT/build/silencer" "$REPO_ROOT/build" \
    "$REPO_ROOT/build/Silencer.exe" "$REPO_ROOT/build" \
    "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" "$REPO_ROOT/clients/silencer/build" \
    "$REPO_ROOT/clients/silencer/build/silencer" "$REPO_ROOT/clients/silencer/build" \
    "$REPO_ROOT/clients/silencer/build/Silencer.exe" "$REPO_ROOT/clients/silencer/build" \
    "$REPO_ROOT/clients/silencer/build-unity/Silencer.exe" "$REPO_ROOT/clients/silencer/build-unity" \
    "$REPO_ROOT/clients/silencer/build-release/Silencer.exe" "$REPO_ROOT/clients/silencer/build-release"
}

find_silencer_build() {
  local best_bin="" best_dir="" best_mtime=0 candidate build_dir mtime
  # Cross-platform binary detection: macOS uses an .app bundle. On
  # Windows the win-ninja-unity preset writes to build-unity/. Pick the
  # newest build identity so stale root/worktree builds do not shadow
  # fresh ones, and keep the binary paired with its matching CMake cache.
  while IFS='|' read -r candidate build_dir; do
    [ -x "$candidate" ] || continue
    mtime="$(candidate_mtime "$candidate")"
    if [ -z "$best_bin" ] || [ "$mtime" -gt "$best_mtime" ]; then
      best_bin="$candidate"
      best_dir="$build_dir"
      best_mtime="$mtime"
    fi
  done <<EOF
$(silencer_binary_candidates)
EOF
  [ -n "$best_bin" ] || return 1
  printf '%s|%s\n' "$best_bin" "$best_dir"
}

infer_silencer_build_dir() {
  local bin="$1"
  case "$bin" in
    */Silencer.app/Contents/MacOS/Silencer)
      (cd "$(dirname "$bin")/../../.." && pwd)
      ;;
    *)
      (cd "$(dirname "$bin")" && pwd)
      ;;
  esac
}

read_silencer_version() {
  local build_dir="$1"
  local cache="$build_dir/CMakeCache.txt"
  [ -f "$cache" ] || return 1
  awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache"
}

if [ -z "${SILENCER_BIN:-}" ]; then
  if resolved="$(find_silencer_build)"; then
    SILENCER_BIN="${resolved%%|*}"
    SILENCER_BUILD_DIR="${resolved#*|}"
  else
    echo "no silencer binary found under build*/ or clients/silencer/build*/" >&2
    exit 1
  fi
fi
SILENCER_BUILD_DIR="${SILENCER_BUILD_DIR:-$(infer_silencer_build_dir "$SILENCER_BIN")}"
SILENCER_VERSION="${SILENCER_VERSION:-$(read_silencer_version "$SILENCER_BUILD_DIR" || true)}"
if [ -z "$SILENCER_VERSION" ]; then
  echo "could not read SILENCER_VERSION from $SILENCER_BUILD_DIR/CMakeCache.txt" >&2
  exit 1
fi
export SILENCER_BIN SILENCER_BUILD_DIR SILENCER_VERSION

# Function (not a variable) so $REPO_ROOT can contain spaces — assigning
# `CLI="bun $REPO_ROOT/.../index.ts"` and then unquoted `$CLI` word-splits the
# path on whitespace. With a function, "$@" carries args verbatim.
cli() {
  bun "$REPO_ROOT/clients/cli/index.ts" "$@"
}

pick_port() {
  # Random ephemeral. Bun (already required by cli()) avoids a python3 dep.
  bun -e 'const s = Bun.listen({hostname:"127.0.0.1",port:0,socket:{data(){}}}); console.log(s.port); s.stop();'
}

start_silencer() {
  local port="$1"
  "$SILENCER_BIN" --headless --control-port "$port" >"/tmp/silencer-e2e-$port.log" 2>&1 &
  echo $!
}

wait_alive() {
  local port="$1"
  for i in $(seq 1 60); do
    if cli --port "$port" ping >/dev/null 2>&1; then return 0; fi
    sleep 0.5
  done
  echo "silencer on $port never came up" >&2
  return 1
}

stop_silencer() {
  local pid="$1" port="${2:-}"
  if [ -n "$port" ]; then
    cli --port "$port" quit >/dev/null 2>&1 || true
  fi
  sleep 0.3
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}
