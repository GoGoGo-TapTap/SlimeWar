import unreal


MAP_PATH = "/Game/_SlimeWar/Maps/L_Rooftop_Court"
TARGET = unreal.Vector(-749.98, -857.089999, 0.0)


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError("Missing map {}".format(MAP_PATH))

site = None
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_class().get_name() == "SlimeFactionSite":
        site = actor
        break

if not site:
    raise RuntimeError("No SlimeFactionSite in {}".format(MAP_PATH))

site.set_actor_label("SlimeConfig_Site_A")
site.set_actor_location(TARGET, False, False)
site.set_editor_property("site_id", "A")
site.set_editor_property("faction", unreal.SlimeFaction.A)
site.set_editor_property("initial_population", 10)
site.set_editor_property("activity_radius", 1200.0)
site.set_editor_property("delivery_radius", 130.0)
site.set_editor_property("spawn_ring_min", 250.0)
site.set_editor_property("spawn_ring_max", 400.0)
site.set_editor_property("display_color", unreal.LinearColor(0.518, 0.863, 0.996, 1.0))

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
unreal.log("Rooftop site moved to {}".format(TARGET))
