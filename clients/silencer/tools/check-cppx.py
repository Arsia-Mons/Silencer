#!/usr/bin/env python3
"""Check Silencer cppx sources for deterministic formatting and transpilation."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
FORMATTER = ROOT / "tools" / "cppx_format.py"
TRANSPILER = ROOT / "tools" / "cppx_transpile.py"


def cppx_sources() -> list[pathlib.Path]:
    ignored_parts = {"build", "build-release", "build-unity", "cmake-build-debug"}
    files: list[pathlib.Path] = []
    for path in ROOT.rglob("*"):
        if path.suffix not in {".cppx", ".hx"}:
            continue
        if ignored_parts.intersection(path.relative_to(ROOT).parts):
            continue
        files.append(path)
    return sorted(files)


def run(argv: list[str]) -> int:
    result = subprocess.run(argv, cwd=ROOT)
    return result.returncode


def main() -> int:
    sources = cppx_sources()
    if not sources:
        print("check-cppx: no .cppx/.hx sources")
        return 0

    rel_sources = [str(path.relative_to(ROOT)) for path in sources]
    format_status = run([sys.executable, str(FORMATTER), "--check", *rel_sources])
    if format_status != 0:
        return format_status

    with tempfile.TemporaryDirectory(prefix="silencer-cppx-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        for source in rel_sources:
            status = run(
                [
                    sys.executable,
                    str(TRANSPILER),
                    source,
                    "--out-dir",
                    str(tmp_dir),
                ]
            )
            if status != 0:
                return status

    print(f"check-cppx: OK ({len(sources)} sources)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
