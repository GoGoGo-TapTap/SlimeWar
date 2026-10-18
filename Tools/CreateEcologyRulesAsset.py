"""Create and bind the M2 ecology rules asset. Safe to run repeatedly."""

import unreal


ASSET_DIRECTORY = "/Game/_SlimeWar/Ecology"
ASSET_NAME = "DA_EcologyRules"
ASSET_PATH = f"{ASSET_DIRECTORY}/{ASSET_NAME}"
RUN_CONFIG_PATH = "/Game/_SlimeWar/Core/Data/DA_RunConfig"

VALUES = {
    "perception_interval": 0.25,
    "ally_perception_radius": 700.0,
    "sight_height_tolerance": 200.0,
    "group_exit_distance": 280.0,
    "group_enter_distance": 180.0,
    "player_avoid_enter_distance": 240.0,
    "player_avoid_exit_distance": 320.0,
    "player_avoid_speed": 110.0,
    "avoid_candidate_distance": 450.0,
    "avoid_candidate_count": 5,
    "explore_speed": 115.0,
    "regroup_speed": 115.0,
    "extra_spacing": 25.0,
    "separation_weight": 3.0,
    "separation_overlay_weight": 0.35,
    "direction_smoothing": 8.0,
    "move_acceleration": 1200.0,
    "braking_deceleration": 1800.0,
    "turn_rate_deg_per_second": 360.0,
    "wander_radius_min": 300.0,
    "wander_radius_max": 600.0,
    "wander_minimum_displacement": 150.0,
    "wander_arrival_radius": 70.0,
    "wander_selection_interval": 12.0,
    "wander_candidate_count": 12,
    "path_refresh_interval": 0.5,
    "waypoint_acceptance_radius": 40.0,
    "stuck_check_seconds": 2.0,
    "stuck_distance": 15.0,
    "max_path_failures": 3,
    "spatial_cell_size": 400.0,
    "body_push_max_mass": 3,
    "body_push_base_speed": 260.0,
    "body_push_player_speed_scale": 0.4,
    "body_push_mass_exponent": 0.35,
    "body_push_contact_padding": 20.0,
    "external_velocity_damping": 500.0,
    "stun_stop_speed": 10.0,
    "stun_settle_time": 0.1,
    "hit_stun_duration": 1.5,
    "split_stun_duration": 2.0,
    "forced_fusion_window": 3.0,
    "impact_split_speed": 600.0,
    "impact_split_window": 0.9,
}


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def load_or_create_asset():
    ensure_directory(ASSET_DIRECTORY)
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        return unreal.EditorAssetLibrary.load_asset(ASSET_PATH)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SlimeEcologyRules.static_class())
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_DIRECTORY,
        unreal.SlimeEcologyRules,
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

    run_config.set_editor_property("ecology_rules", asset)
    unreal.EditorAssetLibrary.save_loaded_asset(run_config, only_if_is_dirty=False)
    unreal.log(f"Created and bound {ASSET_PATH} to {RUN_CONFIG_PATH}")


main()
