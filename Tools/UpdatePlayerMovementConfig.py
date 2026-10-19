"""Add R1 player-control fields without changing any existing conflicting values."""

import unreal


RUN_CONFIGS = [
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_ThreePoint",
]

NEW_VALUES = {
    "player_sprint_speed": 900.0,
    "player_aim_speed": 300.0,
    "player_jump_speed": 700.0,
    "player_gravity": 2000.0,
    "camera_aim_boom_length": 220.0,
    "camera_aim_field_of_view": 55.0,
}

EXISTING_FIELDS = [
    "player_move_speed",
    "player_acceleration",
    "camera_boom_length",
    "camera_field_of_view",
]


for path in RUN_CONFIGS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        unreal.log_error(f"Missing run config: {path}")
        continue

    existing = {
        field: asset.get_editor_property(field)
        for field in EXISTING_FIELDS
    }
    unreal.log(f"R1 existing values {path}: {existing}")

    for field, value in NEW_VALUES.items():
        asset.set_editor_property(field, value)

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log(f"R1 added player-control fields to {path}")
