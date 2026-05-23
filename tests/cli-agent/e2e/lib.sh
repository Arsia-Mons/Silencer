#!/usr/bin/env bash
# Sourced by every E2E scenario.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

candidate_mtime() {
  local path="$1"
  stat -f %m "$path" 2>/dev/null || stat -c %Y "$path" 2>/dev/null || echo 0
}

find_silencer_bin() {
  local best="" best_mtime=0 candidate mtime
  # Cross-platform binary detection: macOS uses an .app bundle. On
  # Windows the win-ninja-unity preset writes to build-unity/. Pick the
  # newest binary so stale root/worktree builds do not shadow fresh ones.
  for candidate in \
    "$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer" \
    "$REPO_ROOT/build/silencer" \
    "$REPO_ROOT/build/Silencer.exe" \
    "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" \
    "$REPO_ROOT/clients/silencer/build/silencer" \
    "$REPO_ROOT/clients/silencer/build/Silencer.exe" \
    "$REPO_ROOT/clients/silencer/build-unity/Silencer.exe" \
    "$REPO_ROOT/clients/silencer/build-release/Silencer.exe"; do
    [ -x "$candidate" ] || continue
    mtime="$(candidate_mtime "$candidate")"
    if [ -z "$best" ] || [ "$mtime" -gt "$best_mtime" ]; then
      best="$candidate"
      best_mtime="$mtime"
    fi
  done
  [ -n "$best" ] || return 1
  printf '%s\n' "$best"
}

if [ -z "${SILENCER_BIN:-}" ]; then
  if SILENCER_BIN="$(find_silencer_bin)"; then
    export SILENCER_BIN
  else
    echo "no silencer binary found under build*/ or clients/silencer/build*/" >&2
    exit 1
  fi
fi

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
