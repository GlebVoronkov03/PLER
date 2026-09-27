# PLER 2.0 — User Guide

This guide is for people who want to **measure geometric quality of 3D models** with PLER. You do not need to build the project from source if you already have the programs.

PLER (Peak Length to Error Ratio) compares a **reference** model with a **test** model and reports a score in **decibels (dB)**. Higher is better. Identical geometry scores about **100 dB**.

---

## What you need


| Item                | Notes                              |
| ------------------- | ---------------------------------- |
| Windows 10/11 (x64) | Current release target             |
| `pler.exe`          | Console / headless (`pler --version`) |
| `pler_gui.exe`      | Optional windowed interface        |
| `pler.dll` + `pler.h` | Optional C library for integrators |
| Mesh files          | Prefer `samples\` for quick tests; **OBJ** / **STL** |


GPU (optional): CUDA uses a **GPU BVH** (same reliability as CPU BVH). Without a GPU or if CUDA runtime DLLs are missing, PLER falls back to the **CPU BVH**.

Typical locations after a local build:

```text
native\build\pler_cli\Release\pler.exe
native\build\pler_gui\Release\pler_gui.exe
native\build\pler_c\Release\pler.dll
```

Packaged zip (after `native\package.bat`):

```text
dist\pler-2.0\bin\pler.exe
dist\pler-2.0\samples\ref_unit_sphere.obj
```

Add the bin folder to PATH, or run with the full path.

---

## Headless / CI

Use the CLI only (no GUI window):

```bat
pler selftest
pler batch ref.obj models\*.obj --rays 5000
pler calibrate ref.obj test.obj
pler report
```

---

## Logs and exports

PLER writes run text to:

```text
.pler_cache\pler.log
```

Research also writes `research_last.csv` and a report folder under `.pler_cache\reports\` (CSV + SVG plots).

Optional: `--dump-rays` writes `ray_depths_last.csv`. Calibrate writes `calibrate_*.csv`.

From the GUI Research tab: **Export CSV**, **Open report folder**, **Calibrate rays**, and in-tab **Charts** (PLER by model, vs verts, time, rays/MSE, calibrate curve). SVG copies still go under `.pler_cache\reports\`.

---


Open a terminal in the folder that contains your models (or use full paths).

**Compare two models**

```bat
pler reference.obj test.obj
```

**More rays (finer sampling)**

```bat
pler reference.obj test.obj --rays 10000
```

**Alignment (opt-in)**

```bat
pler ref.obj test.obj --align iou
pler ref.obj test.obj --align icp
pler ref.obj test.obj --align iou+icp
```

**Open the graphical interface**

```bat
pler gui
```

---



## Reading the result

Example output:

```text
PLER:       18.85 dB
MSE:        0.00867
Peak:       0.816
Miss ref:   0 %
Miss test:  0 %
Rays:       5000
Backend:    cuda
Time:       0.32 s
```


| Field                    | Meaning                                                            |
| ------------------------ | ------------------------------------------------------------------ |
| **PLER**                 | Main quality score in dB. Higher = closer to the reference.        |
| **MSE**                  | Mean squared difference of ray lengths. Lower = better.            |
| **Peak**                 | Peak length used in the formula (from the reference).              |
| **Miss ref / Miss test** | Share of rays that hit no surface (treated as full miss distance). |
| **Rays**                 | How many sample rays were used.                                    |
| **Backend**              | `cuda` (GPU) or `cpu`.                                             |
| **Time**                 | How long the measurement took.                                     |




### Quality bands (rule of thumb)


| PLER (dB) | Interpretation                            |
| --------- | ----------------------------------------- |
| ≥ 60      | Excellent — differences are tiny          |
| ≥ 40      | Good — small visible or measurable change |
| ≥ 20      | Fair — clear geometric difference         |
| < 20      | Poor — large distortion                   |


Same model vs itself should be near **100 dB**.

---



## Console commands

```text
pler <reference> <test> [options]
pler batch <reference> <pattern> [options]
pler selftest
pler gui
pler --help
```



### Options


| Option           | What it does                                                                                                                                                                                                              |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `--rays N`       | Number of sample rays (default **5000**). More rays → more stable score, more time.                                                                                                                                       |
| `--align-volume` | Before scoring, rigidly moves/rotates the **test** model to maximize volume overlap with the reference. Use when you care about **shape**, not placement. **Off by default** so real displacement still lowers the score. |
| `--tsi`          | Also compute topological similarity (components / boundary loops).                                                                                                                                                        |
| `--converge`     | Automatically doubles the ray count until the score changes by less than about 5%.                                                                                                                                        |
| `--cpu`          | Force CPU even if a GPU build is available.                                                                                                                                                                               |
| `--w1 A --w2 B`  | Optional combined score from PLER and TSI (implies `--tsi`). Advanced; most users can ignore this.                                                                                                                        |




### Batch mode

Score every matching mesh in a folder against one reference:

```bat
pler batch reference.obj models\Sphere_*.obj --rays 2000
```

The pattern must contain `*` (for example `models\Cone_*.obj`).

### Self-test

Checks that the metric behaves correctly on a synthetic sphere (and compares CPU vs CUDA if a GPU is available):

```bat
pler selftest
```

You want: `SELFTEST PASS`.

---



## Graphical interface

Start:

```bat
pler gui
```

or double-click `pler_gui.exe`.

### Tab: Single

1. Choose **Reference** and **Test** (type a path or use **Browse**).
2. Set options:
  - **Auto rays** — pick ray count from mesh complexity, or set **Rays** yourself.
  - **Align volume (IoU)** — optional pose alignment (see above).
  - **TSI** — topology score.
  - **Converge** — auto-increase rays until stable.
  - **Prefer CUDA** — use GPU when available.
3. Click **RUN**.
4. Watch the progress bar; results appear below when finished.
5. Use **Cancel** if you need to stop.

If **Converge** is checked, the app may **double** the ray count until the score changes by less than about 5%. The result line **Rays** is the count actually used, which can be larger than the box.

### Tab: Research

For series of distorted models:

1. Set the **Reference**.
2. Choose a **folder** of test meshes (`.obj` / `.stl` in that folder).
3. Enter a **ray list**, for example `1000,5000,10000`.
4. Click **RUN BATCH**.
5. Read the results table (model, PLER, MSE, rays, verts, time).
6. Open **Charts** under the progress bar:
   - **PLER by model** — default LOD/ranking bar chart
   - **PLER vs verts** / **Time vs verts** — quality and cost vs complexity
   - **PLER vs rays** / **MSE vs rays** — useful when the ray list has several N
   - **Calibrate** — after **Calibrate rays**: PLER(N) and time(N) with suggested N marked

**Calibrate rays** sweeps log-spaced N on the reference + first folder mesh (or the Single test path), writes a CSV, and applies the suggested ray count to Single.


### Tab: Settings


| Setting          | Purpose                                             |
| ---------------- | --------------------------------------------------- |
| Min / Max rays   | Limits for auto rays and convergence                |
| Voxel resolution | Detail of volume alignment (higher = slower, finer) |


---



## When to use Align volume


| Situation                                                                    | Recommendation             |
| ---------------------------------------------------------------------------- | -------------------------- |
| You want to know if the **shape** matches, ignoring shift/rotation           | Turn **Align volume** on   |
| Position/orientation differences matter (they are real errors for your task) | Leave it **off** (default) |
| Models are already registered in the same pose                               | Leave it **off**           |


Align volume does **not** fix non-uniform scaling or strong warping — only rigid placement (rotation + translation).

---



## Tips for reliable scores

1. Use the **same units and orientation** for both files when possible.
2. Prefer **watertight** triangle meshes; open surfaces can raise miss rates.
3. Start with **5000** rays; use `--converge` or Research sweeps if you need stability checks.
4. Compare like with like: same export settings, same triangle density when studying compression/simplification.
5. Do not compare textured appearance — PLER measures **geometry only**.

---



## Supported formats


| Format                     | Support                                      |
| -------------------------- | -------------------------------------------- |
| `.obj`                     | Yes                                          |
| `.stl`                     | Yes (binary and ASCII)                       |
| `.ply`, `.gltf`, `.fbx`, … | Not in this build — convert to OBJ/STL first |


---



## Troubleshooting


| Problem                              | What to try                                                                                             |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------- |
| `File not found`                     | Check paths; use quotes if folders have spaces.                                                         |
| `Failed to load mesh`                | Re-export as triangulated OBJ/STL; avoid empty or corrupt files.                                        |
| Score seems wrong after a move       | Pose counts unless you use `--align-volume`.                                                            |
| `Backend: cpu` though you have a GPU | Build may be CPU-only, or use Prefer CUDA / omit `--cpu`. Driver and CUDA toolkit must match the build. |
| GUI does nothing on RUN              | Paths must point to existing `.obj`/`.stl` files.                                                       |
| Python `pler_metric.py` fails        | It only launches `pler.exe` — build or install the native binary first.                                 |


---



## Python helper (optional)

If you previously used Python scripts:

```bat
python pler_metric.py reference.obj test.obj 5000
```

This forwards to `pler.exe`. Prefer calling `pler` directly.

---



## Privacy and safety

- PLER runs **locally**. Models are not uploaded.
- Failed loads **stop with an error** (they are not silently replaced by a fake sphere).

---



## Further reading

- [Developer Guide](DEVELOPER_GUIDE.md) — build, architecture, extending the metric  
- Scientific definition of PLER-2.0: Fibonacci sphere sampling and  
\mathrm{PLER} = 10\log_{10}\frac{(R - L_{\min})^2}{\mathrm{MSE}}

