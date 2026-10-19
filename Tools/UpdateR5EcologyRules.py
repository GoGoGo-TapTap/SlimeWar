"""Add R5 alert/paint/exploration behaviour fields."""

import unreal


RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"
VALUES = {
    "alert_post_empty_gather_rest": 4.0,
    "paint_sense_range": 600.0,
    "paint_approach_speed": 65.0,
    "paint_stop_radius": 50.0,
    "exploration_history_cell_size": 400.0,
    "exploration_visit_penalty": 5.0,
}


asset = unreal.EditorAssetLibrary.load_asset(RULES_PATH)
if not asset:
    raise RuntimeError(f"Missing {RULES_PATH}")

for field, value in VALUES.items():
    asset.set_editor_property(field, value)

unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log(f"Added R5 behaviour fields to {RULES_PATH}")
