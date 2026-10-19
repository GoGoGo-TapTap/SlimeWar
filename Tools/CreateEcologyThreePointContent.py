"""Create the M9 three-point level and its data assets. Run once in UnrealEditor-Cmd.

After creation, open the map in a real editor and run the `SlimeBakeNav` console command so
RecastNavMesh data is baked and saved. Commandlet navigation builds do not finish in this project.
"""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_ThreePoint"
SOURCE_MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_DoublePoint"
RUN_CONFIG_PATH = "/Game/_SlimeWar/Core/Data/DA_RunConfig_ThreePoint"
SOURCE_RUN_CONFIG = "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint"
SPAWN_LAYOUT_PATH = "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_ThreePoint"
LEVEL_DIRECTORY = "/Game/_SlimeWar/Core/Data/Levels"
LEVEL_PATH = f"{LEVEL_DIRECTORY}/DA_Level_ThreePoint"
RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"

D1 = unreal.Vector(-1800.0, -2200.0, 0.0)
D2 = unreal.Vector(1800.0, 2200.0, 0.0)

SITES = [
    ("Site_A", "A", unreal.SlimeFaction.A, unreal.Vector(-1000.0, -1300.0, 2.0),
     unreal.LinearColor(0.33, 0.85, 0.67, 1.0)),
    ("Site_B", "B", unreal.SlimeFaction.B, unreal.Vector(-1000.0, 1300.0, 2.0),
     unreal.LinearColor(0.95, 0.72, 0.36, 1.0)),
    ("Site_C", "C", unreal.SlimeFaction.C, unreal.Vector(1500.0, 0.0, 2.0),
     unreal.LinearColor(0.71, 0.61, 1.0, 1.0)),
]

MINERALS = [
    ("Mineral_A_Near", "A_Near", unreal.Vector(-1400.0, -1300.0, 0.0), 4, 4, False),
    ("Mineral_B_Near", "B_Near", unreal.Vector(-1400.0, 1300.0, 0.0), 4, 4, False),
    ("Mineral_C_Near", "C_Near", unreal.Vector(1900.0, 0.0, 0.0), 4, 4, False),
    ("Mineral_Center_North", "Center_North", unreal.Vector(0.0, 700.0, 0.0), 5, 6, True),
    ("Mineral_Center_South", "Center_South", unreal.Vector(0.0, -700.0, 0.0), 5, 6, True),
]

LEGACY_PREFIXES = (
    "Floor_",
    "Wall_",
    "Divider_",
    "Column_",
    "Shelf_",
    "NavMeshBounds",
    "DirectionalLight",
    "SkyLight",
    "PlayerStart_",
    "Site_",
    "Mineral_",
)


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def save_asset(asset) -> None:
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def place(actor, location: unreal.Vector) -> None:
    actor.set_actor_location(location, False, False)


def create_data_asset(name: str, directory: str, asset_class):
    path = f"{directory}/{name}"
    ensure_directory(directory)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class.static_class())
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        directory,
        asset_class,
        factory,
    )


def create_level_data_assets():
    ensure_directory("/Game/_SlimeWar/Core/Data")
    ensure_directory(LEVEL_DIRECTORY)

    rules = unreal.EditorAssetLibrary.load_asset(RULES_PATH)

    layout = create_data_asset(
        "DA_SpawnLayout_ThreePoint",
        "/Game/_SlimeWar/Core/Data",
        unreal.SlimeSpawnLayout,
    )
    layout.set_editor_property("drop_points", [D1, D2])
    save_asset(layout)

    if not unreal.EditorAssetLibrary.does_asset_exist(RUN_CONFIG_PATH):
        if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_RUN_CONFIG):
            raise RuntimeError(f"missing source run config {SOURCE_RUN_CONFIG}")
        unreal.EditorAssetLibrary.duplicate_asset(SOURCE_RUN_CONFIG, RUN_CONFIG_PATH)

    run_config = unreal.EditorAssetLibrary.load_asset(RUN_CONFIG_PATH)
    run_config.set_editor_property("run_duration", 180.0)
    run_config.set_editor_property("target_score", 160)
    run_config.set_editor_property("auto_start_run", False)
    run_config.set_editor_property("spawn_layout", layout)
    if rules:
        run_config.set_editor_property("ecology_rules", rules)
    save_asset(run_config)

    level = create_data_asset(
        "DA_Level_ThreePoint",
        LEVEL_DIRECTORY,
        unreal.SlimeLevelDefinition,
    )
    map_asset = unreal.EditorAssetLibrary.load_asset(MAP_PACKAGE)
    level.set_editor_property("level_id", "ThreePoint")
    level.set_editor_property("display_name", "Three Point Arena")
    level.set_editor_property("map_asset", map_asset)
    level.set_editor_property("run_config", run_config)
    save_asset(level)
    return run_config, layout, level


def clear_previous_actors() -> None:
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not actor:
            continue
        if actor.get_actor_label().startswith(LEGACY_PREFIXES):
            unreal.EditorLevelLibrary.destroy_actor(actor)


