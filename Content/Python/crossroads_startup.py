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
    medicine_sheet = project / "SourceArt" / "Local" / "MedicineSheet.png"
    if medicine_sheet.is_file():
        sources.append(medicine_sheet)
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
        "/Game/Art/Crossroads/SniperSurface/SK_SniperViewmodel",
        "/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper",
        "/Game/Art/Crossroads/SniperSurface/SniperHold",
        "/Game/Art/Crossroads/SniperSurface/SniperFire",
        "/Game/Art/Crossroads/SniperSurface/SniperReload",
        "/Game/Art/Crossroads/StationZombie/SK_StationZombie",
        "/Game/Art/Crossroads/AlignedSidearm/SK_SidearmViewmodel",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelReload",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelReloadEmpty",
        "/Game/Art/Crossroads/Building/M_BuildPlan",
        "/Game/Art/Crossroads/Building/M_CrossroadsTimber",
    )
    required += tuple(
        f"/Game/Art/Crossroads/Inventory/T_Item_{name}"
        for name in ("Sidearm", "Food", "Water", "Bandage", "Timber", "Stone", "Cloth", "Rounds", "TimberDoor", "Rifle", "RifleRounds", "BuilderGuide", "ScavengerNotes", "DressingSchematic", "FieldDressing",
                     "RifleStock", "RifleReceiver", "RifleBarrel", "RifleOptic")
    )
    required += tuple(
        f"/Game/Art/Crossroads/Interface/{icon.stem}"
        for icon in (project / "SourceArt" / "Crossroads" / "Interface").glob("T_UI_*.png")
    )
    if medicine_sheet.is_file():
        required += ("/Game/Art/Crossroads/Local/MedicineSheet",)
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
