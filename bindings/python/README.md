# pler-metric (Python)

PyPI package wrapping the PLER **C ABI** (`libpler` / `pler.dll`) plus a `pler` console script that launches the bundled native CLI.

```bash
pip install pler-metric
```

```python
from pler_metric import options_init, compute_files, AlignMode

opt = options_init()
opt.num_rays = 2000
opt.align_mode = AlignMode.OFF
opt.prefer_cuda = True  # falls back to CPU if no GPU

r = compute_files("ref.obj", "test.obj", opt)
print(r.pler_db, r.mse, r.backend, r.ok)
```

```bash
pler --version
pler selftest
pler ref.obj test.obj --rays 2000
```

Platform wheels embed natives from [GitHub Release v2.0.0](https://github.com/GlebVoronkov03/PLER/releases/tag/v2.0.0) (CUDA-capable Win/Linux zips preferred when present).

Build a local wheel:

```powershell
powershell -File bindings/python/build_wheels.ps1
```
