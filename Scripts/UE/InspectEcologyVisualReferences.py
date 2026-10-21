import unreal


RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"
MESH_PATH = "/Game/Characters/Slime/SM_Slime_Base"

rules = unreal.load_asset(RULES_PATH)
if not rules:
    raise RuntimeError("Missing rules asset: {}".format(RULES_PATH))

unreal.log("ECOLOGY_RULES {}".format(RULES_PATH))
for prop in ("faction_visuals", "spike_mesh", "core_mesh", "weakpoint_mesh",
             "core_material", "weakpoint_material"):
    try:
        unreal.log("  {}={}".format(prop, rules.get_editor_property(prop)))
    except Exception as exc:
        unreal.log("  {} unavailable: {}".format(prop, exc))

mesh = unreal.load_asset(MESH_PATH)
if mesh:
    settings = mesh.get_editor_property("nanite_settings")
    unreal.log("SLIME_MESH {} nanite={}".format(
        MESH_PATH, settings.get_editor_property("enabled")))
else:
    unreal.log("SLIME_MESH_MISSING {}".format(MESH_PATH))
