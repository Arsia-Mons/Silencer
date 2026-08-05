#!/usr/bin/env bash
# Make a built Silencer Launcher.app self-contained on macOS.
#
# CMake links SDL3 and SDL3_ttf by absolute path (/opt/homebrew/... locally,
# a per-runner install dir in CI), so a freshly built bundle only launches on
# a machine that has those exact files. This copies every non-system dylib
# into Contents/Frameworks and rewrites the recorded paths to @rpath, so the
# loader looks next to the executable instead.
#
# Run it after a build; it is not part of the build, because dev runs do not
# need it and dylibbundler is slow.
#
#   clients/launcher/package-macos.sh ["path/to/Silencer Launcher.app"]
#
# The bundle name has a SPACE in it. Every path here is quoted for that
# reason; `find -print0 | xargs -0` in the CI signing step is there for the
# same reason. Unquote something and it fails on a path component, not on
# the whole path, which reads as a mysterious "no such file".
#
# Mirrors .github/actions/build-silencer-macos/action.yml, which does the same
# for the game. Keep the two in step.
set -euo pipefail

APP="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build/Silencer Launcher.app}"
BIN="$APP/Contents/MacOS/Silencer Launcher"

[ -x "$BIN" ] || { echo "no launcher binary at $BIN (build first)" >&2; exit 1; }

# CI exports SDL3_ROOT / SDL3_ttf_ROOT for the per-runner installs; fall back
# to Homebrew. dylibbundler needs these as -s search paths to resolve
# @rpath/libSDL3*.dylib references — without them it drops into an interactive
# prompt loop and hangs, hence the </dev/null below as a second guard.
SDL3_LIB="${SDL3_ROOT:-/opt/homebrew/opt/sdl3}/lib"
SDL3_TTF_LIB="${SDL3_ttf_ROOT:-/opt/homebrew/opt/sdl3_ttf}/lib"

mkdir -p "$APP/Contents/Frameworks"
dylibbundler \
  -s "$SDL3_LIB" \
  -s "$SDL3_TTF_LIB" \
  --overwrite-dir \
  --bundle-deps \
  --create-dir \
  --fix-file "$BIN" \
  --dest-dir "$APP/Contents/Frameworks/" \
  --install-path @rpath/ \
  </dev/null

install_name_tool -add_rpath @executable_path/../Frameworks "$BIN" 2>/dev/null || true

# Newer dyld (Xcode 26.5+) HARD-ABORTS on a duplicate LC_RPATH where older dyld
# only warned. dylibbundler's `--install-path @rpath/` leaves a redundant,
# self-referential `@rpath/` entry that can duplicate an existing one and abort
# the app at launch (Abort trap 6). Strip every literal `@rpath/`; the bundled
# dylibs resolve through the real @executable_path rpath added above.
for _ in $(seq 1 16); do
  otool -l "$BIN" | grep -q 'path @rpath/ ' || break
  install_name_tool -delete_rpath '@rpath/' "$BIN" 2>/dev/null || break
done

# Guard: the functional rpath must survive the dedupe.
otool -l "$BIN" | grep -q 'path @executable_path/../Frameworks ' \
  || install_name_tool -add_rpath @executable_path/../Frameworks "$BIN"

# Re-sign LAST. Every install_name_tool edit above invalidates the ad-hoc
# signature dylibbundler applied, and Apple Silicon SIGKILLs a binary whose
# signature does not match its contents — the app dies with exit 137 and no
# message whatsoever. Sign the dylibs, then the bundle (which seals the main
# executable and the resources). A release build re-signs with a Developer ID
# afterwards; --force lets that override this.
codesign --force --sign - "$APP/Contents/Frameworks/"*.dylib
codesign --force --sign - "$APP"
codesign --verify --strict --verbose=2 "$APP"

echo "--- bundled dylibs ---"
ls "$APP/Contents/Frameworks/"
echo "--- LC_RPATH ---"
otool -l "$BIN" | grep -A2 LC_RPATH || true
echo "--- linkage ---"
otool -L "$BIN"

# Fail loudly rather than ship a bundle that dies on someone else's Mac.
if otool -L "$BIN" | grep -q '/opt/homebrew\|/usr/local/opt'; then
  echo "ERROR: absolute non-system paths remain in the binary" >&2
  exit 1
fi
echo "OK: $APP is self-contained"
