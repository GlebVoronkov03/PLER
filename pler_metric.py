#!/usr/bin/env python3
"""Legacy entry point — delegates to the native `pler` CLI (PLER 2.0).

Do not use the old Python ray-cast path; it cast outward from the centroid
and independently normalized each mesh. The C++ backend implements the paper.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


def _find_pler() -> Path:
    env = shutil.which("pler")
    if env:
        return Path(env)
    here = Path(__file__).resolve().parent
    candidates = [
        here / "native" / "build" / "pler_cli" / "Release" / "pler.exe",
        here / "native" / "build" / "pler_cli" / "Debug" / "pler.exe",
        here / "native" / "build" / "pler_cli" / "pler",
        here / "build" / "pler_cli" / "Release" / "pler.exe",
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(
        "Native pler executable not found. Build native/ (see README) "
        "or add pler to PATH."
    )


def main(argv: list[str] | None = None) -> int:
    argv = list(argv if argv is not None else sys.argv[1:])
    pler = _find_pler()
    # Map old style: python pler_metric.py ref test [rays]
    if len(argv) >= 2 and not argv[0].startswith("-") and argv[0] not in (
        "batch",
        "selftest",
        "gui",
    ):
        # already pler-compatible; if 3rd arg is int, rewrite as --rays
        if len(argv) >= 3 and argv[2].isdigit():
            argv = [argv[0], argv[1], "--rays", argv[2]] + argv[3:]
    cmd = [str(pler)] + argv
    print(f"[pler_metric.py] → {' '.join(cmd)}", file=sys.stderr)
    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
