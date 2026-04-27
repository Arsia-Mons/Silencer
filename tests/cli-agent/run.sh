#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAIL=0
for s in "$DIR"/e2e/[0-9]*_*.sh; do
  echo "=== $s ==="
  if bash "$s"; then
    echo "  ok"
  else
    echo "  FAIL"
    FAIL=$((FAIL+1))
  fi
done
if [ "$FAIL" -ne 0 ]; then
  echo "$FAIL scenario(s) failed" >&2
  exit 1
fi
echo "all green"
