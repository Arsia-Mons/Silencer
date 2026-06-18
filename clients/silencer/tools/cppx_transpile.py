#!/usr/bin/env python3
"""Transpile JSX-like C++ source files to ordinary C++.

The grammar is intentionally small and native-C++ shaped:

  <Panel key="pause" style={Style{.width = Length::points(320)}}>
    <Panel.Header>
      <Text value="Paused" />
    </Panel.Header>
  </Panel>

lowers to returned element construction:

  ::ui::component("Panel", PanelProps{ .key = "pause", .style = Style{.width = Length::points(320)}, .children = children({
    ::ui::component("Panel.Header", Panel::HeaderProps{ .children = children({
      ::ui::component("Text", TextProps{ .value = "Paused" }, Text),
    }) }, Panel::Header),
  }) }, Panel);

Children are emitted as owned frame data. Text children lower to `text`.
"""

from __future__ import annotations

import argparse
import difflib
import pathlib
import sys
from dataclasses import dataclass


class TranspileError(Exception):
    def __init__(self, path: pathlib.Path, line: int, column: int, message: str):
        self.path = path
        self.line = line
        self.column = column
        self.message = message
        super().__init__(f"{path}:{line}:{column}: {message}")


@dataclass
class Attr:
    name: str
    value: str


@dataclass
class StackEntry:
    tag: str
    line: int
    column: int
    close_suffix: str


def cpp_string(value: str) -> str:
    out = ['"']
    for ch in value:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ord(ch) < 32:
            out.append(f"\\x{ord(ch):02x}")
        else:
            out.append(ch)
    out.append('"')
    return "".join(out)


def component_name(tag: str) -> str:
    return "::".join(part for part in tag.split(".") if part)


def component_props_type(tag: str) -> str:
    return f"{component_name(tag)}Props"


def component_display_name(tag: str) -> str:
    return tag


def is_host_tag(tag: str) -> bool:
    return tag == "detail.Host"


def prop_name(name: str) -> str:
    source = name.replace("-", "_")
    out: list[str] = []
    for index, ch in enumerate(source):
        if ch.isupper():
            if index > 0 and source[index - 1] != "_" and (
                source[index - 1].islower() or source[index - 1].isdigit()
            ):
                out.append("_")
            out.append(ch.lower())
        else:
            out.append(ch)
    return "".join(out)


def props_fields(attrs: list[Attr]) -> list[str]:
    return [f".{prop_name(attr.name)} = {attr.value}" for attr in attrs]


def aggregate_init(type_name: str, attrs: list[Attr]) -> str:
    parts = props_fields(attrs)
    if not parts:
        return f"{type_name}{{}}"
    return f"{type_name}{{ " + ", ".join(parts) + " }"


def aggregate_open_with_children(type_name: str, attrs: list[Attr]) -> str:
    parts = props_fields(attrs)
    parts.append(".children = children({")
    return f"{type_name}{{ " + ", ".join(parts)


def open_element_expression(tag: str, attrs: list[Attr]) -> tuple[str, str]:
    name = component_name(tag)
    if is_host_tag(tag):
        return (
            f"{name}({aggregate_init(name + 'Props', attrs)})",
            f"{name}({aggregate_open_with_children(name + 'Props', attrs)}",
        )
    props_type = component_props_type(tag)
    display_name = component_display_name(tag)
    return (
        f"::ui::component({cpp_string(display_name)}, {aggregate_init(props_type, attrs)}, {name})",
        f"::ui::component({cpp_string(display_name)}, {aggregate_open_with_children(props_type, attrs)}",
    )


def close_element_suffix(tag: str) -> str:
    name = component_name(tag)
    if is_host_tag(tag):
        return "}) })"
    return f"}}) }}, {name})"


def find_tag_end(text: str, start: int, path: pathlib.Path, line: int) -> int:
    quote: str | None = None
    brace_depth = 0
    i = start
    while i < len(text):
        ch = text[i]
        if quote:
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue
        if ch in ('"', "'"):
            quote = ch
        elif ch == "{":
            brace_depth += 1
        elif ch == "}":
            if brace_depth <= 0:
                raise TranspileError(path, line, i + 1, "unmatched '}' in tag")
            brace_depth -= 1
        elif ch == ">" and brace_depth == 0:
            return i
        i += 1
    raise TranspileError(path, line, start + 1, "unterminated JSX tag")


def take_ident(text: str, index: int) -> tuple[str, int]:
    start = index
    while index < len(text):
        ch = text[index]
        if ch.isalnum() or ch in "_:.-":
            index += 1
            continue
        break
    return text[start:index], index


