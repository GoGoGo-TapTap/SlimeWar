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
    "fusion_auto_prep_min": 14.0,
    "fusion_auto_prep_max": 22.0,
    "fusion_auto_seek_range": 400.0,
    "fusion_auto_seek_speed": 75.0,
    "fusion_contact_time": 2.4,
    "fusion_contact_tolerance": 18.0,
    "fusion_forced_capture_gap": 8.0,
    "fusion_success_cooldown": 1.8,
    "fusion_interrupted_block": 5.0,
    "fusion_auto_mass_cap": 2,
    "fusion_total_mass_cap": 8,
    "fusion_seek_timeout": 10.0,
    "core_wake_time": 1.2,
    "core_sense_radius": 900.0,
    "core_chase_speed": 380.0,
    "core_combat_memory": 6.0,
    "core_leash_distance": 1900.0,
    "core_player_proximity": 240.0,
    "core_player_provoked_duration": 6.0,
    "core_retreat_health_fraction": 0.3,
    "core_retreat_duration": 2.5,
    "core_attack_interval": 1.6,
    "core_windup": 0.65,
    "core_recovery": 0.4,
    "pounce_base_range": 340.0,
    "pounce_speed": 1050.0,
    "pounce_duration": 0.4,
    "pounce_lift": 300.0,
    "pounce_hit_height": 240.0,
    "core_damage_player": 20.0,
    "core_damage_rival": 16.0,
    "devour_heal_per_mass": 5.0,
    "explosion_base_radius": 300.0,
    "explosion_radius_per_mass": 25.0,
    "explosion_damage": 42.0,
    "explosion_edge_scale": 0.65,
    "explosion_push": 800.0,
    "explosion_push_mass_exponent": 0.45,
    "hostile_cores_chain_only": True,
    "weakpoint_start_mass": 7,
    "weakpoint_radius": 30.0,
    "weakpoint_height": 32.0,
    "resource_sense_range": 800.0,
    "resource_exploration_range": 2000.0,
    "resource_memory": 28.0,
    "gather_range": 120.0,
    "gather_time": 2.0,
    "delivery_range": 130.0,
    "pickup_range": 110.0,
    "cargo_speed": 115.0,
    "gather_rest_min": 2.0,
    "gather_rest_max": 4.0,
    "site_spawn_ring_min": 250.0,
    "site_spawn_ring_max": 400.0,
    "site_spawn_player_min_distance": 270.0,
    "spawn_requests_per_frame": 8,
    "alert_warning_time": 1.0,
    "alert_duration": 10.0,
    "alert_propagation_radius": 500.0,
    "alert_danger_speed": 154.0,
    "competitor_sense_radius": 600.0,
    "hostile_core_avoid_carrying": 220.0,
    "hostile_core_avoid_empty": 180.0,
    "dispute_range": 340.0,
    "dispute_seconds": 1.5,
    "dispute_cooldown": 5.0,
    "grenades_per_type": 3,
    "grenade_throw_interval": 0.7,
    "grenade_throw_speed": 1300.0,
    "grenade_throw_lift": 400.0,
    "grenade_gravity": 2000.0,
    "grenade_flight_time": 2.0,
    "assimilation_radius": 450.0,
    "tornado_radius": 600.0,
    "tornado_duration": 4.5,
    "tornado_acceleration": 1200.0,
    "tornado_max_speed": 450.0,
    "tornado_stop_radius": 20.0,
    "tornado_fusion_ready_time": 0.5,
    "paint_texture_width": 700,
    "paint_texture_height": 1024,
    "max_paint_splat_records": 2048,
    "paint_world_size": (3000.0, 4400.0),
    "paint_texture_parameter": "SlimePaint",
    "paint_splat_uv_parameter": "SplatUV",
    "paint_splat_color_parameter": "SplatColor",
    "paint_splat_radius_parameter": "SplatRadius",
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
