"""Locate bundled pler CLI and run compares."""
from __future__ import annotations

import os
import platform
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


@dataclass
class RunResult:
    ok: bool
    returncode: int
    stdout: str
    stderr: str
    cache_dir: str
    argv: List[str]


def addon_root() -> Path:
    return Path(__file__).resolve().parent


def platform_bin_dir() -> Path:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "windows":
        key = "windows-x64"
    elif system == "linux":
        key = "linux-x64"
    elif system == "darwin":
        if machine in ("arm64", "aarch64"):
            key = "macos-arm64"
        else:
            key = "macos-x64"
    else:
        raise RuntimeError(f"Unsupported platform: {system} {machine}")
    return addon_root() / "bin" / key


def resolve_pler_binary(override: str = "") -> Path:
    if override:
        p = Path(bpy_path_abs(override))
        if not p.is_file():
            raise FileNotFoundError(f"PLER binary override not found: {p}")
        return p
    root = platform_bin_dir()
    name = "pler.exe" if platform.system().lower() == "windows" else "pler"
    cand = root / name
    if not cand.is_file():
        raise FileNotFoundError(
            f"Bundled PLER binary missing: {cand}\n"
            "Install pler-blender-2.0.zip from the GitHub Release, or set binary override in preferences."
        )
    return cand


def bpy_path_abs(path: str) -> str:
    try:
        import bpy

        return bpy.path.abspath(path)
    except Exception:
        return path


def make_cache_dir() -> str:
    d = Path(tempfile.gettempdir()) / "pler_blender_cache"
    d.mkdir(parents=True, exist_ok=True)
    return str(d)


def build_argv(
    binary: Path,
    ref: str,
    test: str,
    *,
    rays: int = 2000,
    align: str = "off",
    tsi: bool = False,
    force_cpu: bool = False,
    dump_rays: bool = True,
    cache_dir: Optional[str] = None,
) -> List[str]:
    argv = [str(binary), ref, test, "--rays", str(int(rays)), "--align", align]
    if tsi:
        argv.append("--tsi")
    if force_cpu:
        argv.append("--cpu")
    if dump_rays:
        argv.append("--dump-rays")
    # CLI uses default .pler_cache relative to cwd; we set cwd to cache parent via env/workdir
    return argv


def run_pler(
    ref: str,
    test: str,
    *,
    rays: int = 2000,
    align: str = "off",
    tsi: bool = False,
    force_cpu: bool = False,
    dump_rays: bool = True,
    binary_override: str = "",
    timeout: float = 600.0,
) -> RunResult:
    binary = resolve_pler_binary(binary_override)
    cache_dir = make_cache_dir()
    argv = build_argv(
        binary,
        ref,
        test,
        rays=rays,
        align=align,
        tsi=tsi,
        force_cpu=force_cpu,
        dump_rays=dump_rays,
        cache_dir=cache_dir,
    )
    # Run with cwd = cache_dir so .pler_cache / ray dumps land in a known place
    env = os.environ.copy()
    try:
        proc = subprocess.run(
            argv,
            cwd=cache_dir,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env,
        )
        return RunResult(
            ok=proc.returncode == 0,
            returncode=proc.returncode,
            stdout=proc.stdout or "",
            stderr=proc.stderr or "",
            cache_dir=cache_dir,
            argv=argv,
        )
    except subprocess.TimeoutExpired as e:
        return RunResult(
            ok=False,
            returncode=-1,
            stdout=e.stdout or "" if isinstance(e.stdout, str) else "",
            stderr="PLER timed out",
            cache_dir=cache_dir,
            argv=argv,
        )


def ray_dump_path(cache_dir: str) -> Path:
    # dump is written to cache_dir/.pler_cache/ray_depths_last.csv OR cache_dir/ray_depths_last.csv
    # pler uses options.cache_dir default ".pler_cache" relative to cwd
    p1 = Path(cache_dir) / ".pler_cache" / "ray_depths_last.csv"
    p2 = Path(cache_dir) / "ray_depths_last.csv"
    if p1.is_file():
        return p1
    return p2