def spawn_box(label: str, location: unreal.Vector, scale: unreal.Vector) -> None:
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    actor.set_actor_label(label)
    place(actor, location)
    actor.set_actor_scale3d(scale)
    component = actor.static_mesh_component
    mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
    component.set_static_mesh(mesh)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name("BlockAll")


def build_map() -> None:
    ensure_directory("/Game/_SlimeWar/Maps")
    # Duplicate the known-good double-point map so the new level inherits its working navigation
    # system and baked RecastNavMesh data. The actors are then replaced with the three-point layout.
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
        unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE)
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP_PACKAGE):
        raise RuntimeError(f"missing source map {SOURCE_MAP_PACKAGE}")
    unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP_PACKAGE, MAP_PACKAGE)
    unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    clear_previous_actors()

    # Floor: 50 m along X, 60 m along Y, top at Z=0.
    spawn_box("Floor_ThreePoint", unreal.Vector(0.0, 0.0, -50.0), unreal.Vector(50.0, 60.0, 1.0))

    # Outer walls.
    spawn_box("Wall_West", unreal.Vector(-2517.5, 0.0, 210.0), unreal.Vector(0.35, 60.0, 4.2))
    spawn_box("Wall_East", unreal.Vector(2517.5, 0.0, 210.0), unreal.Vector(0.35, 60.0, 4.2))
    spawn_box("Wall_South", unreal.Vector(0.0, -3017.5, 210.0), unreal.Vector(50.0, 0.35, 4.2))
    spawn_box("Wall_North", unreal.Vector(0.0, 3017.5, 210.0), unreal.Vector(50.0, 0.35, 4.2))

    # Divider walls: keep every site and mineral reachable, but break long sight lines.
    spawn_box("Divider_A", unreal.Vector(-1000.0, 0.0, 170.0), unreal.Vector(0.3, 10.0, 3.4))
    spawn_box("Divider_North", unreal.Vector(0.0, 1700.0, 170.0), unreal.Vector(16.0, 0.3, 3.4))
    spawn_box("Divider_South", unreal.Vector(0.0, -1700.0, 170.0), unreal.Vector(16.0, 0.3, 3.4))
    spawn_box("Divider_C", unreal.Vector(700.0, 0.0, 170.0), unreal.Vector(0.3, 8.0, 3.4))

    # Columns and low shelves.
    for x in (-2000.0, 2000.0):
        for y in (-2200.0, 0.0, 2200.0):
            spawn_box(f"Column_{int(x)}_{int(y)}", unreal.Vector(x, y, 210.0),
                      unreal.Vector(0.6, 0.6, 4.2))
    for y in (-1800.0, 1800.0):
        spawn_box(f"Column_1200_{int(y)}", unreal.Vector(1200.0, y, 210.0),
                  unreal.Vector(0.6, 0.6, 4.2))
    for x in (-1400.0, 500.0):
        for y in (-800.0, 800.0):
            spawn_box(f"Shelf_{int(x)}_{int(y)}", unreal.Vector(x, y, 50.0),
                      unreal.Vector(1.8, 4.0, 1.0))
    for y in (-1000.0, 1000.0):
        spawn_box(f"Shelf_1800_{int(y)}", unreal.Vector(1800.0, y, 50.0),
                  unreal.Vector(1.8, 4.0, 1.0))

    # NavMesh volume covers the full arena.
    nav = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(0.0, 0.0, 200.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    nav.set_actor_label("NavMeshBounds")
    place(nav, unreal.Vector(0.0, 0.0, 200.0))
    nav.set_actor_scale3d(unreal.Vector(35.0, 42.0, 10.0))

    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 1500.0),
        unreal.Rotator(-55.0, -30.0, 0.0),
    )
    sun.set_actor_label("DirectionalLight")
    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 500.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    sky.set_actor_label("SkyLight")

    for index, location in enumerate((D1, D2), start=1):
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart,
            location,
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        start.set_actor_label(f"PlayerStart_D{index}")
        place(start, location)

    for label, site_id, faction, location, color in SITES:
        site = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SlimeFactionSite,
            location,
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        site.set_actor_label(label)
        place(site, location)
        site.set_editor_property("site_id", site_id)
        site.set_editor_property("faction", faction)
        site.set_editor_property("initial_population", 10)
        site.set_editor_property("activity_radius", 1200.0)
        site.set_editor_property("delivery_radius", 130.0)
        site.set_editor_property("spawn_ring_min", 250.0)
        site.set_editor_property("spawn_ring_max", 400.0)
        site.set_editor_property("display_color", color)

    for label, patch_id, location, initial_stock, max_stock, regenerates in MINERALS:
        patch = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SlimeMineralPatch,
            location,
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        patch.set_actor_label(label)
        place(patch, location)
        patch.set_editor_property("patch_id", patch_id)
        patch.set_editor_property("initial_stock", initial_stock)
        patch.set_editor_property("max_stock", max_stock)
        patch.set_editor_property("regenerates", regenerates)
        patch.set_editor_property("regrow_seconds", 5.0)
        patch.set_editor_property("gather_range", 120.0)

    unreal.EditorLevelLibrary.save_current_level()


def main() -> None:
    build_map()
    create_level_data_assets()
    unreal.log("M9 three-point content created.")


main()
