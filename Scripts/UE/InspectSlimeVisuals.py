import unreal


def describe_material(material_path):
    material = unreal.load_asset(material_path)
    if not material:
        unreal.log("MISSING MATERIAL {}".format(material_path))
        return
    unreal.log("MATERIAL {}".format(material_path))
    try:
        count = unreal.MaterialEditingLibrary.get_num_material_expressions(material)
        unreal.log("  expression_count={}".format(count))
        for index in range(count):
            expression = unreal.MaterialEditingLibrary.get_material_expression(material, index)
            if not expression:
                continue
            unreal.log(
                "  expression[{}] class={} name={}".format(
                    index,
                    expression.get_class().get_name(),
                    expression.get_name(),
                )
            )
            for prop in ("parameter_name", "parameter_type", "texture"):
                try:
                    unreal.log(
                        "    {}={}".format(prop, expression.get_editor_property(prop))
                    )
                except Exception:
                    pass
    except Exception as exc:
        unreal.log("  expression inspection failed: {}".format(exc))


mesh = unreal.load_asset("/Game/Characters/Slime/SM_Slime_Base")
if mesh:
    unreal.log("STATIC_MESH /Game/Characters/Slime/SM_Slime_Base")
    try:
        lod_count = mesh.get_num_lods()
        unreal.log("  lod_count={}".format(lod_count))
        sections = mesh.get_num_sections(0)
        unreal.log("  lod0_sections={}".format(sections))
        for index in range(sections):
            material = mesh.get_material(index)
            unreal.log(
                "  section[{}] material={}".format(
                    index,
                    material.get_path_name() if material else "None",
                )
            )
    except Exception as exc:
        unreal.log("  mesh inspection failed: {}".format(exc))

describe_material("/Game/_SlimeWar/Enemy/Art/M_Slime_Base")
describe_material("/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal")
