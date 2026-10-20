import unreal


MAPS = [
    "/Game/_SlimeWar/Maps/L_Rooftop_Court",
    "/Game/_SlimeWar/Maps/L_LV2",
    "/Game/_SlimeWar/Maps/L_LV3",
]


for map_path in MAPS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        unreal.log("MAP LOAD FAILED {}".format(map_path))
        continue

    unreal.log("MAP {}".format(map_path))
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        class_name = actor.get_class().get_name()
        if class_name == "SlimeFactionSite":
            unreal.log(
                "  SITE label={} location={} site_id={} faction={} population={}".format(
                    actor.get_actor_label(),
                    actor.get_actor_location(),
                    actor.get_editor_property("site_id"),
                    actor.get_editor_property("faction"),
                    actor.get_editor_property("initial_population"),
                )
            )
        elif class_name in ("RecastNavMesh", "NavMeshBoundsVolume", "BlockingVolume", "PlayerStart"):
            unreal.log(
                "  WORLD {} label={} location={} scale={}".format(
                    class_name,
                    actor.get_actor_label(),
                    actor.get_actor_location(),
                    actor.get_actor_scale3d(),
                )
            )
