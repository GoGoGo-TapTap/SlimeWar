"""Add R2/R3 behaviour timing fields. Existing conflicting values are untouched."""

import unreal


RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"
NEW_VALUES = {
    "resource_target_hold": 2.0,
    "pickup_action_range": 100.0,
    "explore_speed_with_resource_memory": 90.0,
    "initial_gather_rest_min": 0.1,
    "initial_gather_rest_max": 1.2,
    "mineral_crowding_radius": 140.0,
    "mineral_crowding_penalty": 1.8,
}


asset = unreal.EditorAssetLibrary.load_asset(RULES_PATH)
if asset is None:
    raise RuntimeError(f"Missing {RULES_PATH}")

for field, value in NEW_VALUES.items():
    asset.set_editor_property(field, value)

unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log(f"Added R2/R3 behaviour fields to {RULES_PATH}")
