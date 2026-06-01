#!/usr/bin/env python3
"""Component-descriptor architecture guard (ported from the golden ui repo,
tests/component_descriptor_guard.py, for SIL-23).

Locks the cppx component contract: the public API is descriptor factories
(`::ui::component("Name", ...)` / family wrappers like AppButton), NOT a
`Component<T>` template wrapper, direct generic-primitive bodies, or manual
retained-provider macros. The transpiler must lower authored .cppx/.hx into
`::ui::component(...)` descriptors, never direct component calls.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys


GENERIC_COMPONENTS = ("Box", "Button", "Checkbox", "Dialog", "Input", "Text")


def read(path: pathlib.Path) -> str:
    return path.read_text()


def fail(message: str) -> bool:
    print(message, file=sys.stderr)
    return False


def source_files(root: pathlib.Path) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for base in (root / "src", root / "tests"):
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.suffix in {".h", ".cpp", ".cppx", ".hx"}:
                files.append(path)
    return files


def check_no_component_wrapper(root: pathlib.Path) -> bool:
    ok = True
    for path in source_files(root):
        text = read(path)
        if "::ui::Component<" in text or "Component<" in text:
            ok = fail(f"{path}: public Component<T> wrapper is not allowed") and ok
    return ok


def check_no_direct_generic_component_calls(root: pathlib.Path) -> bool:
    ok = True
    pattern = re.compile(
        r"(?<!elements::)(?<!::ui::components::elements::)"
        r"(?:\bcomponents|::ui::components)::"
        r"(" + "|".join(GENERIC_COMPONENTS) + r")\s*\("
    )
    for base in (root / "src" / "client",):
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.suffix not in {".h", ".cpp"}:
                continue
            for match in pattern.finditer(read(path)):
                line = read(path)[: match.start()].count("\n") + 1
                ok = (
                    fail(
                        f"{path}:{line}: call creates a component body directly; use a descriptor factory"
                    )
                    and ok
                )
    return ok


def check_no_manual_retained_providers(root: pathlib.Path) -> bool:
    ok = True
    pattern = re.compile(
        r"\b(REACT_PROVIDER_ENTER|REACT_PROVIDER_EXIT|PROVIDE|react_provider_push|react_provider_pop)\b"
    )
    for base in (root / "src" / "app", root / "src" / "client"):
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.suffix not in {".h", ".cpp"}:
                continue
            text = read(path)
            for match in pattern.finditer(text):
                line = text[: match.start()].count("\n") + 1
                ok = (
                    fail(
                        f"{path}:{line}: retained app/client providers must be UiElement descriptors"
                    )
                    and ok
                )
    return ok


def transpile(tool: pathlib.Path, source: pathlib.Path) -> str:
    result = subprocess.run(
        [sys.executable, str(tool), str(source)],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr)
    return result.stdout


def check_cppx_descriptors(root: pathlib.Path, tool: pathlib.Path) -> bool:
    ok = True
    fixtures = [
        root / "tests" / "fixtures" / "cppx" / "panel.cppx",
        root / "tests" / "fixtures" / "cppx" / "multiline.cppx",
    ]
    direct_call = re.compile(
        r"(?<!::ui::component\(\")\b("
        + "|".join((*GENERIC_COMPONENTS, "Panel", "StatefulProbe"))
        + r")\s*\(\s*(?:\{|[A-Za-z_])"
    )
    for source in fixtures:
        generated = transpile(tool, source)
        for match in direct_call.finditer(generated):
            line = generated[: match.start()].count("\n") + 1
            ok = (
                fail(
                    f"{source}:{line}: transpiler emitted a direct component call instead of a descriptor"
                )
                and ok
            )
        if "::ui::component(" not in generated:
            ok = fail(f"{source}: transpiler did not emit component descriptors") and ok
    return ok


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, required=True)
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)

    root = args.root.resolve()
    checks = [
        check_no_component_wrapper(root),
        check_no_direct_generic_component_calls(root),
        check_no_manual_retained_providers(root),
        check_cppx_descriptors(root, args.tool.resolve()),
    ]
    return 0 if all(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
