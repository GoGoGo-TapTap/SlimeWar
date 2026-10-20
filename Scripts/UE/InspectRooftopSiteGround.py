import unreal


MAP_PATH = "/Game/_SlimeWar/Maps/L_Rooftop_Court"
SITE_X = 0.02
SITE_Y = 2910.001


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
unreal.log("MAP {}".format(MAP_PATH))

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_class().get_name() != "StaticMeshActor":
        continue

    origin, extent = actor.get_actor_bounds(False)
    if abs(SITE_X - origin.x) <= extent.x and abs(SITE_Y - origin.y) <= extent.y:
        if extent.z >= 0.1:
            unreal.log(
                "  CANDIDATE label={} origin={} extent={}".format(
                    actor.get_actor_label(),
                    origin,
                    extent,
                )
            )
