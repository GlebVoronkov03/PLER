"""ctypes bindings mirroring native/pler_c/include/pler.h."""
from __future__ import annotations

import ctypes
from ctypes import (
    POINTER,
    c_char,
    c_char_p,
    c_double,
    c_int,
    c_void_p,
)
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Union

from . import _native_paths


class AlignMode(IntEnum):
    OFF = 0
    VOLUME_IOU = 1
    ICP = 2
    IOU_THEN_ICP = 3


class pler_options(ctypes.Structure):
    _fields_ = [
        ("num_rays", c_int),
        ("min_rays", c_int),
        ("max_rays", c_int),
        ("align_mode", c_int),
        ("compute_tsi", c_int),
        ("converge", c_int),
        ("prefer_cuda", c_int),
        ("voxel_resolution", c_int),
        ("sphere_margin", c_double),
        ("cache_dir", c_char * 512),
    ]


class pler_result(ctypes.Structure):
    _fields_ = [
        ("pler_db", c_double),
        ("mse", c_double),
        ("mean_error", c_double),
        ("max_error", c_double),
        ("peak", c_double),
        ("miss_rate_ref", c_double),
        ("miss_rate_test", c_double),
        ("num_rays", c_int),
        ("computation_time_s", c_double),
        ("tsi", c_double),
        ("volume_iou", c_double),
        ("ok", c_int),
        ("backend", c_char * 32),
        ("error", c_char * 512),
    ]


@dataclass
class Options:
    num_rays: int = 0
    min_rays: int = 1000
    max_rays: int = 20000
    align_mode: AlignMode = AlignMode.OFF
    compute_tsi: bool = False
    converge: bool = False
    prefer_cuda: bool = True
    voxel_resolution: int = 48
    sphere_margin: float = 1.02
    cache_dir: str = ".pler_cache"

    def to_ctypes(self) -> pler_options:
        o = pler_options()
        o.num_rays = int(self.num_rays)
        o.min_rays = int(self.min_rays)
        o.max_rays = int(self.max_rays)
        o.align_mode = int(self.align_mode)
        o.compute_tsi = 1 if self.compute_tsi else 0
        o.converge = 1 if self.converge else 0
        o.prefer_cuda = 1 if self.prefer_cuda else 0
        o.voxel_resolution = int(self.voxel_resolution)
        o.sphere_margin = float(self.sphere_margin)
        raw = self.cache_dir.encode("utf-8")[:511]
        o.cache_dir = raw
        return o


@dataclass
class Result:
    pler_db: float
    mse: float
    mean_error: float
    max_error: float
    peak: float
    miss_rate_ref: float
    miss_rate_test: float
    num_rays: int
    computation_time_s: float
    tsi: float
    volume_iou: float
    ok: bool
    backend: str
    error: str

    @classmethod
    def from_ctypes(cls, r: pler_result) -> "Result":
        return cls(
            pler_db=float(r.pler_db),
            mse=float(r.mse),
            mean_error=float(r.mean_error),
            max_error=float(r.max_error),
            peak=float(r.peak),
            miss_rate_ref=float(r.miss_rate_ref),
            miss_rate_test=float(r.miss_rate_test),
            num_rays=int(r.num_rays),
            computation_time_s=float(r.computation_time_s),
            tsi=float(r.tsi),
            volume_iou=float(r.volume_iou),
            ok=bool(r.ok),
            backend=r.backend.decode("utf-8", errors="replace").rstrip("\x00"),
            error=r.error.decode("utf-8", errors="replace").rstrip("\x00"),
        )


_lib: Optional[ctypes.CDLL] = None


def _load() -> ctypes.CDLL:
    global _lib
    if _lib is not None:
        return _lib
    path = _native_paths.shared_library_path()
    _lib = ctypes.CDLL(str(path))

    _lib.pler_version_string.restype = c_char_p
    _lib.pler_version_string.argtypes = []

    _lib.pler_version_major.restype = c_int
    _lib.pler_version_major.argtypes = []
    _lib.pler_version_minor.restype = c_int
    _lib.pler_version_minor.argtypes = []
    _lib.pler_version_patch.restype = c_int
    _lib.pler_version_patch.argtypes = []

    _lib.pler_build_has_cuda.restype = c_int
    _lib.pler_build_has_cuda.argtypes = []

    _lib.pler_options_init.restype = None
    _lib.pler_options_init.argtypes = [POINTER(pler_options)]

    _lib.pler_compute_files.restype = c_int
    _lib.pler_compute_files.argtypes = [
        c_char_p,
        c_char_p,
        POINTER(pler_options),
        POINTER(pler_result),
    ]

    _lib.pler_last_error.restype = c_char_p
    _lib.pler_last_error.argtypes = []
    return _lib


def version_string() -> str:
    s = _load().pler_version_string()
    return s.decode("utf-8") if s else ""


def version_major() -> int:
    return int(_load().pler_version_major())


def version_minor() -> int:
    return int(_load().pler_version_minor())


def version_patch() -> int:
    return int(_load().pler_version_patch())


def build_has_cuda() -> bool:
    return bool(_load().pler_build_has_cuda())


def options_init() -> Options:
    raw = pler_options()
    _load().pler_options_init(ctypes.byref(raw))
    return Options(
        num_rays=int(raw.num_rays),
        min_rays=int(raw.min_rays),
        max_rays=int(raw.max_rays),
        align_mode=AlignMode(int(raw.align_mode)),
        compute_tsi=bool(raw.compute_tsi),
        converge=bool(raw.converge),
        prefer_cuda=bool(raw.prefer_cuda),
        voxel_resolution=int(raw.voxel_resolution),
        sphere_margin=float(raw.sphere_margin),
        cache_dir=raw.cache_dir.decode("utf-8", errors="replace").rstrip("\x00")
        or ".pler_cache",
    )


def last_error() -> str:
    s = _load().pler_last_error()
    return s.decode("utf-8") if s else ""


def compute_files(
    ref_path: str,
    test_path: str,
    opt: Optional[Union[Options, pler_options]] = None,
) -> Result:
    lib = _load()
    if opt is None:
        opt = options_init()
    if isinstance(opt, Options):
        copt = opt.to_ctypes()
    else:
        copt = opt
    out = pler_result()
    rc = lib.pler_compute_files(
        ref_path.encode("utf-8"),
        test_path.encode("utf-8"),
        ctypes.byref(copt),
        ctypes.byref(out),
    )
    result = Result.from_ctypes(out)
    if rc != 0 and result.ok:
        result.ok = False
        if not result.error:
            result.error = last_error() or f"pler_compute_files returned {rc}"
    return result
