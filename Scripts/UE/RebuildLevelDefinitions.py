import unreal


LEVEL_DIR = "/Game/_SlimeWar/Core/Data/Levels"


def soft_object(path):
    return unreal.SoftObjectPath(path)


def rebuild_level(asset_name, level_id, display_name, map_path, run_config_path, preparation_class_path):
    asset_path = "{}/{}".format(LEVEL_DIR, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise RuntimeError("Could not delete old asset {}".format(asset_path))

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SlimeLevelDefinition)
    asset = asset_tools.create_asset(
        asset_name,
        LEVEL_DIR,
        unreal.SlimeLevelDefinition,
        factory,
    )
    if not asset:
        raise RuntimeError("Could not create {}".format(asset_path))

    map_asset = unreal.load_asset(map_path)
    run_config_asset = unreal.load_asset(run_config_path)
    preparation_class = unreal.load_class(None, preparation_class_path)
    if not map_asset:
        raise RuntimeError("Could not load map {}".format(map_path))
    if not run_config_asset:
        raise RuntimeError("Could not load run config {}".format(run_config_path))

    asset.set_editor_property("level_id", level_id)
    asset.set_editor_property("display_name", display_name)
    asset.set_editor_property("map_asset", map_asset)
    asset.set_editor_property("run_config", run_config_asset)
    asset.set_editor_property("preparation_widget_class", preparation_class)
    asset.set_editor_property("deploy_sequences", [])
    asset.set_editor_property("result_sequence", unreal.SoftObjectPath())

    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("Could not save {}".format(asset_path))
    unreal.log("REBUILT {}".format(asset_path))


rebuild_level(
    "DA_Level_CityPlaza",
    "CityPlaza",
    "Rooftop Court",
    "/Game/_SlimeWar/Maps/L_Rooftop_Court",
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_RooftopCourt",
    "/Game/_SlimeWar/UI/WBP_SlimePreparation.WBP_SlimePreparation_C",
)

rebuild_level(
    "DA_Level_DoublePoint",
    "DoublePoint",
    "LV2",
    "/Game/_SlimeWar/Maps/L_LV2",
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint",
    "/Game/_SlimeWar/UI/WBP_SlimePreparation.WBP_SlimePreparation_C",
)
