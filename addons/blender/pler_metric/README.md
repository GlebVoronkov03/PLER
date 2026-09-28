# PLER Mesh Quality (Blender)

Blender **3.6+ / 4.x** addon that shells out to a **bundled** `pler` CLI for full-reference geometric fidelity scoring.

## Install

1. Download [`pler-blender-2.0.zip`](https://github.com/GlebVoronkov03/PLER/releases/tag/v2.0.0) from the PLER Release (includes platform binaries).
2. Blender → Edit → Preferences → Add-ons → Install… → select the zip.
3. Enable **PLER Mesh Quality**.
4. Open the 3D View sidebar → **PLER** tab.
<img width="1919" height="1023" alt="image" src="https://github.com/user-attachments/assets/74da0c2c-08dc-43d1-b837-d85e0e5f7ce7" />
<img width="314" height="650" alt="image" src="https://github.com/user-attachments/assets/9869c9cd-1e28-4e05-8437-902260815c68" />


## Features

- Compare from **files** or **selected mesh objects** (exports temp OBJ)
- Rays, align (`off` / `iou` / `icp` / `iou+icp`), optional TSI
- **Force CPU** toggle (`--cpu`)
- **Batch folder** of `.obj` / `.stl` → CSV report
- **Error overlay** icosphere colored from `--dump-rays` (`ray_depths_last.csv`)

## Preferences

- Optional absolute path override for `pler` / `pler.exe` if you do not use the bundled `bin/` tree.

## Platforms

Bundled under `pler_metric/bin/`:

| Folder | Binary |
|--------|--------|
| `windows-x64` | `pler.exe` |
| `linux-x64` | `pler` |
| `macos-arm64` | `pler` |
| `macos-x64` | `pler` |

macOS builds are unsigned; clear quarantine on the binary if Gatekeeper blocks execution.

## Extensions store

[`blender_manifest.toml`](blender_manifest.toml) is ready for a later Blender Extensions submission. Until then, use the GitHub Release zip.

## License

MIT — same as [PLER](https://github.com/GlebVoronkov03/PLER).
