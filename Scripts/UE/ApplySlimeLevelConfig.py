import unreal


def vector(x, y, z=0.0):
    return unreal.Vector(float(x), float(y), float(z))


def color_a():
    return unreal.LinearColor(0.518, 0.863, 0.996, 1.0)


def color_b():
    return unreal.LinearColor(0.953, 0.729, 0.769, 1.0)


def color_c():
    return unreal.LinearColor(1.0, 0.953, 0.737, 1.0)


LEVELS = [
    {
        "map": "/Game/_SlimeWar/Maps/L_Rooftop_Court",
        "spawn_layout": "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_RooftopCourt",
        "drop_points": [vector(-630, 800), vector(580, 640)],
        "nav_bounds": (vector(500, 1455, 200), vector(30, 40, 5)),
        "player_start": None,
        "floor_blocker": None,
        "sites": [
            ("A", unreal.SlimeFaction.A, vector(-680, -810), color_a()),
        ],
        "minerals": [
            ("NonRegen_0", vector(0, -210), False),
            ("Regen_0", vector(1000, 600), True),
        ],
        "paint": (vector(500, 1455, 2), (3000.0, 4910.0)),
    },
    {
        "map": "/Game/_SlimeWar/Maps/L_LV2",
        "spawn_layout": "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint",
        "drop_points": [vector(-3740, 2530), vector(3680, -2250)],
        "nav_bounds": (vector(-65, 140, 200), vector(65, 50, 5)),
        "player_start": vector(-3740, 2530, 100),
        "floor_blocker": (vector(-65, 140, -250), vector(65, 50, 2.5)),
        "sites": [
            ("A", unreal.SlimeFaction.A, vector(-4190, 1420), color_a()),
            ("B", unreal.SlimeFaction.B, vector(4060, -1260), color_b()),
        ],
        "minerals": [
            ("NonRegen_0", vector(2600, -520), False),
            ("NonRegen_1", vector(-2260, 610), False),
            ("Regen_0", vector(0, 1310), True),
            ("Regen_1", vector(0, -1280), True),
        ],
        "paint": (vector(-65, 140, 2), (10250.0, 6780.0)),
    },
    {
        "map": "/Game/_SlimeWar/Maps/L_LV3",
        "spawn_layout": "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_ThreePoint",
        "drop_points": [vector(1770, 3550), vector(560, -2920)],
        "nav_bounds": (vector(310, 420, 200), vector(70, 60, 5)),
        "player_start": vector(1770, 3550, 100),
        "floor_blocker": (vector(310, 420, -250), vector(70, 60, 2.5)),
        "sites": [
            ("A", unreal.SlimeFaction.A, vector(-4570, -2870), color_a()),
            ("B", unreal.SlimeFaction.B, vector(5190, -2800), color_b()),
            ("C", unreal.SlimeFaction.C, vector(-130, 3710), color_c()),
        ],
        "minerals": [
            ("NonRegen_0", vector(-1810, -2420), False),
            ("NonRegen_1", vector(800, 2220), False),
            ("NonRegen_2", vector(3000, -2200), False),
            ("Regen_0", vector(140, -610), True),
            ("Regen_1", vector(2490, 100), True),
        ],
        "paint": (vector(310, 420, 2), (11760.0, 8580.0)),
    },
]


