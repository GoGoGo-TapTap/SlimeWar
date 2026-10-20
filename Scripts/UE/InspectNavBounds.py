import unreal


MAPS = [
    "/Game/_SlimeWar/Maps/L_Rooftop_Court",
    "/Game/_SlimeWar/Maps/L_LV2",
    "/Game/_SlimeWar/Maps/L_LV3",
]


for map_path in MAPS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    unreal.log("MAP {}".format(map_path))
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() == "NavMeshBoundsVolume":
            bounds = actor.get_actor_bounds(False)
            unreal.log(
                "  NAVBOUNDS label={} origin={} extent={} scale={}".format(
                    actor.get_actor_label(),
                    bounds[0],
                    bounds[1],
                    actor.get_actor_scale3d(),
                )
            )
