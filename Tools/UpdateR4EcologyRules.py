"""Add approved R4 fusion/core fields without changing conflicting legacy values."""

import unreal


RULES_PATH = "/Game/_SlimeWar/Ecology/DA_EcologyRules"
VALUES = {
    # N06 remains Recorded - No Change. Displacement fusion uses the existing
    # 8 cm FusionForcedCaptureGap instead of adding the 65/45 conflict values.
    "shot_fusion_cooldown": 0.8,
    "core_player_priority_proximity": 180.0,
}


asset = unreal.EditorAssetLibrary.load_asset(RULES_PATH)
if asset is None:
    raise RuntimeError(f"Missing {RULES_PATH}")

for field, value in VALUES.items():
    asset.set_editor_property(field, value)

unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log(f"Added R4 behaviour fields to {RULES_PATH}")
