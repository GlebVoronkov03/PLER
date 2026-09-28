import bpy
from bpy.props import BoolProperty, StringProperty
from bpy.types import AddonPreferences


class PLER_AddonPreferences(AddonPreferences):
    bl_idname = __package__

    binary_override: StringProperty(
        name="PLER binary override",
        description="Optional absolute path to pler / pler.exe (empty = use bundled bin/)",
        subtype="FILE_PATH",
        default="",
    )
    force_cpu_default: BoolProperty(
        name="Force CPU by default",
        description="Default for the Force CPU panel toggle",
        default=False,
    )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "binary_override")
        layout.prop(self, "force_cpu_default")
        layout.label(text="Bundled binaries live under pler_metric/bin/<platform>/")


def register():
    bpy.utils.register_class(PLER_AddonPreferences)


def unregister():
    bpy.utils.unregister_class(PLER_AddonPreferences)
