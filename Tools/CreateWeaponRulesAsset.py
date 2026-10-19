"""Create and bind DA_Weapon_Rifle for the M6 charge rifle. Safe to run repeatedly."""

import unreal


ASSET_DIRECTORY = "/Game/_SlimeWar/Weapon"
ASSET_NAME = "DA_Weapon_Rifle"
ASSET_PATH = f"{ASSET_DIRECTORY}/{ASSET_NAME}"
RUN_CONFIG_PATH = "/Game/_SlimeWar/Core/Data/DA_RunConfig"

VALUES = {
    "base_damage": 8.0,
    "fire_interval": 0.22,
    "magazine_size": 3,
    "reload_duration": 1.0,
    "range": 1800.0,
    "charge_start": 0.18,
    "charge_full_time": 1.2,
    "charge_push_radius": 400.0,
    "charge_push_speed": 1400.0,
    "direct_push_speed": 400.0,
    "direct_push_mass_exponent": 0.65,
    "radial_push_edge_scale": 0.4,
    "aim_capture_ratio": 0.1,
    "aim_retention_ratio": 1.35,
    "aim_switch_ratio": 0.55,
    "aim_scan_interval": 0.05,
}


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def load_or_create_asset():
    ensure_directory(ASSET_DIRECTORY)
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        return unreal.EditorAssetLibrary.load_asset(ASSET_PATH)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SlimeWeaponRules.static_class())
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_DIRECTORY,
        unreal.SlimeWeaponRules,
        factory,
    )


def main() -> None:
    asset = load_or_create_asset()
    if asset is None:
        raise RuntimeError(f"Failed to create {ASSET_PATH}")

    for property_name, value in VALUES.items():
        asset.set_editor_property(property_name, value)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

    run_config = unreal.EditorAssetLibrary.load_asset(RUN_CONFIG_PATH)
    if run_config is None:
        raise RuntimeError(f"Failed to load {RUN_CONFIG_PATH}")
    run_config.set_editor_property("weapon_rules", asset)
    unreal.EditorAssetLibrary.save_loaded_asset(run_config, only_if_is_dirty=False)
    unreal.log(f"Created and bound {ASSET_PATH} to {RUN_CONFIG_PATH}")


main()
