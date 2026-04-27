#!/usr/bin/env bash
# Sourced by every E2E scenario.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

# Cross-platform binary detection: macOS uses an .app bundle.
if [ -z "${SILENCER_BIN:-}" ]; then
  if [ -x "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" ]; then
    SILENCER_BIN="$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
  elif [ -x "$REPO_ROOT/clients/silencer/build/silencer" ]; then
    SILENCER_BIN="$REPO_ROOT/clients/silencer/build/silencer"
  elif [ -x "$REPO_ROOT/clients/silencer/build/Silencer.exe" ]; then
    SILENCER_BIN="$REPO_ROOT/clients/silencer/build/Silencer.exe"
  else
    echo "no silencer binary found under clients/silencer/build/" >&2
    exit 1
  fi
fi

CLI="bun $REPO_ROOT/clients/cli/index.ts"

pick_port() {
  # Random ephemeral.
  python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()'
}

start_silencer() {
  local port="$1"
  "$SILENCER_BIN" --headless --control-port "$port" >"/tmp/silencer-e2e-$port.log" 2>&1 &
  echo $!
}

wait_alive() {
  local port="$1"
  for i in $(seq 1 60); do
    if $CLI --port "$port" ping >/dev/null 2>&1; then return 0; fi
    sleep 0.5
  done
  echo "silencer on $port never came up" >&2
  return 1
}

stop_silencer() {
  local pid="$1" port="${2:-}"
  if [ -n "$port" ]; then
    $CLI --port "$port" quit >/dev/null 2>&1 || true
  fi
  sleep 0.3
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}
