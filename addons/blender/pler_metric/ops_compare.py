import bpy
from bpy.props import BoolProperty, EnumProperty, IntProperty, StringProperty
from bpy.types import Operator, PropertyGroup

from . import export_mesh
from . import overlay
from . import parse_output
from . import runner


class PLER_SceneSettings(PropertyGroup):
    source: EnumProperty(
        name="Source",
        items=[
            ("FILES", "Files", "Compare mesh files on disk"),
            ("SELECTED", "Selected", "Export selected mesh objects to temp OBJ"),
        ],
        default="FILES",
    )
    ref_path: StringProperty(name="Reference", subtype="FILE_PATH", default="")
    test_path: StringProperty(name="Test", subtype="FILE_PATH", default="")
    ref_object: StringProperty(name="Reference object", default="")
    test_object: StringProperty(name="Test object", default="")
    rays: IntProperty(name="Rays", default=2000, min=100, max=100000)
    align: EnumProperty(
        name="Align",
        items=[
            ("off", "Off", "No alignment"),
            ("iou", "IoU", "Volume IoU"),
            ("icp", "ICP", "ICP"),
            ("iou+icp", "IoU+ICP", "IoU then ICP"),
        ],
        default="off",
    )
    compute_tsi: BoolProperty(name="TSI", default=False)
    force_cpu: BoolProperty(name="Force CPU", default=False)
    show_overlay: BoolProperty(name="Show error overlay", default=True)
    batch_folder: StringProperty(name="Batch folder", subtype="DIR_PATH", default="")
    batch_csv: StringProperty(name="Batch CSV", subtype="FILE_PATH", default="//pler_batch.csv")

    last_pler: StringProperty(name="Last PLER", default="")
    last_mse: StringProperty(name="Last MSE", default="")
    last_tsi: StringProperty(name="Last TSI", default="")
    last_backend: StringProperty(name="Last backend", default="")
    last_status: StringProperty(name="Last status", default="")


def _prefs_override() -> str:
    addon = bpy.context.preferences.addons.get(__package__)
    if addon is None:
        return ""
    return addon.preferences.binary_override


class PLER_OT_compare(Operator):
    bl_idname = "pler.compare"
    bl_label = "Run PLER"
    bl_options = {"REGISTER"}

    def execute(self, context):
        s = context.scene.pler
        try:
            if s.source == "FILES":
                ref = bpy.path.abspath(s.ref_path)
                test = bpy.path.abspath(s.test_path)
                if not ref or not test:
                    self.report({"ERROR"}, "Set reference and test file paths")
                    return {"CANCELLED"}
            else:
                if not s.ref_object or not s.test_object:
                    self.report({"ERROR"}, "Set reference and test object names")
                    return {"CANCELLED"}
                ref, test = export_mesh.export_selection_pair(s.ref_object, s.test_object)

            result = runner.run_pler(
                ref,
                test,
                rays=s.rays,
                align=s.align,
                tsi=s.compute_tsi,
                force_cpu=s.force_cpu,
                dump_rays=True,
                binary_override=_prefs_override(),
            )
            parsed = parse_output.parse_pler_stdout(result.stdout)
            if not result.ok:
                s.last_status = (result.stderr or result.stdout or "PLER failed")[:200]
                self.report({"ERROR"}, s.last_status)
                return {"CANCELLED"}

            pdb = parse_output.pler_db(parsed)
            mse = parse_output.mse(parsed)
            tsi_v = parse_output.tsi(parsed)
            s.last_pler = f"{pdb:.4f} dB" if pdb is not None else parsed.get("PLER", "")
            s.last_mse = f"{mse:.6g}" if mse is not None else parsed.get("MSE", "")
            s.last_tsi = f"{tsi_v:.4f}" if tsi_v is not None else parsed.get("TSI", "")
            s.last_backend = parsed.get("Backend", "")
            s.last_status = "OK"

            if s.show_overlay:
                dump = runner.ray_dump_path(result.cache_dir)
                if dump.is_file():
                    overlay.build_overlay_from_csv(str(dump), radius=1.0)
                else:
                    self.report({"WARNING"}, "Ray dump not found; overlay skipped")

            self.report({"INFO"}, f"PLER {s.last_pler}")
            return {"FINISHED"}
        except Exception as ex:
            s.last_status = str(ex)
            self.report({"ERROR"}, str(ex))
            return {"CANCELLED"}


class PLER_OT_clear_overlay(Operator):
    bl_idname = "pler.clear_overlay"
    bl_label = "Clear overlay"
    bl_options = {"REGISTER"}

    def execute(self, context):
        overlay.clear_overlay()
        return {"FINISHED"}


class PLER_OT_set_ref_from_active(Operator):
    bl_idname = "pler.set_ref_from_active"
    bl_label = "Ref = Active"
    bl_options = {"REGISTER"}

    def execute(self, context):
        obj = context.view_layer.objects.active
        if obj is None or obj.type != "MESH":
            self.report({"ERROR"}, "Active object must be a mesh")
            return {"CANCELLED"}
        context.scene.pler.ref_object = obj.name
        return {"FINISHED"}


class PLER_OT_set_test_from_active(Operator):
    bl_idname = "pler.set_test_from_active"
    bl_label = "Test = Active"
    bl_options = {"REGISTER"}

    def execute(self, context):
        obj = context.view_layer.objects.active
        if obj is None or obj.type != "MESH":
            self.report({"ERROR"}, "Active object must be a mesh")
            return {"CANCELLED"}
        context.scene.pler.test_object = obj.name
        return {"FINISHED"}


CLASSES = (
    PLER_SceneSettings,
    PLER_OT_compare,
    PLER_OT_clear_overlay,
    PLER_OT_set_ref_from_active,
    PLER_OT_set_test_from_active,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.pler = bpy.props.PointerProperty(type=PLER_SceneSettings)
    # Sync Force CPU default from prefs once
    try:
        addon = bpy.context.preferences.addons.get(__package__)
        if addon is not None:
            # Can't set scene default easily here; prefs used as initial in panel draw once
            pass
    except Exception:
        pass


def unregister():
    del bpy.types.Scene.pler
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
