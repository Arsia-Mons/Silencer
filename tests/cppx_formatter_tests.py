#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run_formatter(tool: pathlib.Path, source: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(tool), "--stdin"],
        input=source,
        check=False,
        text=True,
        capture_output=True,
    )


def compare(tool: pathlib.Path, name: str, source: str, expected: str) -> bool:
    result = run_formatter(tool, source)
    if result.returncode != 0:
        sys.stderr.write(f"{name}: formatter failed\n")
        sys.stderr.write(result.stderr)
        return False
    if result.stdout == expected:
        return True
    sys.stderr.write(f"{name}: formatted output differed\n")
    sys.stderr.write("--- expected\n")
    sys.stderr.write(expected)
    sys.stderr.write("--- actual\n")
    sys.stderr.write(result.stdout)
    return False


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)

    expression_source = """UiElement Button(const ButtonProps& props) {
\treturn <detail.Host kind={::ui::HostKind::Button} callbacks={button_callbacks(props)}>
\t\t{props.children.count > 0 ? nullptr : <detail.Host kind={::ui::HostKind::Text} key="label" visual={label_visual} text={::ui::HostTextProps{.value = props.label}} />}
\t</detail.Host>
}
"""
    expression_expected = """UiElement Button(const ButtonProps& props) {
\treturn <detail.Host kind={::ui::HostKind::Button} callbacks={button_callbacks(props)}>
\t\t{props.children.count > 0 ? nullptr : <detail.Host
\t\t    kind={::ui::HostKind::Text}
\t\t    key="label"
\t\t    visual={label_visual}
\t\t    text={::ui::HostTextProps{.value = props.label}}
\t\t  />}
\t</detail.Host>
}
"""

    root_source = """UiElement Panel() {
\treturn <Panel key="main" id="PanelRoot" mode="primary" state="interactive" description="Primary interactive root panel" active disabled><Text value="Ready" /></Panel>
}
"""
    root_expected = """UiElement Panel() {
\treturn <Panel
\t  key="main"
\t  id="PanelRoot"
\t  mode="primary"
\t  state="interactive"
\t  description="Primary interactive root panel"
\t  active
\t  disabled
\t><Text value="Ready" /></Panel>
}
"""

    checks = [
        compare(args.tool, "expression-continuation", expression_source, expression_expected),
        compare(args.tool, "inline-root-tag", root_source, root_expected),
    ]
    return 0 if all(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