def parse_braced_value(
    text: str, index: int, path: pathlib.Path, line: int
) -> tuple[str, int]:
    start = index + 1
    depth = 1
    i = start
    quote: str | None = None
    while i < len(text):
        ch = text[i]
        if quote:
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue
        if ch in ('"', "'"):
            quote = ch
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start:i].strip(), i + 1
        i += 1
    raise TranspileError(path, line, index + 1, "unterminated attribute expression")


def parse_quoted_value(
    text: str, index: int, path: pathlib.Path, line: int
) -> tuple[str, int]:
    quote = text[index]
    i = index + 1
    raw: list[str] = []
    while i < len(text):
        ch = text[i]
        if ch == "\\":
            if i + 1 >= len(text):
                raise TranspileError(path, line, i + 1, "unterminated string escape")
            raw.append(ch)
            raw.append(text[i + 1])
            i += 2
            continue
        if ch == quote:
            return quote + "".join(raw) + quote, i + 1
        raw.append(ch)
        i += 1
    raise TranspileError(path, line, index + 1, "unterminated attribute string")


def parse_attrs(body: str, index: int, path: pathlib.Path, line: int) -> list[Attr]:
    attrs: list[Attr] = []
    while index < len(body):
        while index < len(body) and body[index].isspace():
            index += 1
        if index >= len(body):
            break
        name, index = take_ident(body, index)
        if not name:
            raise TranspileError(path, line, index + 1, "expected attribute name")
        while index < len(body) and body[index].isspace():
            index += 1
        if index >= len(body) or body[index] != "=":
            attrs.append(Attr(name=name, value="true"))
            continue
        index += 1
        while index < len(body) and body[index].isspace():
            index += 1
        if index >= len(body):
            raise TranspileError(path, line, index + 1, f"missing value for {name}")
        if body[index] == "{":
            value, index = parse_braced_value(body, index, path, line)
        elif body[index] in ('"', "'"):
            value, index = parse_quoted_value(body, index, path, line)
        else:
            start = index
            while index < len(body) and not body[index].isspace():
                index += 1
            value = body[start:index]
        attrs.append(Attr(name=name, value=value))
    return attrs


def parse_open_tag(
    raw_body: str, path: pathlib.Path, line: int, column: int
) -> tuple[str, list[Attr], bool]:
    body = raw_body.strip()
    self_closing = body.endswith("/")
    if self_closing:
        body = body[:-1].rstrip()
    tag, index = take_ident(body, 0)
    if not tag:
        raise TranspileError(path, line, column, "expected component name")
    attrs = parse_attrs(body, index, path, line)
    return tag, attrs, self_closing


def line_directive(path: pathlib.Path, line: int) -> str:
    try:
        display = path.resolve().relative_to(pathlib.Path.cwd().resolve())
    except ValueError:
        display = path
    return f"#line {line} {cpp_string(display.as_posix())}"


def transpile_jsx_line(
    source: str,
    base_indent: str,
    path: pathlib.Path,
    line_no: int,
    stack: list[StackEntry],
    prefix: str = "",
) -> list[str]:
    out: list[str] = []
    i = 0
    pending_prefix = prefix

    def expression_end() -> str:
        return "," if stack else ";"

    def close_children_expression(entry: StackEntry) -> str:
        return f"{entry.close_suffix}{expression_end()}"

    def take_prefix() -> str:
        nonlocal pending_prefix
        value = pending_prefix
        pending_prefix = ""
        return value

    while i < len(source):
        if source[i].isspace():
            i += 1
            continue
        if source[i] != "<":
            next_tag = source.find("<", i)
            if next_tag < 0:
                next_tag = len(source)
            text = source[i:next_tag]
            if text.strip():
                stripped = text.strip()
                out.append(line_directive(path, line_no))
                if stripped.startswith("{") and stripped.endswith("}"):
                    out.append(
                        f"{base_indent}{take_prefix()}{stripped[1:-1].strip()}{expression_end()}"
                    )
                else:
                    out.append(
                        f"{base_indent}{take_prefix()}text({cpp_string(stripped)}){expression_end()}"
                    )
            i = next_tag
            continue

        end = find_tag_end(source, i, path, line_no)
        body = source[i + 1 : end]
        column = i + 1
        if body.startswith("/"):
            tag = body[1:].strip()
            if not tag:
                raise TranspileError(path, line_no, column, "expected closing tag name")
            if not stack:
                raise TranspileError(path, line_no, column, f"unexpected closing tag {tag}")
            entry = stack.pop()
            if entry.tag != tag:
                raise TranspileError(
                    path,
                    line_no,
                    column,
                    f"mismatched closing tag {tag}; expected {entry.tag}",
                )
            out.append(line_directive(path, line_no))
            out.append(f"{base_indent}{close_children_expression(entry)}")
        else:
            tag, attrs, self_closing = parse_open_tag(body, path, line_no, column)
            out.append(line_directive(path, line_no))
            closed, open_with_children = open_element_expression(tag, attrs)
            if self_closing:
                out.append(f"{base_indent}{take_prefix()}{closed}{expression_end()}")
            else:
                stack.append(
                    StackEntry(
                        tag=tag,
                        line=line_no,
                        column=column,
                        close_suffix=close_element_suffix(tag),
                    )
                )
                out.append(f"{base_indent}{take_prefix()}{open_with_children}")
        i = end + 1
    return out


