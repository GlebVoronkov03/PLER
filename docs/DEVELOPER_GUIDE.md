# PLER 2.0 — Developer Guide

This guide is for engineers who **build, modify, integrate, or validate** the PLER metric. For day-to-day measurement usage, see [USER_GUIDE.md](USER_GUIDE.md).

PLER 2.0 is a native C++ full-reference geometric fidelity metric: shared bounding sphere, inward Fibonacci rays, peak-over-MSE score in dB, optional volume-IoU rigid alignment, optional topological similarity (TSI).

---

## Repository layout

```text
_Metric_PLER+/
├── native/                     # CMake project (source of truth for the metric)
│   ├── CMakeLists.txt
│   ├── build.bat               # Windows convenience build
│   ├── pler_core/              # Static library: load → align → cast → score
│   ├── pler_cli/               # pler.exe
│   ├── pler_gui/               # Dear ImGui + GLFW front end
│   ├── pler_cuda/              # Optional CUDA ray cast (sm_75 default)
│   ├── third_party/            # tiny_obj_loader.h
│   └── deps/                   # Local installer helpers (CUDA, etc.)
├── config/pler.yaml            # Documented defaults (not all wired into CLI yet)
├── docs/
│   ├── USER_GUIDE.md
│   └── DEVELOPER_GUIDE.md
├── models/                     # Sample meshes
├── pler_metric.py              # Thin subprocess wrapper → pler.exe
├── pler_advanced.py            # Wrapper with --tsi
└── README.md                   # Short entry point
```

Legacy Python metric implementations in other folders (`C:\pler_project`, etc.) are **not** the scoring engine. Do not reintroduce their outward-from-centroid cast or per-mesh independent normalization into `pler_core`.

---

## Measurement contract (must not silently drift)

Implementation must preserve this pipeline:

```text
load meshes (fail hard)
  → optional --align-volume (rigid transform on TEST only)
  → shared sphere from REFERENCE (one center, one scale; both inside R)
  → Fibonacci unit directions n_i
  → rays: origin = R·n_i , direction = −n_i
  → L = t_hit ; miss ⇒ L = R
  → peak = R − min(L_ref)
  → MSE = mean( (L_ref − L_test)² )
  → PLER_dB = 10·log10( peak² / MSE )   (cap 100 dB if MSE ≲ 1e−10)
  → optional TSI / optional weighted mix
```

| Rule | Rationale |
|------|-----------|
| Inward rays from the sphere | Matches PLER papers (segment from sphere to first hit) |
| Shared frame from reference | Relative size/shift remain observable unless align is on |
| Miss = R | Empty ray is a real geometric disagreement, not an invented depth |
| No fallback unit sphere on load failure | Corrupt files must not score as perfect spheres |
| Align volume is opt-in | Displacement is often intentional signal, not always “error” |

### Formula

\[
\mathrm{PLER} = 10 \log_{10} \frac{(R - L_{\min})^2}{\mathrm{MSE}}
\]

Fibonacci lattice (equal-area \(z\), golden azimuth) — see `fibonacci_directions()` in `frame.cpp`.

---

## Build

### Requirements

| Component | Version / notes |
|-----------|-----------------|
| CMake | ≥ 3.20 |
| C++ | 17 |
| Compiler | MSVC 2017+ (validated), or clang/gcc |
| Optional CUDA | Toolkit with `nvcc` (validated: **12.2** on GTX 1650 Ti / **sm_75**) |
| Optional Embree | If found, linked; otherwise built-in BVH |
| GUI deps | GLFW + Dear ImGui via FetchContent when `PLER_BUILD_GUI=ON` |

### Configure and build (Windows / VS 2017 example)

```bat
cd native
build.bat
```

Or manually:

```bat
cd native
cmake -B build -G "Visual Studio 15 2017" -A x64 -DPLER_CUDA=ON -DPLER_BUILD_GUI=ON
cmake --build build --config Release
```

Outputs:

```text
build/pler_cli/Release/pler.exe
build/pler_gui/Release/pler_gui.exe
build/pler_core/Release/pler_core.lib
build/pler_cuda/Release/pler_cuda.lib   # if CUDA enabled
```

### CMake options

| Option | Default | Meaning |
|--------|---------|---------|
| `PLER_CUDA` | ON | Compile CUDA path if `nvcc` is found |
| `PLER_BUILD_GUI` | ON | Build `pler_gui` |

CUDA integration uses a **custom `nvcc` command** (does not require Visual Studio CUDA toolset). Architecture default: **sm_75** (`pler_cuda/CMakeLists.txt`). Change `PLER_CUDA_ARCH` there for other GPUs.

Without `nvcc`, configure continues as **CPU-only**.

### Environment

For GPU runs after install:

```bat
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2
set PATH=%CUDA_PATH%\bin;%PATH%
```

---

## Module map (`pler_core`)

