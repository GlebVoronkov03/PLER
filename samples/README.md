# Synthetic PLER samples (MIT)

Tiny UV-sphere meshes for quick-start and packaging. Not perceptual benchmarks.

| File | Role |
|------|------|
| `ref_unit_sphere.obj` | Reference (~312 verts) |
| `test_unit_sphere_lod.obj` | Coarser LOD test (~84 verts) |

```bat
pler samples\ref_unit_sphere.obj samples\test_unit_sphere_lod.obj --rays 2000
```
