#!/usr/bin/env python3
"""Assemble the firmware-only OTA bundle published to GitHub Pages."""

import argparse
import json
import shutil
import sys
from pathlib import Path


def require_file(path: Path) -> Path:
    if not path.is_file():
        sys.exit(f"missing: {path}\nBuild both PlatformIO environments first.")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--channel", required=True, choices=("dev", "stable"))
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--out", default="site")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    output = (repo / args.out).resolve() / args.channel
    builds = repo / ".pio" / "build"
    output.mkdir(parents=True, exist_ok=True)

    boards = {
        "v1_1": "wireless-paper-v1_1",
        "v1_2": "wireless-paper-v1_2",
    }
    for board, environment in boards.items():
        firmware = require_file(builds / environment / "firmware.bin")
        filename = f"firmware-{board}.bin"
        shutil.copyfile(firmware, output / filename)
        manifest = {
            "name": f"Paginotto ({board.upper()} display)",
            "channel": args.channel,
            "version": args.version,
            "commit": args.commit,
            "firmware": filename,
        }
        (output / f"manifest-{board}.json").write_text(
            json.dumps(manifest, indent=2) + "\n"
        )

    print(f"Assembled OTA bundle -> {output} (version: {args.version})")
    for path in sorted(output.iterdir()):
        print(f"  {path.name:24s} {path.stat().st_size:>10d} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