| Header / sources | Responsibility |
|------------------|----------------|
| `types.hpp` | `Mesh`, `Vec3`, `Transform`, `PlerOptions`, `PlerResult` |
| `mesh_io.*` | OBJ (tinyobj) / STL load; fan triangulation; validate; throw `PlerError` |
| `frame.*` | Shared sphere normalize; Fibonacci dirs; inward `RayBatch` |
| `align_volume.*` | Occupancy voxelization; coarse-to-fine rigid IoU search; apply to test |
| `bvh.*` / `raycast_cpu.cpp` | Shared BVH build + CPU cast; CUDA BVH upload + batched rays |
| `pler_cuda` / `raycast.cu` | GPU BVH traversal; accel upload reused across ray batches |
| `score.*` | `hits_to_lengths` (miss→R); peak/MSE/PLER |
| `topology.*` | Face-adjacency components; boundary loops; Jaccard-style TSI |
| `pler.*` | End-to-end `compute_pler`; adaptive rays; converge; optional mix |
| `job.*` | `JobRunner` for GUI: single, batch, research (models × ray list) |

Public API surface: `native/pler_core/include/pler/pler.hpp`.

### Key API

```cpp
pler::PlerOptions opt;
opt.num_rays = 5000;          // <= 0 → adaptive via suggest_rays()
opt.align_volume = false;
opt.compute_tsi = true;
opt.prefer_cuda = true;

pler::PlerResult r = pler::compute_pler(ref_path, test_path, opt, progress, &cancel);
```

`JobRunner::start` / `start_batch` / `start_research` run on detached worker threads; UI must only `snapshot()`.

---

## CLI design

Entry: `pler_cli/main.cpp`.

| Command | Behavior |
|---------|----------|
| `pler ref test [flags]` | Single pair |
| `pler batch ref pattern` | Glob `*` in filename under parent dir |
| `pler selftest` | Synthetic sphere; CPU↔CUDA max \|ΔL\| check |
| `pler gui` | Launches sibling `pler_gui.exe` (Windows) |

Flags: `--rays`, `--align-volume`, `--tsi`, `--converge`, `--cpu`, `--w1`/`--w2`.

Exit codes: `0` success, non-zero on load/cast/selftest failure.

---

## GUI design

Entry: `pler_gui/main.cpp` (Dear ImGui + GLFW).

| Principle | Implementation |
|-----------|----------------|
| Non-blocking | Metric on worker via `JobRunner` |
| Low UI cost | ~30 FPS sleep when idle; no mesh upload to GL during cast |
| Modes | Tabs: Single / Research / Settings |
| Actions | Full-width **RUN** / **RUN BATCH**, Cancel, Browse dialogs |

Do not perform ray casting or heavy I/O on the UI thread.

---

## Backends and accuracy

| Backend | When used | Notes |
|---------|-----------|--------|
| CPU BVH | Always available; `--cpu` or no CUDA | Reference for agreement tests |
| CUDA BVH | `prefer_cuda` and `cuda_available()` | Float32 BVH; compare via `pler selftest` |

Selftest tolerance: max \|L_cpu − L_cuda\| should stay very small (order 1e−7 observed on GTX 1650 Ti). If disagreement grows after kernel changes, fix before merging.

VRAM: CUDA path estimates footprint and aborts to CPU path on allocation failure / insufficient free memory (see `raycast.cu` / host fallback in `cast_rays`).

---

## Adaptive rays and convergence

- `num_rays <= 0`: `suggest_rays(V, T, min_rays, max_rays)` ≈ `min_rays * log1p(V/1000 + T/2000)`, clipped.
- `--converge` / GUI Converge: double \(N\) until relative PLER change &lt; `converge_rel` (default 0.05), capped by `max_rays`.

Defaults: `min_rays=1000`, `max_rays=20000`, fixed default \(N=5000\).

---

## Topology (TSI)

`compute_topology`:

- Connected components on **face adjacency**
- Boundary loops from edges with face-count 1
- Euler / genus estimate as auxiliary fields

`topological_similarity`: Jaccard-style over component and loop counts (plus light genus blend).

Weighted mix (`w_pler`, `w_tsi`) maps PLER/100 into \[0,1\] then blends — **only** when weights are explicitly set. Default reports keep PLER (dB) and TSI separate.

---

## Testing checklist

1. `pler selftest` → PASS; backend `cuda` when toolkit present.
2. Identical files → ~100 dB, miss rates ~0.
3. Known anisotropic distortion → clearly lower dB without align.
4. `--align-volume` on translated copy → IoU printed; score should recover toward shape-only case for rigid offset.
5. One-sided hole / open mesh → non-zero miss rate, lower PLER.
6. GUI: RUN responsive; Cancel between research items; CPU load dominated by worker, not UI.

Add golden meshes under `models/` for regression when changing cast or normalize.

---

## Integration

