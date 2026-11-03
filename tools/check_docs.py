#!/usr/bin/env python3
"""Validate local documentation links and static GitHub Pages files.

No external HTTP requests or third-party Python packages. CI runs this before
publishing and on documentation pull requests.
"""
from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / "showcase"
MARKDOWN_LINK = re.compile(r"!?(?:\\[[^\\]\\n]*\\])\\(([^)]+)\\)")


class ParsedPage(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.ids: set[str] = set()
        self.links: list[tuple[str, str]] = []
        self.has_title = False
        self.has_h1 = False
        self.has_description = False
        self.has_viewport = False
        self.has_language = False

    def handle_starttag(self, tag: str, attributes: list[tuple[str, str | None]]) -> None:
        attrs = dict(attributes)
        if attrs.get("id"):
            self.ids.add(attrs["id"])
        if tag == "html" and attrs.get("lang"):
            self.has_language = True
        if tag == "title":
            self.has_title = True
        if tag == "h1":
            self.has_h1 = True
        if tag == "meta" and attrs.get("name") == "description":
            self.has_description = True
        if tag == "meta" and attrs.get("name") == "viewport":
            self.has_viewport = True
        for key in ("href", "src"):
            if attrs.get(key):
                self.links.append((key, attrs[key]))


def local_target(source: Path, raw: str) -> tuple[Path, str] | None:
    parsed = urlsplit(raw.strip())
    if parsed.scheme or parsed.netloc or raw.startswith("//"):
        return None
    if parsed.path.startswith("/"):
        raise ValueError("root-relative URL would break project subpath deployment")
    path = unquote(parsed.path)
    target = ((source.parent / path) if path else source).resolve()
    try:
        target.relative_to(ROOT)
    except ValueError as e:
        raise ValueError("local link escapes repository") from e
    if target.is_dir():
        target /= "index.html"
    return target, unquote(parsed.fragment)


def check() -> int:
    errors: list[str] = []
    html_files = sorted(SITE.rglob("*.html"))
    pages: dict[Path, ParsedPage] = {}
    for source in html_files:
        parsed = ParsedPage()
        parsed.feed(source.read_text(encoding="utf-8"))
        pages[source.resolve()] = parsed
        if not all((parsed.has_title, parsed.has_h1, parsed.has_description,
                    parsed.has_viewport, parsed.has_language)):
            errors.append(f"{source.relative_to(ROOT)}: missing title/H1/description/viewport/lang")
    for source, page in pages.items():
        for kind, url in page.links:
            try:
                target_data = local_target(source, url)
                if target_data is None:
                    continue
                target, fragment = target_data
                if not target.is_file():
                    errors.append(f"{source.relative_to(ROOT)}: missing {kind} {url!r}")
                elif fragment and target.suffix.lower() == ".html":
                    destination = pages.get(target)
                    if destination is None or fragment not in destination.ids:
                        errors.append(f"{source.relative_to(ROOT)}: missing fragment {url!r}")
            except ValueError as error:
                errors.append(f"{source.relative_to(ROOT)}: {url!r}: {error}")
    markdown_files = [ROOT / name for name in ("README.md", "CONTRIBUTING.md",
                                               "SECURITY.md", "CHANGELOG.md")]
    markdown_files += sorted((ROOT / "docs").rglob("*.md"))
    markdown_files += sorted((ROOT / "showcase").rglob("*.md"))
    for source in markdown_files:
        for match in MARKDOWN_LINK.finditer(source.read_text(encoding="utf-8")):
            url = match.group(1).split(" \"", 1)[0].strip("<>")
            try:
                target_data = local_target(source, url)
                if target_data is None:
                    continue
                target, _ = target_data
                if not target.exists():
                    errors.append(f"{source.relative_to(ROOT)}: broken Markdown link {url!r}")
            except ValueError as error:
                errors.append(f"{source.relative_to(ROOT)}: {url!r}: {error}")
    if not html_files:
        errors.append("No website HTML files found")
    if errors:
        for issue in errors:
            print("ERROR:", issue, file=sys.stderr)
        print(f"Documentation check failed: {len(errors)} issue(s)", file=sys.stderr)
        return 1
    print(f"Validated {len(html_files)} HTML pages, "
          f"{len(markdown_files)} Markdown documents, and local references")
    return 0


if __name__ == "__main__":
    sys.exit(check())
