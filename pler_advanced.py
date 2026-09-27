"""Advanced PLER — wraps native CLI with --tsi by default."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Re-use locator from pler_metric
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pler_metric import _find_pler  # noqa: E402


def main() -> int:
    argv = sys.argv[1:]
    if argv == ["--create-test-models"]:
        print("Create test models with: python create_test_models.py", file=sys.stderr)
        return 1
    if len(argv) < 2:
        print("Usage: python pler_advanced.py <ref.obj> <test.obj> [extra pler flags]")
        return 1
    pler = _find_pler()
    cmd = [str(pler), argv[0], argv[1], "--tsi"] + argv[2:]
    print(f"[pler_advanced.py] → {' '.join(cmd)}", file=sys.stderr)
    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
