# PLER 2.0

Full-reference geometric fidelity metric for 3D assets (**Peak Length to Error Ratio**).

**License:** [MIT](LICENSE) · **Author:** Gleb Alekseevich Voronkov · **Contact:** glebvoronkov03@gmail.com

Native C++ engine · C ABI (`pler.dll` / `libpler.so`) · CLI · Dear ImGui GUI · optional CUDA (CPU is the reference path).

## Download for Windows

Get the latest binaries from **[Releases](https://github.com/GlebVoronkov03/PLER/releases)** (`pler-2.0-win64.zip`).

```bat
pler --version
pler selftest
pler samples\ref_unit_sphere.obj samples\test_unit_sphere_lod.obj --rays 2000
pler gui
```

The Windows zip may include the CUDA runtime. If no NVIDIA GPU / CUDA DLL is available, PLER falls back to the CPU BVH automatically. User guide: [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Build / embed

```bat
cd native
build.bat
```

Requires CMake ≥ 3.20 and a C++17 compiler. CUDA Toolkit with `nvcc` enables the GPU path; without it the build is CPU-only.

**C ABI** (shared library):

```c
#include "pler.h"
pler_options opt;
pler_options_init(&opt);
opt.num_rays = 2000;
pler_result out;
if (pler_compute_files("samples/ref_unit_sphere.obj",
                       "samples/test_unit_sphere_lod.obj", &opt, &out) == 0)
  printf("PLER = %.3f dB\n", out.pler_db);
```

**CMake FetchContent** (in-tree): see [Developer Guide — Integration](docs/DEVELOPER_GUIDE.md#integration). Advanced C++ API: `native/pler_core/include/pler/`.

| Audience | Document |
|----------|----------|
| Users | [docs/USER_GUIDE.md](docs/USER_GUIDE.md) |
| Developers | [docs/DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) |
| Contributing | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Security | [SECURITY.md](SECURITY.md) |

## Score (PLER-2.0)

Shared sphere · inward Fibonacci rays · miss = R ·  
\(\mathrm{PLER} = 10\log_{10}((R-L_{\min})^2/\mathrm{MSE})\) (capped at 100 dB).

Optional alignment (`--align`) and topology (`--tsi`). Prefer **align off** when meshes are already co-registered.

## Citation

```bibtex
@inproceedings{voronkov2026pler,
  title     = {PLER: A Novel Full-Reference Metric for Evaluating the Geometric Fidelity of 3D Assets},
  author    = {Voronkov, Gleb Alekseevich},
  booktitle = {2026 Systems of Signals Generating and Processing in the Field of on Board Communications},
  year      = {2026},
  doi       = {10.1109/IEEECONF68869.2026.11461193}
}

@inproceedings{voronkov2025xr,
  title     = {Research of Methods for Modeling User Assessments of 3D Content for Extended Reality Systems},
  author    = {Voronkov, Gleb Alekseevich},
  booktitle = {2025 Systems of Signals Generating and Processing in the Field of on Board Communications},
  year      = {2025},
  doi       = {10.1109/IEEECONF64229.2025.10948103}
}
```

See also [CITATION.cff](CITATION.cff).
