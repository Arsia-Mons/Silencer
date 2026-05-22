#!/usr/bin/env bash
# Sourced by every E2E scenario.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

# Cross-platform binary detection: macOS uses an .app bundle. On
# Windows the win-ninja-unity preset writes to build-unity/.
# Prefer the worktree-root build/ first — in worktrees the canonical
# build lives there and clients/silencer/build/ may be a stale leftover
# from a pre-worktree checkout (see progress.txt P19 / P20).
if [ -z "${SILENCER_BIN:-}" ]; then
  newest_bin=""
  for candidate in \
    "$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer" \
    "$REPO_ROOT/build/silencer" \
    "$REPO_ROOT/build/Silencer.exe" \
    "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" \
    "$REPO_ROOT/clients/silencer/build/silencer" \
    "$REPO_ROOT/clients/silencer/build/Silencer.exe" \
    "$REPO_ROOT/clients/silencer/build-unity/Silencer.exe" \
    "$REPO_ROOT/clients/silencer/build-release/Silencer.exe"; do
    if [ -x "$candidate" ] && { [ -z "$newest_bin" ] || [ "$candidate" -nt "$newest_bin" ]; }; then
      newest_bin="$candidate"
    fi
  done
  if [ -n "$newest_bin" ]; then
    SILENCER_BIN="$newest_bin"
  else
    echo "no silencer binary found under build*/ or clients/silencer/build*/" >&2
    exit 1
  fi
fi

silencer_build_dir_for_bin() {
  case "$1" in
    */Silencer.app/Contents/MacOS/Silencer)
      cd "$(dirname "$1")/../../.." && pwd
      ;;
    *)
      dirname "$1"
      ;;
  esac
}

SILENCER_BUILD_DIR="$(silencer_build_dir_for_bin "$SILENCER_BIN")"

silencer_version() {
  if [ -n "${SILENCER_VERSION:-}" ]; then
    printf '%s\n' "$SILENCER_VERSION"
    return 0
  fi
  local cache="$SILENCER_BUILD_DIR/CMakeCache.txt"
  if [ ! -f "$cache" ]; then
    echo "could not read SILENCER_VERSION; selected binary has no CMakeCache.txt at $cache" >&2
    return 1
  fi
  local version
  version="$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")"
  if [ -z "$version" ]; then
    echo "could not read SILENCER_VERSION from $cache" >&2
    return 1
  fi
  printf '%s\n' "$version"
}

lobby_bin() {
  local newest_bin=""
  for candidate in \
    "$REPO_ROOT/services/lobby/lobby" \
    "$REPO_ROOT/services/lobby/lobby.exe" \
    "$REPO_ROOT/services/lobby/silencer-lobby" \
    "$REPO_ROOT/services/lobby/silencer-lobby.exe"; do
    if [ -x "$candidate" ] && { [ -z "$newest_bin" ] || [ "$candidate" -nt "$newest_bin" ]; }; then
      newest_bin="$candidate"
    fi
  done
  if [ -z "$newest_bin" ]; then
    echo "lobby binary missing under services/lobby/ — build it via 'cd services/lobby && go build -o lobby'" >&2
    return 1
  fi
  local src base
  for src in "$REPO_ROOT"/services/lobby/*.go; do
    base="$(basename "$src")"
    case "$base" in
      *_test.go) continue ;;
    esac
    if [ "$src" -nt "$newest_bin" ]; then
      echo "lobby binary $newest_bin is older than $src — rebuild via 'cd services/lobby && go build -o lobby'" >&2
      return 1
    fi
  done
  printf '%s\n' "$newest_bin"
}

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

create_initial_character() {
  local alias="${1:-Agent}"
  cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
  wait_for_widget "Create New Character"
  cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
  wait_for_widget "Alias"
  cli --port "$CTRL_PORT" set_text --label "Alias" --text "$alias" >/dev/null
  cli --port "$CTRL_PORT" key --key enter >/dev/null
  cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
  wait_for_widget "Noxis"
  cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
  cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
}
