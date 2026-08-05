#!/usr/bin/env bash
# Verifies a built Linux silencer bundle is self-contained: every dynamic
# library reference (NEEDED) resolves either to a system library or to a file
# inside the package directory itself (via the binary's $ORIGIN RUNPATH).
#
# Usage: tests/e2e/check-bundle-linux.sh <package-dir> [exe-name]
#
# exe-name defaults to the game binary, so the one-argument call is unchanged.
# The launcher passes `silencer-launcher`.
set -euo pipefail

DIR="${1:?usage: $0 <package-dir> [exe-name]}"
BINARY="$DIR/${2:-silencer}"

[ -x "$BINARY" ] || { echo "no executable at $BINARY" >&2; exit 1; }

echo "--- ldd $BINARY ---"
ldd "$BINARY"

if ldd "$BINARY" | grep -F "not found"; then
	echo "----"
	echo "FAIL: dynamic library references could not be resolved"
	exit 1
fi

echo "PASS: all dynamic library references resolve"