def find_class(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError("Missing class {}".format(path))
    return cls


FACTION_SITE_CLASS = find_class("/Script/SlimeWar.SlimeFactionSite")
MINERAL_CLASS = find_class("/Script/SlimeWar.SlimeMineralPatch")
PAINT_CLASS = find_class("/Script/SlimeWar.SlimePaintField")
NAV_BOUNDS_CLASS = find_class("/Script/NavigationSystem.NavMeshBoundsVolume")
BLOCKING_VOLUME_CLASS = find_class("/Script/Engine.BlockingVolume")
PLAYER_START_CLASS = find_class("/Script/Engine.PlayerStart")


def actor_map():
    result = {}
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        result[actor.get_actor_label()] = actor
    return result


def spawn_or_move(actor_class, label, location):
    actors = actor_map()
    actor = actors.get(label)
    if actor:
        actor.set_actor_location(location, False, False)
        return actor

    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = editor_actor_subsystem.spawn_actor_from_class(
        actor_class, location, unreal.Rotator(0, 0, 0)
    )
    actor.set_actor_label(label)
    return actor


def find_or_spawn_by_class(actor_class, label, location):
    actors = actor_map()
    for actor in actors.values():
        if actor.get_class().get_name() == actor_class.get_name():
            actor.set_actor_location(location, False, False)
            return actor

    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = editor_actor_subsystem.spawn_actor_from_class(
        actor_class, location, unreal.Rotator(0, 0, 0)
    )
    actor.set_actor_label(label)
    return actor


def set_bool_property(actor, value):
    for property_name in ("regenerates", "b_regenerates", "bRegenerates"):
        try:
            actor.set_editor_property(property_name, value)
            return
        except Exception:
            pass
    raise RuntimeError("Could not set mineral regeneration boolean on {}".format(actor))


def configure_level(config):
    layout = unreal.load_asset(config["spawn_layout"])
    if not layout:
        raise RuntimeError("Missing spawn layout {}".format(config["spawn_layout"]))

    layout.set_editor_property("drop_points", config["drop_points"])
    unreal.EditorAssetLibrary.save_loaded_asset(layout)

    world = unreal.EditorLoadingAndSavingUtils.load_map(config["map"])
    if not world:
        raise RuntimeError("Missing map {}".format(config["map"]))

    for site_id, faction, location, display_color in config["sites"]:
        actor = spawn_or_move(
            FACTION_SITE_CLASS,
            "SlimeConfig_Site_{}".format(site_id),
            location,
        )
        actor.set_editor_property("site_id", site_id)
        actor.set_editor_property("faction", faction)
        actor.set_editor_property("initial_population", 10)
        actor.set_editor_property("activity_radius", 1200.0)
        actor.set_editor_property("delivery_radius", 130.0)
        actor.set_editor_property("spawn_ring_min", 250.0)
        actor.set_editor_property("spawn_ring_max", 400.0)
        actor.set_editor_property("display_color", display_color)

    for patch_id, location, regenerates in config["minerals"]:
        actor = spawn_or_move(
            MINERAL_CLASS,
            "SlimeConfig_Mineral_{}".format(patch_id),
            location,
        )
        actor.set_editor_property("patch_id", patch_id)
        actor.set_editor_property("initial_stock", 4)
        actor.set_editor_property("max_stock", 4)
        set_bool_property(actor, regenerates)
        actor.set_editor_property("regrow_seconds", 5.0)
        actor.set_editor_property("gather_range", 120.0)

    nav_location, nav_scale = config["nav_bounds"]
    nav = find_or_spawn_by_class(
        NAV_BOUNDS_CLASS,
        "SlimeConfig_NavBounds",
        nav_location,
    )
    nav.set_actor_scale3d(nav_scale)

    if config["floor_blocker"]:
        blocker_location, blocker_scale = config["floor_blocker"]
        blocker = find_or_spawn_by_class(
            BLOCKING_VOLUME_CLASS,
            "SlimeConfig_FloorBlocker",
            blocker_location,
        )
        blocker.set_actor_scale3d(blocker_scale)

    if config["player_start"]:
        find_or_spawn_by_class(
            PLAYER_START_CLASS,
            "SlimeConfig_PlayerStart",
            config["player_start"],
        )

    paint_location, paint_size = config["paint"]
    paint_actor = spawn_or_move(PAINT_CLASS, "SlimeConfig_PaintField", paint_location)
    paint_actor.set_editor_property(
        "world_size",
        unreal.Vector2D(float(paint_size[0]), float(paint_size[1])),
    )

    unreal.EditorLoadingAndSavingUtils.save_map(world, config["map"])
    unreal.log("Configured {}".format(config["map"]))


for level_config in LEVELS:
    configure_level(level_config)
