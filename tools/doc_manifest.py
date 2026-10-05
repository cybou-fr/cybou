#!/usr/bin/env python3
# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

"""
CYBOU documentation manifest generator and consistency checker.

Computes line counts and SHA-256 prefixes over normalized LF text for all
authoritative documents, specifications, and public llms metadata.
"""

import argparse
import glob
import hashlib
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

INCLUDED_ROOT_FILES = [
    "AGENTS.md",
    "CONTRIBUTING.md",
    "INSTALL.md",
    "README.md",
    "SECURITY.md",
]


def collect_manifest_files():
    files = list(INCLUDED_ROOT_FILES)

    docs_cybou = sorted(glob.glob(str(ROOT / "docs" / "cybou" / "*.md")))
    for p in docs_cybou:
        rel = os.path.relpath(p, ROOT).replace("\\", "/")
        files.append(rel)

    spec_files = sorted(glob.glob(str(ROOT / "spec" / "*.*")))
    for p in spec_files:
        rel = os.path.relpath(p, ROOT).replace("\\", "/")
        files.append(rel)

    files.append("www/llms.txt")
    return files


def compute_file_stats(rel_path):
    full_path = ROOT / rel_path
    if not full_path.exists():
        raise FileNotFoundError(f"Manifest target file missing: {rel_path}")

    raw = full_path.read_bytes()
    normalized = raw.replace(b"\r\n", b"\n")
    lines = len(normalized.splitlines())
    sha_prefix = hashlib.sha256(normalized).hexdigest()[:16]
    return lines, sha_prefix


def generate_manifest_content(date_str="2026-10-01"):
    files = collect_manifest_files()
    lines = [
        f"# CYBOU documentation manifest — {date_str}",
        "",
        "This inventory records the current implementation authority, product and",
        "protocol documents, and machine-readable specifications, excluding this",
        "manifest. SHA-256 prefixes are computed from UTF-8 text after normalizing",
        "line endings to LF. Line counts use normalized text lines. Regenerate after",
        "editing an included file.",
        "",
        "| File | Lines | SHA-256 prefix |",
        "|---|---:|---|",
    ]

    for rel_path in files:
        line_count, sha_prefix = compute_file_stats(rel_path)
        lines.append(f"| {rel_path} | {line_count} | {sha_prefix} |")

    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="CYBOU documentation manifest manager")
    parser.add_argument("--check", action="store_true", help="Check that MANIFEST.md matches disk")
    parser.add_argument("--generate", action="store_true", help="Generate and overwrite MANIFEST.md")
    parser.add_argument("--date", default="2026-10-01", help="Manifest date header (YYYY-MM-DD)")
    args = parser.parse_args()

    manifest_path = ROOT / "MANIFEST.md"

    if args.check:
        if not manifest_path.exists():
            print("ERROR: MANIFEST.md does not exist", file=sys.stderr)
            sys.exit(1)

        expected = generate_manifest_content(date_str=args.date)
        # Compare normalized contents
        actual_raw = manifest_path.read_bytes().replace(b"\r\n", b"\n").decode("utf-8")

        # Ignore date line difference if checking
        expected_lines = expected.splitlines()[2:]
        actual_lines = actual_raw.splitlines()[2:]

        if expected_lines != actual_lines:
            print("ERROR: MANIFEST.md does not match disk content!", file=sys.stderr)
            for i, (exp, act) in enumerate(zip(expected_lines, actual_lines)):
                if exp != act:
                    print(f"  Line {i+3} mismatch:", file=sys.stderr)
                    print(f"    Expected: {exp}", file=sys.stderr)
                    print(f"    Actual:   {act}", file=sys.stderr)
            if len(expected_lines) != len(actual_lines):
                print(f"  Line count mismatch: expected {len(expected_lines)}, got {len(actual_lines)}", file=sys.stderr)
            sys.exit(1)

        print("OK: MANIFEST.md is up to date and consistent.")
        sys.exit(0)

    # Default to generate
    content = generate_manifest_content(date_str=args.date)
    manifest_path.write_bytes(content.encode("utf-8"))
    print(f"Generated MANIFEST.md ({len(collect_manifest_files())} files).")


if __name__ == "__main__":
    main()