def is_jsx_line(line: str) -> bool:
    stripped = line.lstrip()
    jsx_start = stripped.startswith("<") and not stripped.startswith("<<")
    return jsx_start or stripped.startswith("return <")


def jsx_tag_is_complete(source: str, path: pathlib.Path, line_no: int) -> bool:
    tag_start = source.find("<")
    if tag_start < 0:
        return True
    try:
        find_tag_end(source, tag_start, path, line_no)
        return True
    except TranspileError as exc:
        if exc.message == "unterminated JSX tag":
            return False
        raise


def collect_jsx_source(
    lines: list[str], start_index: int, path: pathlib.Path
) -> tuple[str, int]:
    line_no = start_index + 1
    source = lines[start_index].strip()
    next_index = start_index + 1
    while not jsx_tag_is_complete(source, path, line_no):
        if next_index >= len(lines):
            column = lines[start_index].find("<") + 1
            raise TranspileError(path, line_no, column, "unterminated JSX tag")
        source += " " + lines[next_index].strip()
        next_index += 1
    return source, next_index


def transpile_text(text: str, path: pathlib.Path) -> str:
    stack: list[StackEntry] = []
    out: list[str] = []
    lines = text.splitlines()
    index = 0
    while index < len(lines):
        line = lines[index]
        line_no = index + 1
        if not is_jsx_line(line):
            if stack and line.strip():
                indent = line[: len(line) - len(line.lstrip())]
                generated = transpile_jsx_line(
                    line.strip(), indent, path, line_no, stack
                )
                out.extend(generated)
                index += 1
                continue
            out.append(line)
            index += 1
            continue
        indent = line[: len(line) - len(line.lstrip())]
        stripped, index = collect_jsx_source(lines, index, path)
        prefix = ""
        if stripped.startswith("return <"):
            prefix = "return "
            stripped = stripped[len("return ") :]
        generated = transpile_jsx_line(stripped, indent, path, line_no, stack, prefix)
        out.extend(generated)
    if stack:
        entry = stack[-1]
        raise TranspileError(
            path,
            entry.line,
            entry.column,
            f"unclosed JSX tag {entry.tag}",
        )
    return "\n".join(out) + "\n"


def output_path_for(source: pathlib.Path, out_dir: pathlib.Path) -> pathlib.Path:
    ext = source.suffix
    if ext == ".cppx":
        target_ext = ".cpp"
    elif ext == ".hx":
        target_ext = ".h"
    else:
        raise ValueError(f"unsupported extension: {ext}")
    return out_dir / source.with_suffix(target_ext).name


def write_if_changed(path: pathlib.Path, data: str) -> None:
    old = path.read_text() if path.exists() else None
    if old == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(data)


def run_verify(source: pathlib.Path, expected: pathlib.Path) -> int:
    actual = transpile_text(source.read_text(), source)
    expected_text = expected.read_text()
    if actual == expected_text:
        return 0
    diff = difflib.unified_diff(
        expected_text.splitlines(keepends=True),
        actual.splitlines(keepends=True),
        fromfile=str(expected),
        tofile=f"{source} (actual)",
    )
    sys.stderr.writelines(diff)
    return 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("-o", "--output", type=pathlib.Path)
    parser.add_argument("--out-dir", type=pathlib.Path)
    parser.add_argument("--verify", type=pathlib.Path)
    args = parser.parse_args(argv)

    try:
        source_text = args.source.read_text()
        generated = transpile_text(source_text, args.source)
        if args.verify:
            expected_text = args.verify.read_text()
            if generated == expected_text:
                return 0
            diff = difflib.unified_diff(
                expected_text.splitlines(keepends=True),
                generated.splitlines(keepends=True),
                fromfile=str(args.verify),
                tofile=f"{args.source} (actual)",
            )
            sys.stderr.writelines(diff)
            return 1

        if args.output:
            output = args.output
        elif args.out_dir:
            output = output_path_for(args.source, args.out_dir)
        else:
            sys.stdout.write(generated)
            return 0
        write_if_changed(output, generated)
        return 0
    except TranspileError as exc:
        print(exc, file=sys.stderr)
        return 2
    except OSError as exc:
        print(exc, file=sys.stderr)
        return 2
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
