#!/usr/bin/env python3
# Copyright (c) 2026 Stanislav Saveliev
# SPDX-License-Identifier: Apache-2.0
"""Check documentation classification, catalogue coverage and local file links.

This structural check does not resolve architecture disagreements or certify
the truth of a CURRENT document. Content/source audit is tracked separately.
"""
import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs" / "cybou"
STATUSES = {"CURRENT", "PROPOSAL", "EVIDENCE", "HISTORICAL"}


def check():
    errors = []
    files = sorted(DOCS.rglob("*.md"))
    catalogue = (DOCS / "README.md").read_text(encoding="utf-8")
    for path in files:
        text = path.read_text(encoding="utf-8")
        match = re.search(r"^Status: (\w+)\s*$", text, re.MULTILINE)
        if not match or match[1] not in STATUSES:
            errors.append(f"{path.relative_to(ROOT)}: missing document classification")
        if "history" in path.relative_to(DOCS).parts and (not match or match[1] != "HISTORICAL"):
            errors.append(f"{path.relative_to(ROOT)}: archive must be HISTORICAL")
        if path.name != "README.md" and f"({path.relative_to(DOCS).as_posix()})" not in catalogue:
            errors.append(f"{path.relative_to(ROOT)}: absent from document catalogue")
        # Fenced code contains examples, not navigation links.
        body = re.sub(r"^(```|~~~).*?^\1[^\n]*$", lambda m: "\n" * m[0].count("\n"),
                      text, flags=re.MULTILINE | re.DOTALL)
        for link in re.finditer(r"\[[^\]\n]*\]\(([^\s)]+)(?:\s+\"[^\"]*\")?\)", body):
            target = unquote(link[1].strip("<>"))
            if re.match(r"[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
                continue
            filename = target.partition("#")[0].partition("?")[0]
            if not filename:
                continue
            resolved = ROOT / filename.lstrip("/") if filename.startswith("/") else path.parent / filename
            if not resolved.exists():
                line = body[:link.start()].count("\n") + 1
                errors.append(f"{path.relative_to(ROOT)}:{line}: missing link target {target}")
    decisions = (DOCS / "24_DECISIONS.md").read_text(encoding="utf-8")
    for decision in (151, 248, 257, 268, 269, 270):
        if re.search(rf"^\| DEC-{decision} \|", decisions, re.MULTILINE):
            errors.append(f"Cancelled DEC-{decision} remains in current register")
    return files, errors


if __name__ == "__main__":
    files, errors = check()
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        sys.exit(1)
    print(f"OK: {len(files)} classified/indexed documents; local file links resolve; cancelled decisions archived.")
    print("Content/protocol/legal acceptance is outside this structural check.")
