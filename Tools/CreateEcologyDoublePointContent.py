"""Create the M7 double-point level and its data assets. Run once in UnrealEditor-Cmd."""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_DoublePoint"
RUN_CONFIG_PATH = "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint"
SPAWN_LAYOUT_PATH = "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint"
LEVEL_DIRECTORY = "/Game/_SlimeWar/Core/Data/Levels"
LEVEL_PATH = f"{LEVEL_DIRECTORY}/DA_Level_DoublePoint"
RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"

D1 = unreal.Vector(-1100.0, -1800.0, 0.0)
D2 = unreal.Vector(1100.0, 1800.0, 0.0)
SITE_A = unreal.Vector(-100.0, -1400.0, 2.0)
SITE_B = unreal.Vector(-100.0, 1400.0, 2.0)

MINERALS = [
    ("WestNear", unreal.Vector(500.0, -700.0, 0.0), 4, 4, False),
    ("EastNear", unreal.Vector(-500.0, 700.0, 0.0), 4, 4, False),
    ("SouthShared", unreal.Vector(600.0, 0.0, 0.0), 5, 6, True),
    ("NorthShared", unreal.Vector(-600.0, 0.0, 0.0), 5, 6, True),
]


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
        "DA_SpawnLayout_DoublePoint",
        "/Game/_SlimeWar/Core/Data",
        unreal.SlimeSpawnLayout,
    )
    layout.set_editor_property("drop_points", [D1, D2])
    save_asset(layout)

    if not unreal.EditorAssetLibrary.does_asset_exist(RUN_CONFIG_PATH):
        raise RuntimeError(
            f"{RUN_CONFIG_PATH} is missing. Create it once with the full data table bindings; "
            "this historical tool no longer duplicates the retired DA_RunConfig."
        )
    run_config = unreal.EditorAssetLibrary.load_asset(RUN_CONFIG_PATH)
    run_config.set_editor_property("run_duration", 150.0)
    run_config.set_editor_property("target_score", 200)
    run_config.set_editor_property("auto_start_run", False)
    run_config.set_editor_property("spawn_layout", layout)
    if rules:
        run_config.set_editor_property("ecology_rules", rules)
    save_asset(run_config)

    level = create_data_asset(
        "DA_Level_DoublePoint",
        LEVEL_DIRECTORY,
        unreal.SlimeLevelDefinition,
    )
    map_asset = unreal.EditorAssetLibrary.load_asset(MAP_PACKAGE)
    level.set_editor_property("level_id", "DoublePoint")
    level.set_editor_property("display_name", "Double Point Warehouse")
    level.set_editor_property("map_asset", map_asset)
    level.set_editor_property("run_config", run_config)
    save_asset(level)
    return run_config, layout, level


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
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
        unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    else:
        unreal.EditorLevelLibrary.new_level(MAP_PACKAGE)

    level_system = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_system:
        pass

    # Floor: 44 m along Y, 30 m along X, 1 m thick, top at Z=0.
    spawn_box("Floor_44x30", unreal.Vector(0.0, 0.0, -50.0), unreal.Vector(30.0, 44.0, 1.0))

    # Outer walls.
    spawn_box("Wall_West", unreal.Vector(-1517.5, 0.0, 210.0), unreal.Vector(0.35, 44.0, 4.2))
    spawn_box("Wall_East", unreal.Vector(1517.5, 0.0, 210.0), unreal.Vector(0.35, 44.0, 4.2))
    spawn_box("Wall_South", unreal.Vector(0.0, -2217.5, 210.0), unreal.Vector(30.0, 0.35, 4.2))
    spawn_box("Wall_North", unreal.Vector(0.0, 2217.5, 210.0), unreal.Vector(30.0, 0.35, 4.2))

    # Three divider walls.
    for index, x in enumerate((-1200.0, 0.0, 1200.0)):
        spawn_box(
            f"Divider_{index}",
            unreal.Vector(x, 0.0, 170.0),
            unreal.Vector(0.3, 6.0, 3.4),
        )

    # Columns and low shelves.
    for sx in (-1000.0, 1000.0):
        for sy in (-1700.0, 1700.0):
            spawn_box(
                f"Column_{int(sx)}_{int(sy)}",
                unreal.Vector(sx, sy, 210.0),
                unreal.Vector(0.6, 0.6, 4.2),
            )
    for sx in (-900.0, 900.0):
        for sy in (-1200.0, 1200.0):
            spawn_box(
                f"Shelf_{int(sx)}_{int(sy)}",
                unreal.Vector(sx, sy, 50.0),
                unreal.Vector(1.8, 4.0, 1.0),
            )

    # NavMesh and lighting.
    nav = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(0.0, 0.0, 200.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    nav.set_actor_label("NavMeshBounds")
    place(nav, unreal.Vector(0.0, 0.0, 200.0))
    nav.set_actor_scale3d(unreal.Vector(40.0, 55.0, 10.0))

    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 1500.0),
        unreal.Rotator(-55.0, -30.0, 0.0),
    )
    sun.set_actor_label("DirectionalLight")
    place(sun, unreal.Vector(0.0, 0.0, 1500.0))
    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 500.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    sky.set_actor_label("SkyLight")
    place(sky, unreal.Vector(0.0, 0.0, 500.0))

    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        D1,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    player_start.set_actor_label("PlayerStart_D1")
    place(player_start, D1)

    site_a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SlimeFactionSite,
        SITE_A,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    site_a.set_actor_label("Site_A")
    place(site_a, SITE_A)
    site_a.set_editor_property("site_id", "A")
    site_a.set_editor_property("faction", unreal.SlimeFaction.A)
    site_a.set_editor_property("initial_population", 10)
    site_a.set_editor_property("activity_radius", 1200.0)
    site_a.set_editor_property("delivery_radius", 130.0)
    site_a.set_editor_property("spawn_ring_min", 250.0)
    site_a.set_editor_property("spawn_ring_max", 400.0)

    site_b = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SlimeFactionSite,
        SITE_B,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    site_b.set_actor_label("Site_B")
    place(site_b, SITE_B)
    site_b.set_editor_property("site_id", "B")
    site_b.set_editor_property("faction", unreal.SlimeFaction.B)
    site_b.set_editor_property("initial_population", 10)
    site_b.set_editor_property("activity_radius", 1200.0)
    site_b.set_editor_property("delivery_radius", 130.0)
    site_b.set_editor_property("spawn_ring_min", 250.0)
    site_b.set_editor_property("spawn_ring_max", 400.0)

    for patch_id, location, initial_stock, max_stock, regenerates in MINERALS:
        patch = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SlimeMineralPatch,
            location,
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        patch.set_actor_label(f"Mineral_{patch_id}")
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
    unreal.log("M7 double-point content created.")


main()
