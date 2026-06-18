#!/usr/bin/env python3
"""SDL-free guard for the vendored cppx retained runtime.

The golden runtime (`/Users/hv/repos/ui` `src/ui/`) is an SDL-free generic
toolkit: the ONLY seams across the SDL boundary are the geometry mesh contract
and the single `MeasureTextFn` function pointer, neither of which names an SDL
or SDL_ttf type. So no vendored runtime file may `#include` SDL/SDL_ttf or
reference an `SDL_`/`TTF_` symbol.

Post-Clay (SIL-23): the legacy Clay runtime that used to share `src/ui/` is
deleted, so this folds into a WHOLE-`src/ui/` scan — every authored runtime/style
source (`.h/.cpp/.cppx/.hx`) under `src/ui/` must be SDL-free. It also covers the
explicitly-listed vendored cppx app-shell files under `src/client/ui/` (the
5-phase pipeline + navigation), whose only render seam is an injected
`std::function` the host supplies (SDL lives in that host lambda, not here).

Comments and string/char literals are stripped before scanning, so prose that
mentions "SDL_ttf" stays legal (mirrors the golden runtime_dependency_guard).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # clients/silencer/

_SCAN_EXTS = {".h", ".cpp", ".cppx", ".hx"}

# The vendored cppx app-shell (namespace client::ui): the 5-phase UiPipeline +
# ClientUi + ScreenStack/UiScreen + the NavigationProvider it compile-depends on.
# All SDL-free — the render seam is an injected std::function the host supplies.
CLIENT_APP_SHELL = [
    "src/client/ui/app_shell/navigation/ui_screen.h",
    "src/client/ui/app_shell/navigation/screen_stack.h",
    "src/client/ui/app_shell/navigation/screen_stack.cpp",
    "src/client/ui/app_shell/client_ui.h",
    "src/client/ui/app_shell/client_ui.cpp",
    "src/client/ui/app_shell/deferred_ui_mutation.h",
    "src/client/ui/app_shell/ui_pipeline.h",
    "src/client/ui/app_shell/ui_pipeline.cpp",
    "src/client/ui/providers/navigation_provider.h",
    "src/client/ui/providers/navigation_provider.cpp",
    "src/client/ui/hooks/use_navigation.h",
]


def runtime_files() -> list[Path]:
    """The whole SDL-free substrate: every src/ui source + the app-shell list."""
    files: list[Path] = []
    ui_dir = ROOT / "src" / "ui"
    for path in sorted(ui_dir.rglob("*")):
        if path.suffix in _SCAN_EXTS:
            files.append(path)
    files.extend(ROOT / rel for rel in CLIENT_APP_SHELL)
    return files

SDL_INCLUDE_RE = re.compile(r'#\s*include\s*[<"]\s*(SDL3?/|SDL[._]|SDL_ttf)', re.IGNORECASE)
SDL_SYMBOL_RE = re.compile(r'\b(?:SDL|TTF)_[A-Za-z][A-Za-z0-9_]*')


def strip_comments(text: str) -> str:
    """Remove // and /* */ comments and string/char literals so identifier
    scans only see real code. Good enough for a structural guard."""
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            j = text.find("\n", i)
            i = n if j == -1 else j
        elif two == "/*":
            j = text.find("*/", i + 2)
            i = n if j == -1 else j + 2
        elif c in ('"', "'"):
            quote = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def main() -> int:
    violations: list[str] = []
    files = runtime_files()
    for path in files:
        try:
            rel = path.relative_to(ROOT)
        except ValueError:
            rel = path
        if not path.exists():
            violations.append(f"{rel}: MISSING (expected vendored runtime file)")
            continue
        code = strip_comments(path.read_text())
        for m in SDL_INCLUDE_RE.finditer(code):
            violations.append(f"{rel}: SDL/SDL_ttf include -> {m.group(0).strip()}")
        for m in SDL_SYMBOL_RE.finditer(code):
            violations.append(f"{rel}: SDL/TTF symbol -> {m.group(0)}")

    if violations:
        print("cppx runtime SDL-free guard FAILED:", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1
    print(f"cppx runtime SDL-free guard OK ({len(files)} files clean)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
