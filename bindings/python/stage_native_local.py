# Stage natives for local editable testing (Windows x64 from Release).
from __future__ import annotations

import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEST = Path(__file__).resolve().parent / "pler_metric" / "_native" / "windows-x64"


def main() -> None:
    DEST.mkdir(parents=True, exist_ok=True)
    tmp = Path(tempfile.mkdtemp(prefix="pler_py_stage_"))
    try:
        subprocess.check_call(
            [
                "gh",
                "release",
                "download",
                "v2.0.0",
                "-R",
                "GlebVoronkov03/PLER",
                "-D",
                str(tmp),
                "--clobber",
                "-p",
                "pler-2.0-win64.zip",
            ]
        )
        zpath = tmp / "pler-2.0-win64.zip"
        with zipfile.ZipFile(zpath) as zf:
            zf.extractall(tmp / "x")
        exe = next((tmp / "x").rglob("pler.exe"))
        dll = next((tmp / "x").rglob("pler.dll"))
        shutil.copy2(exe, DEST / "pler.exe")
        shutil.copy2(dll, DEST / "pler.dll")
        for cudart in exe.parent.glob("cudart64_*.dll"):
            shutil.copy2(cudart, DEST / cudart.name)
        print("staged", DEST)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