### CMake FetchContent (source tree)

```cmake
include(FetchContent)
FetchContent_Declare(
  pler
  GIT_REPOSITORY https://github.com/GlebVoronkov03/PLER.git
  GIT_TAG        v2.0.0
  SOURCE_SUBDIR  native
)
set(PLER_CUDA OFF CACHE BOOL "" FORCE)
set(PLER_BUILD_GUI OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pler)
# Link the shared C library (output name "pler") and/or pler_core
target_link_libraries(your_app PRIVATE pler_shared)
target_include_directories(your_app PRIVATE ${pler_SOURCE_DIR}/pler_c/include)
```

Alternatively clone this repository and `add_subdirectory(native)` from a parent project, or install and `find_package(pler)`.

### C ABI (shared library) — preferred for plugins

Public header: `native/pler_c/include/pler.h`. Build produces `pler.dll` / `libpler.so` (CMake target `pler_shared`, output name `pler`).

```c
#include "pler.h"
pler_options opt;
pler_options_init(&opt);
opt.num_rays = 2000;
pler_result out;
int rc = pler_compute_files("ref.obj", "test.obj", &opt, &out);
```

Example: `native/pler_c/examples/pler_c_example.c` (target `pler_c_example`).

Install / `find_package`:

```cmake
find_package(pler 2.0 REQUIRED)
target_link_libraries(your_app PRIVATE pler::pler)  # after install
```

In-tree: link `pler_shared` and include `pler_c/include`.

### Subprocess (scripts / Blender shell-out)

```bat
pler ref.obj test.obj --rays 5000 --tsi
pler --version
```

Parse stdout keys (`PLER:`, `MSE:`, …). Non-zero exit ⇒ failure.

### Link `pler_core` (C++ advanced)

```cmake
# via parent native/CMakeLists.txt
target_link_libraries(your_app PRIVATE pler_core)
```

Include path: `native/pler_core/include`. Prefer the C ABI for long-lived plugin boundaries.

### Python

`pler_metric.py` locates `pler.exe` and `subprocess.call`s it. Keep wrappers thin; do not reimplement scoring in Python.

**Align caveats:** leave align **off** for pre-registered LODs. Volume IoU can be extremely slow on dense meshes; ICP may degrade already-aligned pairs.
---

## Performance notes

| Cost center | Guidance |
|-------------|----------|
| BVH build + cast | Dominates CPU path; one scene per mesh per cast |
| CUDA BVH upload + stack traversal | Prefer this over triangle scan; OOM falls back to CPU BVH |
| Align volume | Voxel grid search is expensive; lower `voxel_resolution` for interactive use |
| Research mode | \(N_{\mathrm{models}} \times N_{\mathrm{ray\ lists}}\) full pipeline runs — expect linear growth |

Avoid: rebuilding Open3D-style scenes per ray chunk; reordering parallel chunk results (historical bug in old `pler_metric_crash.py`).

---

## Extending the metric safely

| Change | Do | Don’t |
|--------|----|--------|
| New sampler | Keep equal-area / deterministic for fair MSE | Mix random non-uniform dirs without redesigning MSE |
| New align mode | New flag, default off | Silent ICP that hides pose |
| Materials / color | Separate score / future PLERcolor | Fold into geometric PLER without paper update |
| Fallback geometry | Never on production score path | Unit sphere on load error |

---

## Known limitations

- Formats beyond OBJ/STL need Assimp (or preprocess).
- CUDA kernel walks a host-built BVH (same split/leaf policy as CPU); triangle scan removed.
- `config/pler.yaml` documents defaults; CLI/GUI currently use compiled/`PlerOptions` defaults — wire YAML if you need runtime config files.
- GUI file dialogs are Windows-oriented (`GetOpenFileName` / folder browser).

---

## Versioning and papers

Software version: **2.0.0** (`pler --version`). License: MIT.

| Artifact | Role |
|----------|------|
| Voronkov 2025 (DOI 10.1109/IEEECONF64229.2025.10948103) | Motivation; user assessments / XR |
| Voronkov 2026 PLER (DOI 10.1109/IEEECONF68869.2026.11461193) | Authoritative sampling + peak/MSE formula |
| This codebase | Software reference of PLER-2.0 + opt-in align + TSI + C ABI |

When citing behavior in publications, state ray count \(N\), align mode, backend (cpu/cuda), and mesh preprocessing. See root `CITATION.cff` / `README.md`.

**Packaging:** `native/package.bat` builds `dist/pler-2.0-win64.zip` with CLI, GUI, `pler.dll`, `pler.h`, samples, docs, and CUDA `cudart` DLLs when the Toolkit is present.
---

## Related docs

- [USER_GUIDE.md](USER_GUIDE.md) — operators and artists  
- [../README.md](../README.md) — short overview and command cheat sheet  
- [../config/pler.yaml](../config/pler.yaml) — default parameter names
