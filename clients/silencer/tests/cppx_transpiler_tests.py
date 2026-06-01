#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import tempfile


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=False, text=True, capture_output=True)


def compare_fixture(tool: pathlib.Path, source: pathlib.Path, expected: pathlib.Path) -> bool:
    with tempfile.TemporaryDirectory() as tmp:
        output = pathlib.Path(tmp) / expected.name
        result = run([sys.executable, str(tool), str(source), "-o", str(output)])
        if result.returncode != 0:
            sys.stderr.write(result.stderr)
            return False
        actual_text = output.read_text()
    expected_text = expected.read_text()
    if actual_text == expected_text:
        return True
    sys.stderr.write(f"{source}: generated output differed from {expected}\n")
    sys.stderr.write("--- expected\n")
    sys.stderr.write(expected_text)
    sys.stderr.write("--- actual\n")
    sys.stderr.write(actual_text)
    return False


def verify_fixture(tool: pathlib.Path, source: pathlib.Path, expected: pathlib.Path) -> bool:
    result = run([sys.executable, str(tool), str(source), "--verify", str(expected)])
    if result.returncode == 0:
        return True
    sys.stderr.write(result.stderr)
    return False


def error_fixture(tool: pathlib.Path, source: pathlib.Path, expected: str) -> bool:
    result = run([sys.executable, str(tool), str(source)])
    if result.returncode == 0:
        sys.stderr.write(f"{source}: expected failure, got success\n")
        return False
    if expected in result.stderr:
        return True
    sys.stderr.write(f"{source}: missing expected diagnostic\n")
    sys.stderr.write(f"expected substring: {expected}\n")
    sys.stderr.write(result.stderr)
    return False


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    parser.add_argument("--fixtures", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)

    fixtures = args.fixtures
    checks = [
        compare_fixture(args.tool, fixtures / "panel.cppx", fixtures / "panel.expected.cpp"),
        compare_fixture(
            args.tool,
            fixtures / "multiline.cppx",
            fixtures / "multiline.expected.cpp",
        ),
        compare_fixture(args.tool, fixtures / "components.hx", fixtures / "components.expected.h"),
        verify_fixture(args.tool, fixtures / "panel.cppx", fixtures / "panel.expected.cpp"),
        verify_fixture(
            args.tool,
            fixtures / "multiline.cppx",
            fixtures / "multiline.expected.cpp",
        ),
        error_fixture(
            args.tool,
            fixtures / "bad_mismatch.cppx",
            "bad_mismatch.cppx:4:1: mismatched closing tag Panel.Body; expected Panel.Header",
        ),
        error_fixture(
            args.tool,
            fixtures / "bad_unclosed.cppx",
            "bad_unclosed.cppx:2:1: unclosed JSX tag Panel",
        ),
    ]
    return 0 if all(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
