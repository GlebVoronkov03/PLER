bl_info = {
    "name": "PLER Mesh Quality",
    "author": "Gleb Alekseevich Voronkov",
    "version": (2, 0, 0),
    "blender": (3, 6, 0),
    "location": "View3D > Sidebar > PLER",
    "description": "Full-reference geometric fidelity (Peak Length to Error Ratio) via bundled pler CLI",
    "category": "Mesh",
    "doc_url": "https://github.com/GlebVoronkov03/PLER",
}

from . import prefs
from . import ops_compare
from . import ops_batch
from . import overlay
from . import ui_panel


def register():
    prefs.register()
    ops_compare.register()
    ops_batch.register()
    overlay.register()
    ui_panel.register()


def unregister():
    ui_panel.unregister()
    overlay.unregister()
    ops_batch.unregister()
    ops_compare.unregister()
    prefs.unregister()


if __name__ == "__main__":
    register()
