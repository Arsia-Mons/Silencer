#!/usr/bin/env python3
"""Canonicalise JSX layout in .cppx / .hx source.

Scope is deliberately narrow: this formatter rewrites only the opening
(and self-closing) tags of JSX elements. Tag children and surrounding
C++ source are left byte-identical so the formatter never touches code
it doesn't understand. Closing tags `</Name>` need no rewriting.

Canonical opening-tag layout:

  * If the whole tag fits inside the configured line width on its
    original line, keep it inline with no spaces around `=`.
        <Tag attr={value} other="text">

  * Otherwise break across lines, one attribute per line, indented one
    step beyond the line that opened the tag. The closing `>` (or `/>`)
    goes back to the opener's indent.
        <Tag
          attr={value}
          other="text"
        >

The parser is a small recursive-descent walker over the source string.
It uses the same `<`-disambiguation rules as the tree-sitter grammar in
tools/editor/zed-cppx/: a `<` only begins a JSX tag when it is preceded
by non-identifier whitespace (or `template <…>`) and followed by a
letter/underscore that can start a tag name. Everything else stays
opaque.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass, field

WORD_CHARS = set(
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789_"
)
IDENT_START = set(
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "_"
)
TAG_NAME_CHARS = WORD_CHARS | {"."}
ATTR_NAME_CHARS = WORD_CHARS | {"-"}


class FormatError(Exception):
    def __init__(self, line: int, column: int, message: str):
        self.line = line
        self.column = column
        self.message = message
        super().__init__(f"{line}:{column}: {message}")


@dataclass
class OpenTag:
    """A parsed opening (or self-closing) JSX tag and its source span."""

    start: int  # index of `<`
    end: int  # one past `>` or `/>`
    tag: str
    attrs: list["Attr"] = field(default_factory=list)
    self_closing: bool = False
    indent: str = ""  # leading whitespace of the line that the tag starts on
    embedded: bool = False  # tag starts after non-whitespace on its line


@dataclass
class Attr:
    name: str
    value: str | None  # full source text of value (with quotes / braces), or None for boolean


def line_col(source: str, index: int) -> tuple[int, int]:
    line = source.count("\n", 0, index) + 1
    last_nl = source.rfind("\n", 0, index)
    col = index - (last_nl + 1) + 1
    return line, col


def is_jsx_tag_start(source: str, i: int) -> bool:
    """Decide whether `source[i] == '<'` opens a JSX tag.

    Mirrors the tree-sitter grammar's disambiguation:
      * `<` must be followed by an identifier-start (letter or `_`).
      * `<` must NOT be immediately preceded by an identifier character
        (ruling out `vector<int>`, `cast<T>` etc.).
      * `<` must NOT follow the keyword `template` (with any whitespace),
        which rules out `template <typename T>`.
    """
    if source[i] != "<":
        return False
    if i + 1 >= len(source):
        return False
    nxt = source[i + 1]
    if nxt not in IDENT_START:
        return False
    if i > 0 and source[i - 1] in WORD_CHARS:
        return False
    # Look back through whitespace for the `template` keyword.
    j = i - 1
    while j >= 0 and source[j] in " \t":
        j -= 1
    if j >= 7 and source[j - 7 : j + 1] == "template":
        before = j - 8
        if before < 0 or source[before] not in WORD_CHARS:
            return False
    return True

def line_indent(source: str, index: int) -> str:
    """Return the leading whitespace of the line containing `index`."""
    line_start = source.rfind("\n", 0, index) + 1
    j = line_start
    while j < len(source) and source[j] in " \t":
        j += 1
    return source[line_start:j]


def is_embedded_tag(source: str, index: int) -> bool:
    """True when a tag starts after non-whitespace expression text."""
    line_start = source.rfind("\n", 0, index) + 1
    prefix = source[line_start:index]
    if not prefix.strip(" \t"):
        return False
    if prefix.strip() in {"return", "co_return"}:
        return False
    return True


def parse_open_tag(source: str, start: int) -> OpenTag:
    """Parse a JSX opening or self-closing tag starting at `source[start] == '<'`."""

    assert source[start] == "<", f"expected '<' at index {start}"
    i = start + 1

    # Tag name.
    name_start = i
    while i < len(source) and source[i] in TAG_NAME_CHARS:
        i += 1
    if name_start == i:
        line, col = line_col(source, start)
        raise FormatError(line, col, "expected tag name after '<'")
    tag = source[name_start:i]

    attrs: list[Attr] = []
    self_closing = False

    while True:
        # Skip whitespace.
        while i < len(source) and source[i] in " \t\r\n":
            i += 1
        if i >= len(source):
            line, col = line_col(source, start)
            raise FormatError(line, col, f"unterminated JSX tag <{tag}>")

        ch = source[i]
        if ch == ">":
            i += 1
            break
        if ch == "/" and i + 1 < len(source) and source[i + 1] == ">":
            self_closing = True
            i += 2
            break

        # Attribute name.
        name_begin = i
        while i < len(source) and source[i] in ATTR_NAME_CHARS:
            i += 1
        if name_begin == i:
            line, col = line_col(source, i)
            raise FormatError(line, col, "expected attribute name")
        attr_name = source[name_begin:i]

        # Optional '= value'.
        save = i
        while i < len(source) and source[i] in " \t\r\n":
            i += 1
        if i >= len(source) or source[i] != "=":
            attrs.append(Attr(name=attr_name, value=None))
            i = save
            continue
        i += 1  # past '='
        while i < len(source) and source[i] in " \t\r\n":
            i += 1
        if i >= len(source):
            line, col = line_col(source, i)
            raise FormatError(line, col, f"missing value for attribute {attr_name}")

        value_start = i
        if source[i] == "{":
            depth = 1
            i += 1
            in_str: str | None = None
            while i < len(source):
                c = source[i]
                if in_str:
                    if c == "\\":
                        i += 2
                        continue
                    if c == in_str:
                        in_str = None
                    i += 1
                    continue
                if c in ('"', "'"):
                    in_str = c
                    i += 1
                    continue
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                    if depth == 0:
                        i += 1
                        break
                i += 1
            else:
                line, col = line_col(source, value_start)
                raise FormatError(line, col, "unterminated `{…}` attribute value")
        elif source[i] in ('"', "'"):
            quote = source[i]
            i += 1
            while i < len(source):
                c = source[i]
                if c == "\\":
                    i += 2
                    continue
                if c == quote:
                    i += 1
                    break
                i += 1
            else:
                line, col = line_col(source, value_start)
                raise FormatError(line, col, "unterminated string attribute value")
        else:
            # Bare token (rare; preserved verbatim).
            while i < len(source) and source[i] not in " \t\r\n/>":
                i += 1
        value_text = source[value_start:i]
        attrs.append(Attr(name=attr_name, value=value_text))

    indent = line_indent(source, start)
    return OpenTag(
        start=start,
        end=i,
        tag=tag,
        attrs=attrs,
        self_closing=self_closing,
        indent=indent,
        embedded=is_embedded_tag(source, start),
    )


def find_open_tags(source: str) -> list[OpenTag]:
    """Return every JSX opening / self-closing tag, in source order.

    Skips children of an opened element — we only want top-level tags
    plus tags nested at any depth, all flat. Each returned span refers
    to the opening tag itself (not its children or closing tag).
    """
    tags: list[OpenTag] = []
    i = 0
    n = len(source)
    while i < n:
        if is_jsx_tag_start(source, i):
            tag = parse_open_tag(source, i)
            tags.append(tag)
            i = tag.end
            continue
        # Skip a string so we don't trip on `<` inside it.
        c = source[i]
        if c in ('"', "'"):
            quote = c
            i += 1
            while i < n:
                if source[i] == "\\":
                    i += 2
                    continue
                if source[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        # Skip line comments.
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            nl = source.find("\n", i)
            i = nl if nl >= 0 else n
            continue
        # Skip block comments.
        if c == "/" and i + 1 < n and source[i + 1] == "*":
            end = source.find("*/", i + 2)
            i = (end + 2) if end >= 0 else n
            continue
        i += 1
    return tags


_BRACKET_CLEANUPS = [
    (re.compile(r"\(\s+"), "("),
    (re.compile(r"\s+\)"), ")"),
    (re.compile(r"\[\s+"), "["),
    (re.compile(r"\s+\]"), "]"),
    (re.compile(r"\s+,"), ","),
]


def _has_nested_brace(inner: str) -> bool:
    """True if `inner` contains a `{` outside of any string literal.

    Used to detect intentionally multi-line attribute values such as
    `{Style{ .width = …, .height = …, }}`. Those carry the user's
    deliberate layout and we leave them alone; bare expressions like
    `{foo(bar)}` that got wrapped by a C++ formatter are safe to collapse.
    """
    in_str: str | None = None
    i = 0
    while i < len(inner):
        c = inner[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in ('"', "'"):
            in_str = c
            i += 1
            continue
        if c == "{":
            return True
        i += 1
    return False


def normalise_attr_value(raw: str) -> str:
    """Tidy up incidental whitespace inside a `{…}` value.

    If the value contains a nested `{`, preserve it verbatim — that's a
    sign the user laid out a struct literal or lambda intentionally.
    Otherwise collapse runs of whitespace (typically introduced by an
    upstream C++ formatter wrapping a long call) and clean up the
    `func( arg )` -> `func(arg)` artefacts that fall out.
    """
    if raw.startswith("{") and raw.endswith("}"):
        inner = raw[1:-1]
        if _has_nested_brace(inner):
            return raw
        collapsed = " ".join(inner.split())
        for pattern, replacement in _BRACKET_CLEANUPS:
            collapsed = pattern.sub(replacement, collapsed)
        return "{" + collapsed + "}"
    return raw


def render_inline(tag: OpenTag) -> str:
    pieces: list[str] = [tag.tag]
    for attr in tag.attrs:
        if attr.value is None:
            pieces.append(attr.name)
        else:
            pieces.append(f"{attr.name}={normalise_attr_value(attr.value)}")
    inner = " ".join(pieces)
    if tag.self_closing:
        return f"<{inner} />"
    return f"<{inner}>"


def render_multiline(tag: OpenTag, indent_step: str) -> str:
    base = tag.indent + indent_step if tag.embedded else tag.indent
    inner_indent = base + indent_step
    lines = [f"<{tag.tag}"]
    for attr in tag.attrs:
        if attr.value is None:
            lines.append(inner_indent + attr.name)
        else:
            lines.append(inner_indent + f"{attr.name}={normalise_attr_value(attr.value)}")
    lines.append(base + ("/>" if tag.self_closing else ">"))
    return ("\n").join(lines)


def render_tag(tag: OpenTag, line_width: int, indent_step: str, column_at_start: int) -> str:
    inline = render_inline(tag)
    # Does the inline form fit? `column_at_start` is the column of `<` on
    # its original line (0-based). The rendered line includes everything
    # from column_at_start through the end of the tag.
    if "\n" not in inline and column_at_start + len(inline) <= line_width:
        return inline
    if "\n" not in inline and tag.embedded and len(inline) <= line_width:
        return inline
    return render_multiline(tag, indent_step)


def column_of(source: str, index: int) -> int:
    """0-based column of `source[index]` on its line."""
    line_start = source.rfind("\n", 0, index) + 1
    return index - line_start


def format_source(source: str, line_width: int = 100, indent_step: str = "  ") -> str:
    tags = find_open_tags(source)
    if not tags:
        return source
    out: list[str] = []
    cursor = 0
    for tag in tags:
        out.append(source[cursor : tag.start])
        col = column_of(source, tag.start)
        out.append(render_tag(tag, line_width=line_width, indent_step=indent_step, column_at_start=col))
        cursor = tag.end
    out.append(source[cursor:])
    return "".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Format JSX layout in .cppx / .hx files")
    parser.add_argument("files", nargs="*", type=pathlib.Path, help="files to format; omit to read from stdin")
    parser.add_argument("--stdin", action="store_true", help="force reading from stdin even if files are given")
    parser.add_argument("--in-place", "-i", action="store_true", help="rewrite files in place instead of printing to stdout")
    parser.add_argument("--check", action="store_true", help="exit 1 if any file would change; don't write anything")
    parser.add_argument("--line-width", type=int, default=100, help="target line width (default: 100)")
    parser.add_argument("--indent", default="  ", help="indent step used when wrapping attributes (default: 2 spaces)")
    args = parser.parse_args(argv)

    if args.stdin or not args.files:
        source = sys.stdin.read()
        try:
            formatted = format_source(source, line_width=args.line_width, indent_step=args.indent)
        except FormatError as err:
            print(f"error: {err}", file=sys.stderr)
            return 2
        if args.check:
            return 0 if formatted == source else 1
        sys.stdout.write(formatted)
        return 0

    changed = False
    for path in args.files:
        source = path.read_text()
        try:
            formatted = format_source(source, line_width=args.line_width, indent_step=args.indent)
        except FormatError as err:
            print(f"{path}:{err}", file=sys.stderr)
            return 2
        differs = formatted != source
        if differs:
            changed = True
        if args.check:
            if differs:
                print(f"would reformat {path}", file=sys.stderr)
            continue
        if args.in_place:
            if differs:
                path.write_text(formatted)
                print(f"reformatted {path}", file=sys.stderr)
        else:
            sys.stdout.write(formatted)
    if args.check and changed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
