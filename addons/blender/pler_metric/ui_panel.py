import bpy
from bpy.types import Panel


class PLER_PT_main(Panel):
    bl_label = "PLER Mesh Quality"
    bl_idname = "PLER_PT_main"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "PLER"

    def draw(self, context):
        layout = self.layout
        s = context.scene.pler

        layout.prop(s, "source", expand=True)
        if s.source == "FILES":
            layout.prop(s, "ref_path")
            layout.prop(s, "test_path")
        else:
            row = layout.row(align=True)
            row.prop(s, "ref_object")
            row.operator("pler.set_ref_from_active", text="", icon="EYEDROPPER")
            row = layout.row(align=True)
            row.prop(s, "test_object")
            row.operator("pler.set_test_from_active", text="", icon="EYEDROPPER")

        layout.separator()
        layout.prop(s, "rays")
        layout.prop(s, "align")
        row = layout.row(align=True)
        row.prop(s, "compute_tsi")
        row.prop(s, "force_cpu")
        layout.prop(s, "show_overlay")

        layout.operator("pler.compare", icon="PLAY")
        layout.operator("pler.clear_overlay", icon="X")

        box = layout.box()
        box.label(text="Last result")
        box.label(text=f"PLER: {s.last_pler or '—'}")
        box.label(text=f"MSE:  {s.last_mse or '—'}")
        box.label(text=f"TSI:  {s.last_tsi or '—'}")
        box.label(text=f"Backend: {s.last_backend or '—'}")
        if s.last_status:
            box.label(text=s.last_status, icon="INFO")

        layout.separator()
        layout.label(text="Batch")
        layout.prop(s, "batch_folder")
        layout.prop(s, "batch_csv")
        layout.operator("pler.batch", icon="FILE_FOLDER")


CLASSES = (PLER_PT_main,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
