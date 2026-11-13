#!/usr/bin/env python3
"""Verify PLER C ABI v2.1: in-memory API matches file API on sample meshes."""
from __future__ import annotations

import ctypes
import os
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]  # native/pler_c/tests -> repo root
DLL = ROOT / "dist" / "pler-2.1" / "bin" / "pler.dll"
if not DLL.is_file():
    DLL = ROOT / "native" / "build-abi21" / "pler_c" / "pler.dll"
SAMPLE_REF = ROOT / "samples" / "ref_unit_sphere.obj"
SAMPLE_TEST = ROOT / "samples" / "test_unit_sphere_lod.obj"


class pler_options(ctypes.Structure):
    _fields_ = [
        ("num_rays", ctypes.c_int),
        ("min_rays", ctypes.c_int),
        ("max_rays", ctypes.c_int),
        ("align_mode", ctypes.c_int),
        ("compute_tsi", ctypes.c_int),
        ("converge", ctypes.c_int),
        ("prefer_cuda", ctypes.c_int),
        ("voxel_resolution", ctypes.c_int),
        ("sphere_margin", ctypes.c_double),
        ("cache_dir", ctypes.c_char * 512),
    ]


class pler_result(ctypes.Structure):
    _fields_ = [
        ("pler_db", ctypes.c_double),
        ("mse", ctypes.c_double),
        ("mean_error", ctypes.c_double),
        ("max_error", ctypes.c_double),
        ("peak", ctypes.c_double),
        ("miss_rate_ref", ctypes.c_double),
        ("miss_rate_test", ctypes.c_double),
        ("num_rays", ctypes.c_int),
        ("computation_time_s", ctypes.c_double),
        ("tsi", ctypes.c_double),
        ("volume_iou", ctypes.c_double),
        ("ok", ctypes.c_int),
        ("backend", ctypes.c_char * 32),
        ("error", ctypes.c_char * 512),
        ("pler_db_raw", ctypes.c_double),
    ]


def load(dll: Path):
    os.add_dll_directory(str(dll.parent))
    lib = ctypes.CDLL(str(dll))
    lib.pler_version_string.restype = ctypes.c_char_p
    lib.pler_options_init.argtypes = [ctypes.POINTER(pler_options)]
    lib.pler_compute_files.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.POINTER(pler_options),
        ctypes.POINTER(pler_result),
    ]
    lib.pler_compute_files.restype = ctypes.c_int
    lib.pler_reference_create.argtypes = [
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint32),
        ctypes.c_size_t,
        ctypes.POINTER(pler_options),
        ctypes.POINTER(ctypes.c_void_p),
    ]
    lib.pler_reference_create.restype = ctypes.c_int
    lib.pler_compare_mesh.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint32),
        ctypes.c_size_t,
        ctypes.POINTER(pler_result),
    ]
    lib.pler_compare_mesh.restype = ctypes.c_int
    lib.pler_reference_destroy.argtypes = [ctypes.c_void_p]
    lib.pler_last_error.restype = ctypes.c_char_p
    return lib


def load_obj(path: Path):
    vs = []
    fs = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            vs.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("f "):
            idx = [int(tok.split("/")[0]) - 1 for tok in line.split()[1:]]
            for i in range(1, len(idx) - 1):
                fs.append([idx[0], idx[i], idx[i + 1]])
    return np.asarray(vs, np.float32), np.asarray(fs, np.uint32)


def write_obj(path: Path, v: np.ndarray, f: np.ndarray) -> None:
    with path.open("w", encoding="utf-8") as fh:
        for p in v:
            fh.write(f"v {p[0]:.9f} {p[1]:.9f} {p[2]:.9f}\n")
        for tri in f:
            fh.write(f"f {tri[0]+1} {tri[1]+1} {tri[2]+1}\n")


