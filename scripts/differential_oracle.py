#!/usr/bin/env python3
"""Check the reproducible compatibility/provenance oracle for Paginotto.

This is deliberately an engineering oracle, not a legal originality test.  The
historical upstream is an ESP32 firmware without a host-side black-box harness,
so the check combines explicit contract-to-test mappings with source and
format invariants that can be evaluated reproducibly in CI.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


CONTRACTS = {
    "reader core": {
        "sources": [
            "Paginotto/src/pure/paginator.cpp",
            "Paginotto/src/pure/text_util.cpp",
            "Paginotto/src/pure/library_nav.cpp",
        ],
        "tests": [
            "test/test_paginator.cpp",
            "test/test_text_util.cpp",
            "test/test_library_nav.cpp",
        ],
    },
    "storage and persistence": {
        "sources": [
            "Paginotto/src/pure/page_cache_format.cpp",
            "Paginotto/src/storage/book_metadata.cpp",
        ],
        "tests": [
            "test/test_page_cache_format.cpp",
            "test/test_book_metadata.cpp",
        ],
    },
    "reader integration": {
        "sources": [
            "Paginotto/src/pure/reader_integration.cpp",
            "Paginotto/src/pure/reader_resume.cpp",
            "Paginotto/src/pure/reader_scan.cpp",
            "Paginotto/src/pure/reader_navigation.cpp",
        ],
        "tests": [
            "test/test_reader_integration.cpp",
            "test/test_reader_resume.cpp",
            "test/test_reader_scan.cpp",
            "test/test_reader_navigation.cpp",
        ],
    },
    "device and UI policy": {
        "sources": [
            "Paginotto/src/pure/button_policy.cpp",
            "Paginotto/src/pure/bionic_text.cpp",
            "Paginotto/src/pure/device_policy.cpp",
            "Paginotto/src/pure/screensaver_policy.cpp",
            "Paginotto/src/pure/ui_policy.cpp",
        ],
        "tests": [
            "test/test_button_policy.cpp",
            "test/test_bionic_text.cpp",
            "test/test_device_policy.cpp",
            "test/test_screensaver_policy.cpp",
            "test/test_ui_policy.cpp",
        ],
    },
    "connectivity and maintenance": {
        "sources": [
            "Paginotto/src/pure/ota_policy.cpp",
            "Paginotto/src/pure/wifi_credentials.cpp",
        ],
        "tests": [
            "test/test_ota_policy.cpp",
            "test/test_wifi_credentials.cpp",
        ],
    },
}

PERSISTENT_MARKERS = {
    "page offset key": "_off",
    "legacy page key": "_p",
    "page anchor key": "_anc",
    "anchor offset key": "_anc_at",
}


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=repo, check=True, text=True, capture_output=True
    )
    return result.stdout


def exact_matches(repo: Path, upstream_ref: str) -> list[str]:
    matches = []
    for current in git(repo, "ls-files", "Paginotto").splitlines():
        if not current.endswith((".c", ".cc", ".cpp", ".h", ".hpp", ".ino", ".csv")):
            continue
        counterpart = current.replace("Paginotto/", "Pala_One_2_1/", 1)
        current_blob = git(repo, "show", f"HEAD:{current}")
        try:
            upstream_blob = git(repo, "show", f"{upstream_ref}:{counterpart}")
        except subprocess.CalledProcessError:
            continue
        if current_blob == upstream_blob:
            matches.append(current)
    return matches


def check(repo: Path, upstream_ref: str) -> tuple[list[str], list[str], list[str]]:
    errors: list[str] = []
    notes: list[str] = []
    exact = exact_matches(repo, upstream_ref)
    if exact != ["Paginotto/partitions.csv"]:
        errors.append(f"unexpected exact-match set: {exact!r}")
    else:
        notes.append("exact-match allowlist: Paginotto/partitions.csv only")

    for name, contract in CONTRACTS.items():
        missing_sources = [p for p in contract["sources"] if not (repo / p).is_file()]
        missing_tests = [p for p in contract["tests"] if not (repo / p).is_file()]
        if missing_sources:
            errors.append(f"{name}: missing sources: {', '.join(missing_sources)}")
        if missing_tests:
            errors.append(f"{name}: missing tests: {', '.join(missing_tests)}")
        if not missing_sources and not missing_tests:
            notes.append(f"{name}: {len(contract['sources'])} sources mapped to {len(contract['tests'])} host tests")

    storage_text = "\n".join(
        (repo / path).read_text(errors="replace")
        for path in ("Paginotto/src/storage/book_metadata.cpp", "Paginotto/src/pure/page_cache_format.cpp")
    )
    for label, marker in PERSISTENT_MARKERS.items():
        if marker not in storage_text:
            errors.append(f"missing persistent compatibility marker: {label} ({marker})")
    if not errors:
        notes.append("persistent page-state markers are present")

    return errors, notes, exact


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--upstream-ref", default="upstream/main")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()

    try:
        errors, notes, exact = check(repo, args.upstream_ref)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"oracle error: {exc}", file=sys.stderr)
        return 2

    status = "PASS" if not errors else "FAIL"
    lines = [
        "# Differential compatibility oracle",
        "",
        f"Status: **{status}**",
        f"Upstream ref: `{args.upstream_ref}`",
        "",
        "This is a reproducible engineering check, not a legal originality certificate.",
        "The upstream firmware has no equivalent host black-box harness, so the oracle",
        "checks contract-to-test coverage, persistent-format markers and exact-match",
        "allowlisting. Runtime hardware behaviour still requires device testing.",
        "",
        "## Checks",
    ]
    lines.extend(f"- {note}." for note in notes)
    lines.extend(f"- ERROR: {error}." for error in errors)
    lines += [
        "",
        "## Current exact matches",
        *[f"- `{path}`" for path in exact],
        "",
        "## Limitations",
        "- This oracle does not prove source-code originality or licensing rights.",
        "- It does not replace hardware testing of display, input, radio, sleep and OTA.",
        "- It intentionally preserves the upstream-compatible persistent keys and formats.",
    ]
    report = "\n".join(lines) + "\n"
    if args.report:
        args.report.write_text(report)
    print(report, end="")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
