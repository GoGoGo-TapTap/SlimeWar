import unreal


RULES = unreal.load_asset("/Game/_SlimeWar/Ecology/DA_EcologyRules")
MATERIALS = [
    "/Game/_SlimeWar/Enemy/Art/MI_Slime_FactionA",
    "/Game/_SlimeWar/Enemy/Art/MI_Slime_FactionB",
    "/Game/_SlimeWar/Enemy/Art/MI_Slime_FactionC",
    "/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal",
]


if RULES:
    for prop in (
        "faction_visuals",
        "faction_colors",
        "spike_mesh",
        "core_mesh",
        "weakpoint_mesh",
        "core_material",
        "weakpoint_material",
        "paint_splat_material",
        "paint_surface_material",
        "paint_decal_material",
        "paint_world_size",
    ):
        try:
            unreal.log("RULES {}={}".format(prop, RULES.get_editor_property(prop)))
        except Exception as exc:
            unreal.log("RULES {} unavailable: {}".format(prop, exc))
else:
    unreal.log("RULES missing")

for material_path in MATERIALS:
    material = unreal.load_asset(material_path)
    if not material:
        unreal.log("MATERIAL missing {}".format(material_path))
        continue

    try:
        domain = material.get_editor_property("material_domain")
    except Exception:
        domain = None
    unreal.log("MATERIAL {} domain={}".format(material_path, domain))

    for parameter in ("TintNormal", "Opacity", "RimStrength", "JiggleSpeed", "JiggleStrength"):
        try:
            value = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
                material, parameter
            )
            unreal.log("  vector {}={}".format(parameter, value))
        except Exception:
            try:
                value = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
                    material, parameter
                )
                unreal.log("  scalar {}={}".format(parameter, value))
            except Exception:
                pass

