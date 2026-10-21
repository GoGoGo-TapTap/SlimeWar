import unreal


MESH_PATH = "/Game/Characters/Slime/SM_Slime_Base"


def get_nanite_enabled(mesh):
    settings = mesh.get_editor_property("nanite_settings")
    return bool(settings.get_editor_property("enabled"))


mesh = unreal.load_asset(MESH_PATH)
if not mesh:
    raise RuntimeError("Missing static mesh: {}".format(MESH_PATH))

if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError("Asset is not a StaticMesh: {}".format(MESH_PATH))

before = get_nanite_enabled(mesh)
unreal.log("SLIME_NANITE before={}".format(before))

if before:
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", settings)

    if hasattr(mesh, "set_editor_property"):
        mesh.set_editor_property("nanite_settings", settings)

    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
        raise RuntimeError("Failed to save asset: {}".format(MESH_PATH))

after = get_nanite_enabled(mesh)
unreal.log("SLIME_NANITE after={}".format(after))

if after:
    raise RuntimeError("Failed to disable Nanite for: {}".format(MESH_PATH))
