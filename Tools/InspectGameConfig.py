"""Read-only inspection of the playable levels, run configs and gameplay rules.

Run with:
  UnrealEditor-Cmd.exe SlimeWar.uproject -run=PythonScript -script="<repo>/Tools/InspectGameConfig.py"
"""

import unreal


RUN_CONFIGS = (
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_RunConfig_ThreePoint",
)
LEVELS = (
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint",
    "/Game/_SlimeWar/Core/Data/Levels/DA_Level_ThreePoint",
)
SPAWN_LAYOUTS = (
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint",
    "/Game/_SlimeWar/Core/Data/DA_SpawnLayout_ThreePoint",
)
RULES = (
    "/Game/_SlimeWar/Ecology/DA_EcologyRules",
    "/Game/_SlimeWar/Weapon/DA_Weapon_Rifle",
)

RUN_FIELDS = (
    "run_duration",
    "target_score",
    "deploy_duration",
    "result_orbit_duration",
    "auto_start_run",
    "tutorial_fusion_proximity",
    "player_max_health",
    "player_move_speed",
    "player_sprint_speed",
    "player_aim_speed",
    "player_jump_speed",
    "player_gravity",
    "player_turn_rate_deg_per_sec",
    "player_acceleration",
    "player_braking_deceleration",
    "hit_protection_duration",
    "camera_boom_length",
    "camera_aim_boom_length",
    "camera_socket_offset",
    "camera_field_of_view",
    "camera_aim_field_of_view",
    "muzzle_offset_local",
    "aim_assist_max_angle_deg",
    "spawn_layout",
    "ecology_rules",
    "weapon_rules",
    "default_weapon_id",
)

LEVEL_FIELDS = (
    "level_id",
    "display_name",
    "map_asset",
    "run_config",
    "preparation_widget_class",
    "deploy_sequences",
    "result_sequence",
)

ECOLOGY_FIELDS = (
    "perception_interval",
    "player_avoid_enter_distance",
    "player_avoid_exit_distance",
    "player_avoid_speed",
    "body_push_max_mass",
    "body_push_base_speed",
    "external_velocity_damping",
    "stun_stop_speed",
    "stun_settle_time",
    "hit_stun_duration",
    "split_stun_duration",
    "forced_fusion_window",
    "impact_split_speed",
    "fusion_auto_prep_min",
    "fusion_auto_prep_max",
    "fusion_auto_seek_range",
    "fusion_auto_seek_speed",
    "fusion_contact_time",
    "fusion_contact_tolerance",
    "fusion_forced_capture_gap",
    "fusion_success_cooldown",
    "fusion_interrupted_block",
    "fusion_auto_mass_cap",
    "fusion_total_mass_cap",
    "shot_fusion_cooldown",
    "core_player_priority_proximity",
    "core_player_proximity",
    "resource_sense_range",
    "resource_exploration_range",
    "gather_time",
    "delivery_range",
    "pickup_action_range",
    "cargo_speed",
    "alert_post_empty_gather_rest",
    "paint_sense_range",
    "paint_approach_speed",
    "paint_stop_radius",
    "assimilation_radius",
    "tornado_radius",
    "tornado_duration",
    "weakpoint_start_mass",
    "paint_world_size",
)

WEAPON_FIELDS = (
    "base_damage",
    "fire_interval",
    "magazine_size",
    "reload_duration",
    "range",
    "charge_start",
    "charge_full_time",
    "charge_push_radius",
    "charge_push_speed",
    "direct_push_speed",
    "aim_capture_ratio",
    "aim_retention_ratio",
    "aim_switch_ratio",
    "aim_scan_interval",
)


def log_asset(path, fields):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        unreal.log_warning(f"CONFIG INSPECT missing {path}")
        return
    unreal.log_warning(f"CONFIG INSPECT {path} class={asset.get_class().get_name()}")
    for field in fields:
        try:
            unreal.log_warning(f"CONFIG INSPECT {path} {field}={asset.get_editor_property(field)}")
        except Exception as exc:
            unreal.log_warning(f"CONFIG INSPECT {path} {field}=<{exc}>")


for config_path in RUN_CONFIGS:
    log_asset(config_path, RUN_FIELDS)

for level_path in LEVELS:
    log_asset(level_path, LEVEL_FIELDS)

for layout_path in SPAWN_LAYOUTS:
    log_asset(layout_path, ("drop_points",))

log_asset(RULES[0], ECOLOGY_FIELDS)
log_asset(RULES[1], WEAPON_FIELDS)
