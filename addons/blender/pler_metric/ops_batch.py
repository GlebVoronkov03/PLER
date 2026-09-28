import csv
from pathlib import Path

import bpy
from bpy.types import Operator

from . import export_mesh
from . import parse_output
from . import runner
from .ops_compare import _prefs_override


class PLER_OT_batch(Operator):
    bl_idname = "pler.batch"
    bl_label = "Batch folder"
    bl_options = {"REGISTER"}

    def execute(self, context):
        s = context.scene.pler
        folder = bpy.path.abspath(s.batch_folder)
        if not folder or not Path(folder).is_dir():
            self.report({"ERROR"}, "Set a valid batch folder")
            return {"CANCELLED"}

        try:
            if s.source == "FILES":
                ref = bpy.path.abspath(s.ref_path)
                if not ref:
                    self.report({"ERROR"}, "Set reference file for batch")
                    return {"CANCELLED"}
            else:
                if not s.ref_object:
                    self.report({"ERROR"}, "Set reference object for batch")
                    return {"CANCELLED"}
                ref_obj = bpy.data.objects.get(s.ref_object)
                if ref_obj is None or ref_obj.type != "MESH":
                    self.report({"ERROR"}, "Reference object missing or not a mesh")
                    return {"CANCELLED"}
                import tempfile

                tmp = tempfile.mkdtemp(prefix="pler_batch_ref_")
                ref = str(Path(tmp) / "ref.obj")
                export_mesh.export_objects_to_obj([ref_obj], ref)

            exts = {".obj", ".stl"}
            tests = sorted(
                p for p in Path(folder).iterdir() if p.suffix.lower() in exts and p.is_file()
            )
            if not tests:
                self.report({"ERROR"}, "No .obj/.stl files in batch folder")
                return {"CANCELLED"}

            out_csv = (
                bpy.path.abspath(s.batch_csv)
                if s.batch_csv
                else str(Path(folder) / "pler_batch.csv")
            )
            Path(out_csv).parent.mkdir(parents=True, exist_ok=True)

            rows = []
            for test in tests:
                result = runner.run_pler(
                    ref,
                    str(test),
                    rays=s.rays,
                    align=s.align,
                    tsi=s.compute_tsi,
                    force_cpu=s.force_cpu,
                    dump_rays=False,
                    binary_override=_prefs_override(),
                )
                parsed = parse_output.parse_pler_stdout(result.stdout)
                rows.append(
                    {
                        "name": test.name,
                        "pler_db": parse_output.pler_db(parsed) if result.ok else "",
                        "mse": parse_output.mse(parsed) if result.ok else "",
                        "tsi": parse_output.tsi(parsed) if result.ok else "",
                        "backend": parsed.get("Backend", ""),
                        "ok": int(result.ok),
                        "error": "" if result.ok else (result.stderr or result.stdout)[:120],
                    }
                )

            with open(out_csv, "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(
                    f,
                    fieldnames=["name", "pler_db", "mse", "tsi", "backend", "ok", "error"],
                )
                w.writeheader()
                w.writerows(rows)

            s.last_status = f"Batch wrote {len(rows)} rows → {out_csv}"
            self.report({"INFO"}, s.last_status)
            return {"FINISHED"}
        except Exception as ex:
            s.last_status = str(ex)
            self.report({"ERROR"}, str(ex))
            return {"CANCELLED"}


CLASSES = (PLER_OT_batch,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
