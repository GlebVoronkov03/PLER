"""Console entry point: exec the bundled native pler CLI."""
from __future__ import annotations

import os
import sys

from . import _native_paths


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    binary = _native_paths.cli_binary_path()
    if sys.platform.startswith("win"):
        # os.execv on Windows replaces process; use subprocess for reliability with args
        import subprocess

        return subprocess.call([str(binary), *args])
    os.execv(str(binary), [str(binary), *args])
    return 0  # pragma: no cover


if __name__ == "__main__":
    raise SystemExit(main())
