import unreal


ASSETS = [
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_RooftopCourt",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_RooftopCourt",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_ThreePoint",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_CityPlaza",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_ThreePoint",
]

MAPS = [
    "/Game/_SlimeWar/Maps/L_Rooftop_Court",
    "/Game/_SlimeWar/Maps/L_LV2",
    "/Game/_SlimeWar/Maps/L_LV3",
]


def describe_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log_warning("MISSING {}".format(path))
        return

    unreal.log("ASSET {} class={}".format(path, asset.get_class().get_name()))
    for prop in ("drop_points", "run_config", "map_asset", "display_name", "level_id"):
        try:
            unreal.log("  {}={}".format(prop, asset.get_editor_property(prop)))
        except Exception as exc:
            unreal.log("  {} unavailable ({})".format(prop, exc))


def describe_map(path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(path)
    if not world:
        unreal.log_warning("MAP LOAD FAILED {}".format(path))
        return

    unreal.log("MAP {}".format(path))
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        class_name = actor.get_class().get_name()
        if class_name in (
            "SlimeFactionSite",
            "SlimeMineralPatch",
            "SlimePaintField",
            "PlayerStart",
        ):
            unreal.log(
                "  {} label={} location={}".format(
                    class_name,
                    actor.get_actor_label(),
                    actor.get_actor_location(),
                )
            )


for asset_path in ASSETS:
    describe_asset(asset_path)

for map_path in MAPS:
    describe_map(map_path)
