import unreal


MATERIAL_PATH = "/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal"
TEXTURE_PATH = "/Game/_SlimeWar/Enemy/Art/T_Slime_Splatter_01"


material = unreal.load_asset(MATERIAL_PATH)
texture = unreal.load_asset(TEXTURE_PATH)

if material:
    unreal.log("MATERIAL {}".format(MATERIAL_PATH))
    unreal.log("  domain={}".format(material.get_editor_property("material_domain")))
    try:
        expressions = unreal.MaterialEditingLibrary.get_num_material_expressions(material)
        unreal.log("  expressions={}".format(expressions))
        for index in range(expressions):
            expression = unreal.MaterialEditingLibrary.get_material_expression(material, index)
            if not expression:
                continue
            name = expression.get_class().get_name()
            try:
                parameter_name = expression.get_editor_property("parameter_name")
            except Exception:
                parameter_name = None
            if parameter_name:
                unreal.log("  expression={} parameter={}".format(name, parameter_name))
    except Exception as exc:
        unreal.log("  expression inspection failed: {}".format(exc))

if texture:
    unreal.log("TEXTURE {}".format(TEXTURE_PATH))
    unreal.log("  size={}x{}".format(texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
