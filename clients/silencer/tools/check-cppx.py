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
CPPX_SUFFIXES = {".cppx", ".hx"}


def cppx_sources() -> list[pathlib.Path]:
    ignored_parts = {"build", "build-release", "build-unity", "cmake-build-debug"}
    files: list[pathlib.Path] = []
    for path in ROOT.rglob("*"):
        if path.suffix not in CPPX_SUFFIXES:
            continue
        if ignored_parts.intersection(path.relative_to(ROOT).parts):
            continue
        files.append(path)
    return sorted(files)


def cmake_cppx_sources() -> set[str]:
    cmake = ROOT / "CMakeLists.txt"
    if not cmake.exists():
        return set()
    in_block = False
    sources: set[str] = set()
    for raw_line in cmake.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not in_block:
            if line.startswith("cppx_transpile("):
                in_block = True
                line = line[len("cppx_transpile("):]
            else:
                continue
        if ")" in line:
            line = line.split(")", 1)[0]
            done = True
        else:
            done = False
        for token in line.split():
            token = token.strip()
            if pathlib.Path(token).suffix in CPPX_SUFFIXES:
                sources.add(token)
        if done:
            break
    return sources


def check_cmake_source_list(rel_sources: list[str]) -> int:
    actual = set(rel_sources)
    listed = cmake_cppx_sources()
    missing = sorted(actual - listed)
    stale = sorted(listed - actual)
    for source in missing:
        print(f"CMakeLists.txt missing cppx_transpile source: {source}")
    for source in stale:
        print(f"CMakeLists.txt lists missing cppx source: {source}")
    return 1 if missing or stale else 0


def run(argv: list[str]) -> int:
	result = subprocess.run(argv, cwd=ROOT)
	return result.returncode


def main() -> int:
    sources = cppx_sources()
    if not sources:
        print("check-cppx: no .cppx/.hx sources")
        return 0

    rel_sources = [str(path.relative_to(ROOT)) for path in sources]
    cmake_status = check_cmake_source_list(rel_sources)
    if cmake_status != 0:
        return cmake_status

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
