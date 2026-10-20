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
        class_name = actor.get_class().get_name()
        label = actor.get_actor_label().lower()
        if class_name in ("Landscape", "StaticMeshActor") and any(
            token in label for token in ("floor", "ground", "court", "plaza", "road", "street")
        ):
            bounds = actor.get_actor_bounds(False)
            unreal.log(
                "  FLOOR {} label={} origin={} extent={}".format(
                    class_name,
                    actor.get_actor_label(),
                    bounds[0],
                    bounds[1],
                )
            )
