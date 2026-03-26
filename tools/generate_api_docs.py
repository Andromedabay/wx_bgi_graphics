#!/usr/bin/env python3
"""Generate API reference markdown from exported C-style headers."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Iterable

PROTOTYPE_RE = re.compile(r"^\s*BGI_API\s+(.+?)\s*BGI_CALL\s+([A-Za-z0-9_]+)\s*\((.*?)\);\s*$")


def classify(name: str) -> str:
    if name.startswith("wxbgi_"):
        return "Advanced OpenGL API"
    return "Classic BGI API"


def parse_header(header_path: Path) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    for line in header_path.read_text(encoding="utf-8").splitlines():
        match = PROTOTYPE_RE.match(line)
        if not match:
            continue
        return_type, name, params = match.groups()
        entries.append(
            {
                "name": name,
                "return_type": return_type.strip(),
                "params": params.strip(),
                "category": classify(name),
            }
        )
    return entries


def render_markdown(entries: Iterable[dict[str, str]]) -> str:
    grouped: dict[str, list[dict[str, str]]] = {}
    for entry in entries:
        grouped.setdefault(entry["category"], []).append(entry)

    lines: list[str] = []
    lines.append("# API Reference")
    lines.append("")
    lines.append("This file is generated. Do not edit manually.")
    lines.append("")

    for category in sorted(grouped.keys()):
        lines.append(f"## {category}")
        lines.append("")
        for entry in sorted(grouped[category], key=lambda item: item["name"]):
            signature = f"{entry['return_type']} {entry['name']}({entry['params']})"
            lines.append(f"- `{signature}`")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate API markdown from headers")
    parser.add_argument("--output", required=True, help="Output markdown file")
    parser.add_argument("headers", nargs="+", help="Header files to scan")
    args = parser.parse_args()

    entries: list[dict[str, str]] = []
    for header in args.headers:
        header_path = Path(header)
        if not header_path.exists():
            raise FileNotFoundError(f"Header not found: {header_path}")
        entries.extend(parse_header(header_path))

    markdown = render_markdown(entries)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(markdown, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
