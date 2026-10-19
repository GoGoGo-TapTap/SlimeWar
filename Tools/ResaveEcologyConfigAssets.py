"""Resave the ecology data assets so properties removed from C++ are dropped from their packages.

Run once in UnrealEditor-Cmd after removing legacy fields from USlimeRunConfig /
USlimeSpawnLayout. The assets keep their existing values; this only rewrites the serialized form.
"""

import unreal


ASSET_PATHS = [
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint",
]


for asset_path in ASSET_PATHS:
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log_warning(f"ResaveEcologyConfigAssets: missing {asset_path}")
        continue

    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        unreal.log_warning(f"ResaveEcologyConfigAssets: could not load {asset_path}")
        continue

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log_warning(f"ResaveEcologyConfigAssets: saved {asset_path}")
