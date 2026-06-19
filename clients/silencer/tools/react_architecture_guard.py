#!/usr/bin/env python3
"""React/shadcn architecture guard for the cppx app-shell (src/client/ui).

Deterministic smells only — judgement calls (composition quality, state
ownership, idiom match vs the golden engine) belong to the critic panel in
tools/cap/visual_parity_gate.js. Small per-component variant->token switches
are the documented convention (tokens.h) and are NOT flagged; this guard
catches the structural rot a reviewer shouldn't have to argue about:

  paint-literal   raw ::ui::Color{...} outside the token/theme files
  big-switch      switch with > MAX_SWITCH_CASES cases (variant maps scale, switches don't)
  fat-props       Props struct with > MAX_PROPS_FIELDS fields (prop-drilling smell)
  fat-signature   function with > MAX_PARAMS parameters
  god-view        .cppx function body > MAX_VIEW_LINES lines (screens compose, not orchestrate)
  conditional-hook use_*() behind a same-line if/for/while/ternary (rules of hooks)
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

MAX_SWITCH_CASES = 6
MAX_PROPS_FIELDS = 10
MAX_PARAMS = 8
MAX_VIEW_LINES = 200

PAINT_ALLOWLIST = {"tokens.h", "app_theme.cpp"}


def fail(msg: str) -> bool:
    print(msg, file=sys.stderr)
    return False


def ui_sources(root: pathlib.Path) -> list[pathlib.Path]:
    base = root / "src" / "client" / "ui"
    return [p for p in sorted(base.rglob("*")) if p.suffix in {".h", ".cpp", ".cppx", ".hx"}]


def line_of(text: str, pos: int) -> int:
    return text[:pos].count("\n") + 1


def check_paint_literals(files: list[pathlib.Path]) -> bool:
    ok = True
    pat = re.compile(r"(?:::ui::)?\bColor\s*\{\s*\d")
    for path in files:
        if path.name in PAINT_ALLOWLIST:
            continue
        text = path.read_text()
        for m in pat.finditer(text):
            ok = fail(f"{path}:{line_of(text, m.start())}: paint-literal: raw Color{{...}} — add a token to tokens.h") and ok
    return ok


def check_big_switches(files: list[pathlib.Path]) -> bool:
    ok = True
    for path in files:
        text = path.read_text()
        for m in re.finditer(r"\bswitch\s*\(", text):
            # count cases inside the switch's brace block
            i = text.find("{", m.end())
            if i < 0:
                continue
            depth, j = 1, i + 1
            while j < len(text) and depth:
                depth += {"{": 1, "}": -1}.get(text[j], 0)
                j += 1
            cases = len(re.findall(r"\bcase\b", text[i:j]))
            if cases > MAX_SWITCH_CASES:
                ok = fail(f"{path}:{line_of(text, m.start())}: big-switch: {cases} cases — use a variant/data map") and ok
    return ok


def check_fat_props(files: list[pathlib.Path]) -> bool:
    ok = True
    for path in files:
        text = path.read_text()
        for m in re.finditer(r"\bstruct\s+(\w*Props)\s*\{", text):
            i = m.end() - 1
            depth, j = 1, i + 1
            while j < len(text) and depth:
                depth += {"{": 1, "}": -1}.get(text[j], 0)
                j += 1
            body = text[i:j]
            fields = len(re.findall(r"^\s+[\w:<>,*&\s]+\s[\w_]+\s*(?:=[^;]+)?;", body, re.M))
            if fields > MAX_PROPS_FIELDS:
                ok = fail(f"{path}:{line_of(text, m.start())}: fat-props: {m.group(1)} has {fields} fields — split the component or move shared state to a provider/hook") and ok
    return ok


def check_fat_signatures(files: list[pathlib.Path]) -> bool:
    ok = True
    for path in files:
        text = path.read_text()
        for m in re.finditer(r"^[\w:<>&*\s]+\s(\w+)\s*\(([^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*\{", text, re.M):
            params = m.group(2).strip()
            if not params:
                continue
            depth = 0
            count = 1
            for ch in params:
                depth += {"<": 1, ">": -1, "(": 1, ")": -1}.get(ch, 0)
                if ch == "," and depth == 0:
                    count += 1
            if count > MAX_PARAMS:
                ok = fail(f"{path}:{line_of(text, m.start())}: fat-signature: {m.group(1)} takes {count} params — bundle into props or use a hook/provider") and ok
    return ok


def check_god_views(files: list[pathlib.Path]) -> bool:
    ok = True
    for path in files:
        if path.suffix != ".cppx":
            continue
        text = path.read_text()
        for m in re.finditer(r"::ui::UiElement\s+(\w+)\s*\([^)]*\)\s*\{", text):
            i = text.find("{", m.start())
            depth, j = 1, i + 1
            while j < len(text) and depth:
                depth += {"{": 1, "}": -1}.get(text[j], 0)
                j += 1
            lines = text[i:j].count("\n")
            if lines > MAX_VIEW_LINES:
                ok = fail(f"{path}:{line_of(text, m.start())}: god-view: {m.group(1)} is {lines} lines — extract sections into components") and ok
    return ok


def check_conditional_hooks(files: list[pathlib.Path]) -> bool:
    # Same-line if/for/while guarding a hook only. Ternary asymmetry (a hook in
    # one branch) is not reliably greppable in C++ (`::` defeats branch
    # splitting) — that judgement belongs to the critic panel.
    ok = True
    ctrl = re.compile(r"\b(?:if|for|while)\s*\(.*\buse_\w+\s*\(")
    for path in files:
        for n, line in enumerate(path.read_text().splitlines(), 1):
            if ctrl.search(line):
                ok = fail(f"{path}:{n}: conditional-hook: hooks must run unconditionally every build") and ok
    return ok


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)
    files = ui_sources(args.root.resolve())
    checks = [
        check_paint_literals(files),
        check_big_switches(files),
        check_fat_props(files),
        check_fat_signatures(files),
        check_god_views(files),
        check_conditional_hooks(files),
    ]
    return 0 if all(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
