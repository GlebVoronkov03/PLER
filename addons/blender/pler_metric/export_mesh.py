"""Export selected mesh object(s) to a temporary OBJ for PLER."""
from __future__ import annotations

import tempfile
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

import bpy


def _blender_version() -> Tuple[int, int, int]:
    return bpy.app.version


def export_objects_to_obj(objects: Sequence[bpy.types.Object], path: str) -> None:
    """Export given mesh objects to OBJ at path (absolute)."""
    meshes = [o for o in objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError("No mesh objects to export")

    # Isolate selection
    view_layer = bpy.context.view_layer
    for obj in view_layer.objects:
        obj.select_set(False)
    for obj in meshes:
        obj.select_set(True)
    view_layer.objects.active = meshes[0]

    ver = _blender_version()
    abs_path = bpy.path.abspath(path)
    Path(abs_path).parent.mkdir(parents=True, exist_ok=True)

    if ver >= (4, 0, 0):
        # Blender 4.x built-in OBJ exporter
        bpy.ops.wm.obj_export(
            filepath=abs_path,
            export_selected_objects=True,
            export_materials=False,
            export_uv=False,
            export_normals=True,
            forward_axis="NEGATIVE_Z",
            up_axis="Y",
        )
    else:
        bpy.ops.export_scene.obj(
            filepath=abs_path,
            use_selection=True,
            use_materials=False,
            use_uvs=False,
            use_normals=True,
            axis_forward="-Z",
            axis_up="Y",
        )


def export_selection_pair(
    ref_name: str,
    test_name: str,
    temp_dir: Optional[str] = None,
) -> Tuple[str, str]:
    """Export two named objects to temp OBJs. Returns (ref_path, test_path)."""
    ref_obj = bpy.data.objects.get(ref_name)
    test_obj = bpy.data.objects.get(test_name)
    if ref_obj is None or test_obj is None:
        raise RuntimeError("Reference or test object not found")
    if ref_obj.type != "MESH" or test_obj.type != "MESH":
        raise RuntimeError("Reference and test must be mesh objects")

    base = Path(temp_dir or tempfile.mkdtemp(prefix="pler_export_"))
    base.mkdir(parents=True, exist_ok=True)
    ref_path = str(base / "ref.obj")
    test_path = str(base / "test.obj")
    export_objects_to_obj([ref_obj], ref_path)
    export_objects_to_obj([test_obj], test_path)
    return ref_path, test_path


def export_active_as(
    role: str,
    temp_dir: Optional[str] = None,
) -> str:
    """Export active mesh as ref.obj or test.obj under temp_dir."""
    obj = bpy.context.view_layer.objects.active
    if obj is None or obj.type != "MESH":
        raise RuntimeError("Active object must be a mesh")
    base = Path(temp_dir or tempfile.mkdtemp(prefix="pler_export_"))
    base.mkdir(parents=True, exist_ok=True)
    path = str(base / f"{role}.obj")
    export_objects_to_obj([obj], path)
    return path


def list_mesh_object_names() -> List[str]:
    return [o.name for o in bpy.context.scene.objects if o.type == "MESH"]