def main() -> int:
    if not DLL.is_file():
        print(f"MISSING DLL {DLL}", file=sys.stderr)
        return 2
    if not SAMPLE_REF.is_file():
        print(f"MISSING sample {SAMPLE_REF}", file=sys.stderr)
        return 2

    lib = load(DLL)
    ver = lib.pler_version_string().decode()
    print(f"DLL={DLL} version={ver}")
    assert ver.startswith("2.1"), ver

    v, f = load_obj(SAMPLE_REF)
    opt = pler_options()
    lib.pler_options_init(ctypes.byref(opt))
    opt.num_rays = 1000
    opt.prefer_cuda = 0
    opt.cache_dir = b".pler_cache"

    v_arr = (ctypes.c_float * v.size)(*v.reshape(-1))
    f_arr = (ctypes.c_uint32 * f.size)(*f.reshape(-1))

    # identical via memory API
    ref = ctypes.c_void_p()
    rc = lib.pler_reference_create(
        v_arr, v.shape[0], f_arr, f.shape[0], ctypes.byref(opt), ctypes.byref(ref)
    )
    assert rc == 0, (rc, lib.pler_last_error())
    out_mem = pler_result()
    rc = lib.pler_compare_mesh(
        ref, v_arr, v.shape[0], f_arr, f.shape[0], ctypes.byref(out_mem)
    )
    lib.pler_reference_destroy(ref)
    assert rc == 0 and out_mem.ok == 1
    print(
        f"identical mem: capped={out_mem.pler_db:.6f} raw={out_mem.pler_db_raw} "
        f"mse={out_mem.mse:.3e} miss={out_mem.miss_rate_ref:.4f}"
    )
    assert out_mem.pler_db >= 99.999
    assert out_mem.miss_rate_ref < 0.01
    assert out_mem.mse <= 1e-10

    # scale ×0.8: memory vs file
    v2 = (v * 0.8).astype(np.float32)
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        ref_p = td / "ref.obj"
        test_p = td / "test.obj"
        write_obj(ref_p, v, f)
        write_obj(test_p, v2, f)
        out_file = pler_result()
        rc = lib.pler_compute_files(
            str(ref_p).encode(),
            str(test_p).encode(),
            ctypes.byref(opt),
            ctypes.byref(out_file),
        )
        assert rc == 0 and out_file.ok == 1

        ref2 = ctypes.c_void_p()
        rc = lib.pler_reference_create(
            v_arr, v.shape[0], f_arr, f.shape[0], ctypes.byref(opt), ctypes.byref(ref2)
        )
        assert rc == 0
        v2_arr = (ctypes.c_float * v2.size)(*v2.reshape(-1))
        out_m2 = pler_result()
        rc = lib.pler_compare_mesh(
            ref2, v2_arr, v2.shape[0], f_arr, f.shape[0], ctypes.byref(out_m2)
        )
        lib.pler_reference_destroy(ref2)
        assert rc == 0 and out_m2.ok == 1

    diff = abs(out_file.pler_db - out_m2.pler_db)
    print(
        f"scale0.8 file={out_file.pler_db:.6f} raw={out_file.pler_db_raw:.6f} | "
        f"mem={out_m2.pler_db:.6f} raw={out_m2.pler_db_raw:.6f} | diff={diff:.6e}"
    )
    assert out_file.pler_db_raw + 1e-9 >= out_file.pler_db
    # Shared-sphere (files) vs ref-only sphere (cached ref) can differ slightly.
    assert diff < 1.0, f"file vs mem PLER diverge too much: {diff}"

    # LOD sample via files (sanity)
    if SAMPLE_TEST.is_file():
        out_lod = pler_result()
        rc = lib.pler_compute_files(
            str(SAMPLE_REF).encode(),
            str(SAMPLE_TEST).encode(),
            ctypes.byref(opt),
            ctypes.byref(out_lod),
        )
        assert rc == 0 and out_lod.ok == 1
        print(
            f"lod file: capped={out_lod.pler_db:.4f} raw={out_lod.pler_db_raw:.4f} "
            f"mse={out_lod.mse:.6g}"
        )
        assert 10.0 < out_lod.pler_db < 100.0

    print("ALL PASS")
    return 0


if __name__ == "__main__":
    # expose last_error
    raise SystemExit(main())
