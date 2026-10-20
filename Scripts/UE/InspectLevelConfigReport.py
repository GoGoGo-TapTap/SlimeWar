import unreal


LEVEL_ASSETS = [
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_CityPlaza",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_ThreePoint",
]

RUN_CONFIGS = [
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_RooftopCourt",
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_ThreePoint",
]

SPAWN_LAYOUTS = [
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_RooftopCourt",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_ThreePoint",
]

MAPS = [
    "/Game/_SlimeWar/Maps/L_Rooftop_Court",
    "/Game/_SlimeWar/Maps/L_LV2",
    "/Game/_SlimeWar/Maps/L_LV3",
]


for path in LEVEL_ASSETS:
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log("MISSING LEVEL {}".format(path))
        continue
    unreal.log("LEVEL {}".format(path))
    for prop in ("level_id", "display_name", "map_asset", "run_config", "deploy_sequences", "result_sequence"):
        try:
            unreal.log("  {}={}".format(prop, asset.get_editor_property(prop)))
        except Exception as exc:
            unreal.log("  {} unavailable: {}".format(prop, exc))

for path in RUN_CONFIGS:
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log("MISSING RUNCONFIG {}".format(path))
        continue
    unreal.log("RUNCONFIG {}".format(path))
    for prop in ("run_duration", "target_score", "deploy_duration", "result_orbit_duration", "spawn_layout", "auto_start_run"):
        try:
            unreal.log("  {}={}".format(prop, asset.get_editor_property(prop)))
        except Exception as exc:
            unreal.log("  {} unavailable: {}".format(prop, exc))

for path in SPAWN_LAYOUTS:
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log("MISSING SPAWNLAYOUT {}".format(path))
        continue
    unreal.log("SPAWNLAYOUT {}".format(path))
    try:
        for index, point in enumerate(asset.get_editor_property("drop_points")):
            unreal.log("  drop[{}]={}".format(index, point))
    except Exception as exc:
        unreal.log("  drop_points unavailable: {}".format(exc))

for map_path in MAPS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        unreal.log("MISSING MAP {}".format(map_path))
        continue
    unreal.log("MAP {}".format(map_path))
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        class_name = actor.get_class().get_name()
        label = actor.get_actor_label()
        if class_name == "SlimeFactionSite":
            unreal.log(
                "  SITE label={} location={} faction={} population={}".format(
                    label,
                    actor.get_actor_location(),
                    actor.get_editor_property("faction"),
                    actor.get_editor_property("initial_population"),
                )
            )
        elif class_name == "SlimeMineralPatch":
            unreal.log(
                "  MINERAL label={} location={} regen={}".format(
                    label,
                    actor.get_actor_location(),
                    actor.get_editor_property("regenerates"),
                )
            )
