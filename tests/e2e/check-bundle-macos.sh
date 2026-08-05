#!/usr/bin/env bash
# Verifies a built Silencer.app bundle is self-contained: every non-system
# dylib reference (in the main binary and in each bundled dylib) must resolve
# to a file inside Contents/Frameworks/.
#
# Usage: tests/e2e/check-bundle-macos.sh <app> [exe-name] [helper-relpath] [strict]
#
# Defaults describe the game bundle, so the one-argument call is unchanged.
# The launcher's stub-first outer bundle passes "strict" (its binary must
# depend on NOTHING non-system — that is the bootstrap stub's safety
# property) and an empty helper path; the nested payload app is checked with
# a second, normal call:
#
#   check-bundle-macos.sh "build-launcher/Silencer Launcher.app" "Silencer Launcher" "" strict
#   check-bundle-macos.sh ".../Contents/Resources/payload/Silencer Launcher.app" "Silencer Launcher" ""
set -euo pipefail

APP="${1:?usage: $0 <app> [exe-name] [helper-relpath] [strict]}"
EXE_NAME="${2:-Silencer}"
HELPER_REL="${3-Contents/Helpers/updater-stage-2}"
STRICT="${4:-}"
BINARY="$APP/Contents/MacOS/$EXE_NAME"
FRAMEWORKS="$APP/Contents/Frameworks"

[ -x "$BINARY" ] || { echo "no executable at $BINARY" >&2; exit 1; }
if [ -n "$HELPER_REL" ]; then
	HELPER="$APP/$HELPER_REL"
	[ -x "$HELPER" ] || { echo "no executable at $HELPER" >&2; exit 1; }
fi

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

if [ "$STRICT" = "strict" ]; then
	check_file "$BINARY" 0
else
	check_file "$BINARY"
fi
if [ -n "$HELPER_REL" ]; then
	# An `[ ... ] && check_file` one-liner would exit the script under set -e
	# on the skip path, because the whole statement then returns 1.
	check_file "$HELPER" 0
fi

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
