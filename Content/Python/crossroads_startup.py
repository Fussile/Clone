"""Prepare Crossroads once per set of source files in interactive Unreal Editor."""

import hashlib
import os
from pathlib import Path
import runpy

import unreal


def prepare_if_needed():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    stamp = project / "Saved" / "CrossroadsPreparation.sha256"
    sources = list((project / "Tools").glob("*.py"))
    sources.extend(path for path in (project / "SourceArt" / "Crossroads").rglob("*") if path.is_file())
    digest = hashlib.sha256()
    digest.update(Path(__file__).read_bytes())
    for path in sorted(sources):
        digest.update(path.relative_to(project).as_posix().encode("utf-8"))
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    revision = digest.hexdigest()

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    required = (
        "/Game/Maps/L_SouthernCrossroads",
        "/Game/Art/Crossroads/Player/SK_Survivor",
        "/Game/Art/Crossroads/StationZombie/SK_StationZombie",
        "/Game/Art/Crossroads/ArmedViewmodel/SK_SidearmViewmodel",
        "/Game/Art/Crossroads/Building/M_BuildPlan",
    )
    if (stamp.is_file() and stamp.read_text(encoding="utf-8") == revision
            and all(assets.does_asset_exist(path) for path in required)):
        return

    unreal.log("Crossroads: preparing the editor map and assets.")
    runpy.run_path(str(project / "Tools" / "prepare_crossroads.py"), run_name="__main__")
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(revision, encoding="utf-8")


def schedule_preparation():
    # Packaging runs the same preparation itself before cooking.
    command_line = unreal.SystemLibrary.get_command_line()
    _, switches, params = unreal.SystemLibrary.parse_command_line(command_line)
    if (os.environ.get("SURVIVAL_PREPARED_MARKER")
            or any(key.lower() == "run" for key in params)
            or any(switch.lower() == "game" for switch in switches)):
        return

    def prepare_after_editor_load(_delta_seconds):
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        if not editor.get_editor_world() or registry.is_loading_assets():
            return

        unreal.unregister_slate_post_tick_callback(handle)
        prepare_if_needed()

    handle = unreal.register_slate_post_tick_callback(prepare_after_editor_load)
