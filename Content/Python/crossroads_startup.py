"""Prepare Crossroads once per set of source files in interactive Unreal Editor."""

import hashlib
import os
from pathlib import Path
import runpy
import traceback

import unreal


REGION = "/Game/Maps/L_CrossroadsRelief"
READY_TAG = "CrossroadsRegionReady"


def open_level(path):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = editor.get_editor_world()
    return (world and world.get_path_name().split(".")[0] == path) or levels.load_level(path)


def region_is_ready():
    if not open_level(REGION):
        return False
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return any(actor.actor_has_tag(unreal.Name(READY_TAG)) for actor in actors.get_all_level_actors())


def prepare_if_needed():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    stamp = project / "Saved" / "CrossroadsPreparation.sha256"
    sources = list(path for path in (project / "Tools").iterdir() if path.suffix in (".py", ".json"))
    sources.extend(path for path in (project / "Source" / "SurvivalWorldEditor").rglob("*") if path.is_file())
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
    for pack in ("CitySampleBuildings", "OvergrownFactoryENV"):
        for path in sorted((project / "Content" / pack).rglob("*.uasset")):
            digest.update(path.relative_to(project).as_posix().encode("utf-8"))
            stat = path.stat()
            digest.update(f"{stat.st_size}:{stat.st_mtime_ns}".encode())
    revision = digest.hexdigest()

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    required = (
        REGION,
        "/Game/Art/Crossroads/World/SM_WarehouseDoorwayShell",
        "/Game/Art/Crossroads/World/SM_WeatheredShelf",
        "/Game/Art/Crossroads/World/SM_SchoolDesk",
        "/Game/Art/Crossroads/World/Props/ScavengeCrate/SM_ScavengeCrate",
        "/Game/Art/Crossroads/World/Props/FuelPumpSolid/SM_FuelPumpSolid",
        "/Game/Art/Crossroads/Player/SK_Survivor",
        "/Game/Art/Crossroads/SniperSurface/SK_SniperViewmodel",
        "/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper",
        "/Game/Art/Crossroads/SniperSurface/SniperHold",
        "/Game/Art/Crossroads/SniperSurface/SniperFire",
        "/Game/Art/Crossroads/SniperSurface/SniperReload",
        "/Game/Art/Crossroads/StationZombie/SK_StationZombie",
        "/Game/Art/Crossroads/AlignedSidearm/SK_SidearmViewmodel",
        "/Game/Art/Crossroads/Building/SM_CrossroadsSidearm",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelIdle",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelWalk",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelFire",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelReload",
        "/Game/Art/Crossroads/AlignedSidearm/ViewmodelReloadEmpty",
        "/Game/Art/Crossroads/Building/M_BuildPlan",
        "/Game/Art/Crossroads/Building/M_CrossroadsTimber",
    )
    required += tuple(
        f"/Game/Art/Crossroads/World/Landmarks/{name}/SM_{name}"
        for name in ("AircraftHangar", "CommunityPitch", "PitchEntranceSign", "RadiationSign",
                     "RunwayMarkings", "WeatheredBusStop",
                     "YardTransformer")
    )
    required += tuple(
        f"/Game/Art/Crossroads/World/Communications/{name}/SM_{name}"
        for name in ("RepeaterMast", "CheckpointWall", "TimberWire", "RoadSpikes")
    )
    required += tuple(f"/Game/Art/Crossroads/World/SurveyDetail/{name}/SM_{name}"
                      for name in ("AbandonedVan",))
    required += tuple(f"/Game/Art/Crossroads/World/SurveyAccess/{name}/SM_{name}"
                      for name in ("SkalkaTower", "LaitseStation", "SubstationHouse", "RuinedTerminal", "RuinedAirportShop"))
    required += tuple(
        f"/Game/Art/Crossroads/Inventory/T_Item_{name}"
        for name in ("Sidearm", "Food", "Water", "Bandage", "Timber", "Stone", "Cloth", "Rounds", "TimberDoor", "Rifle", "RifleRounds", "BuilderGuide", "ScavengerNotes", "DressingSchematic", "FieldDressing",
                     "RifleStock", "RifleReceiver", "RifleBarrel", "RifleOptic")
    )
    required += tuple(
        f"/Game/Art/Crossroads/Interface/{icon.stem}"
        for icon in (project / "SourceArt" / "Crossroads" / "Interface").glob("T_UI_*.png")
    )
    required += tuple(f"/Game/Art/Crossroads/World/Props/{name}/SM_{name}"
                      for name in ("OfficeWorkstation", "OfficeFiles", "OfficeDrawer", "OfficeCooler"))
    required += tuple(f"/Game/Art/Crossroads/World/M_RegionFinish_{name}"
                      for name in ("Concrete", "StationBrick", "ShopTile"))
    required += tuple(f"/Game/Art/Crossroads/World/Region/{name}/SM_{name}" for name in (
        "CornerShop", "SubstationSwitchgear", "TransmissionPylon", "TransmissionCable",
        "BurnedSedan", "BurnedCompact", "RustedTruck", "SixTwelveShop", "SixTwelveCanopy",
        "StoneFacade", "OfficeFacade", "Storefront",
    ))
    required += tuple(f"/Game/Art/Crossroads/World/Rail/{name}/SM_{name}"
                      for name in ("FreightTrack", "TramTrack", "SovietLocomotive", "CrossroadsTram"))
    required += tuple(f"/Game/Art/Crossroads/World/Parish/SM_{name}/SM_{name}"
                      for name in ("TimberLeaf", "GlazedLeaf", "ServiceLeaf", "RoofLadder", "ParishUpperChurch"))
    prop_materials = runpy.run_path(str(project / "Tools" / "world_prop_materials.py"))
    required += tuple(prop_materials["required_packages"](project / "SourceArt" / "Crossroads" / "World"))
    if medicine_sheet.is_file():
        required += ("/Game/Art/Crossroads/Local/MedicineSheet",)
    if (stamp.is_file() and stamp.read_text(encoding="utf-8") == revision
            and all(assets.does_asset_exist(path) for path in required) and region_is_ready()):
        return

    unreal.log("Crossroads: preparing the default region map and assets.")
    try:
        runpy.run_path(str(project / "Tools" / "prepare_crossroads.py"), run_name="__main__")
        if not region_is_ready():
            raise RuntimeError("The region did not finish preparing.")
        missing = [path for path in required if not assets.does_asset_exist(path)]
        if missing:
            raise RuntimeError("Missing prepared assets: " + ", ".join(missing))
    except Exception as error:
        unreal.log_error("Crossroads preparation failed.\n" + traceback.format_exc())
        unreal.EditorDialog.show_message("Crossroads preparation failed",
                                         f"{error}\n\nSee Saved/Logs/Survival.log for the traceback.",
                                         unreal.AppMsgType.OK)
        return
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(revision, encoding="utf-8")
    unreal.log("Crossroads: region ready. You can press Play.")


def schedule_preparation():
    # Packaging runs the same preparation itself before cooking.
    command_line = unreal.SystemLibrary.get_command_line()
    _, switches, params = unreal.SystemLibrary.parse_command_line(command_line)
    if (os.environ.get("SURVIVAL_PREPARED_MARKER")
            or any(key.lower() == "run" for key in params)
            or any(switch.lower() in {"game", "run"} for switch in switches)):
        return

    def prepare_after_editor_load(_delta_seconds):
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        if not editor.get_editor_world() or registry.is_loading_assets():
            return

        unreal.unregister_slate_post_tick_callback(handle)
        prepare_if_needed()

    handle = unreal.register_slate_post_tick_callback(prepare_after_editor_load)
