#!/usr/bin/env python3
"""SDL-free guard for the vendored cppx retained runtime.

The golden runtime (`/Users/hv/repos/ui` `src/ui/`) is an SDL-free generic
toolkit: the ONLY seams across the SDL boundary are the geometry mesh contract
and the single `MeasureTextFn` function pointer, neither of which names an SDL
or SDL_ttf type. So no vendored runtime file may `#include` SDL/SDL_ttf or
reference an `SDL_`/`TTF_` symbol.

This guard scans an explicit ALLOWLIST of the vendored runtime files rather than
all of `src/ui/`, because the legacy Clay runtime (also under `src/ui/`) still
uses SDL during the migration. Extend RUNTIME_FILES as later slices vendor more
golden runtime (SIL-10 flex_layout, SIL-11 draw_command, SIL-12 focus). Once the
Clay runtime is deleted (SIL-22), SIL-23 folds this into a whole-`src/ui/` scan.

Comments and string/char literals are stripped before scanning, so prose that
mentions "SDL_ttf" stays legal (mirrors the golden runtime_dependency_guard).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # clients/silencer/

# The vendored cppx retained runtime. Grows per migration slice.
RUNTIME_FILES = [
    "src/ui/input.h",
    "src/ui/style/visual_style.h",
    "src/ui/style/text_measure.h",
    "src/ui/style/text_measure.cpp",
    "src/ui/runtime/react.h",
    "src/ui/runtime/react.cpp",
    "src/ui/runtime/element.h",
    "src/ui/runtime/element.cpp",
    "src/ui/runtime/tree.h",
    "src/ui/runtime/tree.cpp",
    # SIL-10 Yoga flex layout adapter. yoga_flex_layout.cpp includes
    # <yoga/Yoga.h> (not SDL) — Yoga is an allowed layout backend; the guard
    # only bans SDL/SDL_ttf.
    "src/ui/runtime/flex_layout.h",
    "src/ui/runtime/flex_layout.cpp",
    "src/ui/runtime/yoga_flex_layout.h",
    "src/ui/runtime/yoga_flex_layout.cpp",
    # SIL-11 SDL-free draw model (RGBA IR + tessellation). The renderer-side
    # SDL executor/registries are a separate bridge layer (not listed here).
    "src/ui/runtime/draw_command.h",
    "src/ui/runtime/draw_command.cpp",
    "src/ui/runtime/draw_command_builder.h",
    "src/ui/runtime/draw_command_builder.cpp",
    "src/ui/runtime/geometry.h",
    "src/ui/runtime/geometry.cpp",
    # SIL-12 focus runtime + fiber-keyed interaction hooks. The tree IS the
    # focusable registry (NodeInteraction.focusable); focus_update is a pure
    # function over UiTree + a POD InputFrame, and use_focused/hovered/pressed
    # read a snapshot via ReactContext. All SDL-free.
    "src/ui/runtime/focus.h",
    "src/ui/runtime/focus.cpp",
    "src/ui/runtime/interaction_hooks.h",
    "src/ui/runtime/interaction_hooks.cpp",
]

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
    for rel in RUNTIME_FILES:
        path = ROOT / rel
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
    print(f"cppx runtime SDL-free guard OK ({len(RUNTIME_FILES)} files clean)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
