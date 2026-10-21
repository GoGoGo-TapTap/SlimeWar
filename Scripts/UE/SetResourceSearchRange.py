import unreal


RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"
RULES = unreal.load_asset(RULES_PATH)
if not RULES:
    raise RuntimeError("Missing {}".format(RULES_PATH))

RULES.set_editor_property("resource_exploration_range", 8000.0)
unreal.EditorAssetLibrary.save_loaded_asset(RULES)
unreal.log("Set ResourceExplorationRange=8000.0")
