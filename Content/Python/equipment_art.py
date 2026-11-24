"""Import wearable meshes into their own packages for editor play and cooking."""
from pathlib import Path
import hashlib
import unreal


def prepare():
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for source in sorted((root / "SourceArt" / "Equipment").glob("SM_*.glb")):
        folder = "/Game/Art/Equipment/" + source.stem.removeprefix("SM_")
        path = folder + "/" + source.stem
        current = assets.load_asset(path) if assets.does_asset_exist(path) else None
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        if current and assets.get_metadata_tag(current, "GearSource") == digest:
            continue
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = folder
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        meshes = [obj for obj in task.get_objects() if isinstance(obj, unreal.StaticMesh)]
        if len(meshes) != 1:
            raise RuntimeError(f"Expected one wearable mesh from {source.name}")
        mesh = meshes[0]
        if mesh.get_path_name().split(".")[0] != path and not assets.rename_loaded_asset(mesh, path):
            raise RuntimeError(f"Could not name {source.name}")
        assets.set_metadata_tag(mesh, "GearSource", digest)
        if not assets.save_loaded_asset(mesh, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {path}")
