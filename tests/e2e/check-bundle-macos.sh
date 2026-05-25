#!/usr/bin/env bash
# Verifies a built Silencer.app bundle is self-contained: every non-system
# dylib reference (in the main binary and in each bundled dylib) must resolve
# to a file inside Contents/Frameworks/.
#
# Usage: tests/e2e/check-bundle-macos.sh <path-to-Silencer.app>
set -euo pipefail

APP="${1:?usage: $0 <path-to-Silencer.app>}"
BINARY="$APP/Contents/MacOS/Silencer"
FRAMEWORKS="$APP/Contents/Frameworks"
HELPER="$APP/Contents/Helpers/updater-stage-2"

[ -x "$BINARY" ] || { echo "no executable at $BINARY" >&2; exit 1; }
[ -x "$HELPER" ] || { echo "no executable at $HELPER" >&2; exit 1; }

errors=0

check_file() {
	local file="$1"
	local allow_bundled_deps="${2:-1}"
	local refs
	refs=$(otool -L "$file" | tail -n +2 | awk '{print $1}')
	while IFS= read -r ref; do
		case "$ref" in
			"") ;;
			/System/*|/usr/lib/*) ;;
			@executable_path/*|@loader_path/*)
				if [ "$allow_bundled_deps" != "1" ]; then
					echo "UNEXPECTED helper dependency: $ref (in $file)"
					errors=$((errors + 1))
				fi
				;;
			@rpath/*)
				if [ "$allow_bundled_deps" != "1" ]; then
					echo "UNEXPECTED helper dependency: $ref (in $file)"
					errors=$((errors + 1))
					continue
				fi
				local lib="${ref#@rpath/}"
				if [ ! -f "$FRAMEWORKS/$lib" ]; then
					echo "MISSING in Frameworks/: $lib (referenced from $file)"
					errors=$((errors + 1))
				fi
				;;
			/*)
				echo "UNBUNDLED absolute path: $ref (in $file)"
				errors=$((errors + 1))
				;;
			*)
				echo "UNRECOGNIZED ref: $ref (in $file)"
				errors=$((errors + 1))
				;;
		esac
	done <<< "$refs"
}

check_file "$BINARY"
check_file "$HELPER" 0

if [ -d "$FRAMEWORKS" ]; then
	while IFS= read -r dylib; do
		check_file "$dylib"
	done < <(find "$FRAMEWORKS" -name "*.dylib" -type f)
fi

if [ "$errors" -gt 0 ]; then
	echo "----"
	echo "FAIL: $errors unbundled dependency reference(s) detected"
	exit 1
fi

echo "PASS: all non-system dylib references resolve into $FRAMEWORKS/"
