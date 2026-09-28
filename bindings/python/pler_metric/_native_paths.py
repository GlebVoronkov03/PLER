"""Locate bundled native lib/CLI for the current platform."""
from __future__ import annotations

import platform
import sys
from pathlib import Path


def platform_key() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "windows":
        return "windows-x64"
    if system == "linux":
        return "linux-x64"
    if system == "darwin":
        if machine in ("arm64", "aarch64"):
            return "macos-arm64"
        return "macos-x64"
    raise RuntimeError(f"Unsupported platform: {system} {machine}")


def native_dir() -> Path:
    return Path(__file__).resolve().parent / "_native" / platform_key()


def shared_library_path() -> Path:
    key = platform_key()
    d = native_dir()
    if key.startswith("windows"):
        name = "pler.dll"
    elif key.startswith("macos"):
        name = "libpler.dylib"
    else:
        name = "libpler.so"
    path = d / name
    if not path.is_file():
        raise FileNotFoundError(
            f"Native library missing: {path}\n"
            "Rebuild the wheel with bindings/python/build_wheels.*, "
            "or install a platform wheel from PyPI."
        )
    return path


def cli_binary_path() -> Path:
    d = native_dir()
    name = "pler.exe" if sys.platform.startswith("win") else "pler"
    path = d / name
    if not path.is_file():
        raise FileNotFoundError(
            f"PLER CLI missing: {path}\n"
            "This wheel may be incomplete; reinstall pler-metric for your platform."
        )
    return path
