#!/usr/bin/env python3
"""Builds switchboard-lab.html: wraps page_template.html in a page skeleton and inlines engine.js."""
from pathlib import Path

here = Path(__file__).resolve().parent
template = (here / "page_template.html").read_text(encoding="utf-8")
engine = (here / "engine.js").read_text(encoding="utf-8")
if "/*ENGINE*/" not in template:
    raise SystemExit("build.py: page_template.html has no /*ENGINE*/ placeholder")

head = '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n'
page = head + template.replace("/*ENGINE*/", engine) + "\n</html>\n"
out = here / "switchboard-lab.html"
out.write_text(page, encoding="utf-8")
print(f"wrote {out.relative_to(here.parent.parent)} ({len(page) // 1024} KB)")
