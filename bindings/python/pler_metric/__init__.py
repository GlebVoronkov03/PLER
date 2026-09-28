"""pler-metric: ctypes wrapper around the PLER C ABI."""

from . import _lib
from ._lib import (
    AlignMode,
    Options,
    Result,
    build_has_cuda,
    compute_files,
    last_error,
    options_init,
    version_major,
    version_minor,
    version_patch,
    version_string,
)

__all__ = [
    "AlignMode",
    "Options",
    "Result",
    "build_has_cuda",
    "compute_files",
    "last_error",
    "options_init",
    "version_major",
    "version_minor",
    "version_patch",
    "version_string",
]

__version__ = "2.0.0"
