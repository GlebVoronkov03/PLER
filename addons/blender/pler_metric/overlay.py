"""Build a simple error-sphere overlay from ray_depths_last.csv."""
from __future__ import annotations

import csv
from pathlib import Path
from typing import List, Optional, Tuple

import bpy
from mathutils import Vector

OVERLAY_NAME = "PLER_ErrorOverlay"


def clear_overlay() -> None:
    obj = bpy.data.objects.get(OVERLAY_NAME)
    if obj is None:
        return
    mesh = obj.data
    bpy.data.objects.remove(obj, do_unlink=True)
    if mesh is not None and mesh.users == 0:
        bpy.data.meshes.remove(mesh)


def _read_dump(path: Path) -> List[Tuple[Vector, float]]:
    rows: List[Tuple[Vector, float]] = []
    with path.open(newline="") as f:
        for row in csv.DictReader(f):
            try:
                d = Vector((float(row["dir_x"]), float(row["dir_y"]), float(row["dir_z"])))
                err = float(row["abs_err"])
            except (KeyError, ValueError):
                continue
            if d.length < 1e-8:
                continue
            # Dump dirs are inward; place samples on outward sphere
            rows.append(((-d).normalized(), err))
    return rows


def build_overlay_from_csv(csv_path: str, radius: float = 1.0) -> Optional[bpy.types.Object]:
    path = Path(csv_path)
    if not path.is_file():
        return None
    samples = _read_dump(path)
    if not samples:
        return None

    clear_overlay()

    emax = max((e for _, e in samples), default=1.0) or 1.0

    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=radius, location=(0.0, 0.0, 0.0))
    ico = bpy.context.active_object
    ico.name = OVERLAY_NAME
    mesh = ico.data

    if hasattr(mesh, "color_attributes"):
        attr = mesh.color_attributes.new(name="PLER_Error", type="FLOAT_COLOR", domain="POINT")
        dirs = [d for d, _ in samples]
        errs = [e for _, e in samples]
        for i, vert in enumerate(mesh.vertices):
            vd = vert.co.normalized()
            best_j = 0
            best_dot = -2.0
            for j, sd in enumerate(dirs):
                dot = vd.dot(sd)
                if dot > best_dot:
                    best_dot = dot
                    best_j = j
            t = min(1.0, max(0.0, errs[best_j] / emax))
            attr.data[i].color = (t, 0.15, 1.0 - t, 1.0)

    mat = bpy.data.materials.get("PLER_ErrorMat")
    if mat is None:
        mat = bpy.data.materials.new("PLER_ErrorMat")
        mat.use_nodes = True
        nt = mat.node_tree
        nt.nodes.clear()
        out_n = nt.nodes.new("ShaderNodeOutputMaterial")
        em = nt.nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (1.0, 0.4, 0.15, 1.0)
        em.inputs["Strength"].default_value = 1.5
        nt.links.new(em.outputs["Emission"], out_n.inputs["Surface"])
    if ico.data.materials:
        ico.data.materials[0] = mat
    else:
        ico.data.materials.append(mat)

    screen = getattr(bpy.context, "screen", None)
    if screen is not None:
        for area in screen.areas:
            if area.type != "VIEW_3D":
                continue
            for space in area.spaces:
                if space.type != "VIEW_3D":
                    continue
                try:
                    space.shading.type = "SOLID"
                    space.shading.color_type = "VERTEX"
                except Exception:
                    pass

    return ico


def register():
    pass


def unregister():
    clear_overlay()
