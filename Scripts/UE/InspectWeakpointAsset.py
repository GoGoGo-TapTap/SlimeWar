import unreal


mesh = unreal.load_asset("/Game/_SlimeWar/Enemy/Art/SM_Slime_Weakpoint")
if mesh:
    unreal.log("WEAKPOINT_MESH {}".format(mesh.get_path_name()))
    try:
        bounds = mesh.get_bounds()
        unreal.log("  bounds={}".format(bounds))
    except Exception as exc:
        unreal.log("  bounds unavailable: {}".format(exc))
    try:
        unreal.log("  sections={}".format(mesh.get_num_sections(0)))
        for index in range(mesh.get_num_sections(0)):
            unreal.log("  material[{}]={}".format(index, mesh.get_material(index)))
    except Exception as exc:
        unreal.log("  material inspection failed: {}".format(exc))
else:
    unreal.log("WEAKPOINT_MESH missing")
