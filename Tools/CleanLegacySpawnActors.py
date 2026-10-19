"""Remove retired spawn-point / Normal / Aggro actors from legacy maps.

Run once in UnrealEditor-Cmd after the legacy classes are deleted:

  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<repo>/Tools/CleanLegacySpawnActors.py"

The classes no longer exist, so the actors would otherwise load as missing-class placeholders.
Only the explicitly listed maps are touched, and only actors whose class or label matches the
retired names are destroyed.
"""

import unreal


MAPS = [
    "/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza",
]

LEGACY_CLASS_NAMES = {"SpawnPoint", "SlimeNormal", "SlimeAggro"}
LEGACY_LABEL_PREFIXES = ("SpawnPoint", "SlimeNormal", "SlimeAggro", "SlimeSpawnSlot")


def is_legacy(actor) -> bool:
    if not actor:
        return False

    actor_class = actor.get_class()
    class_name = actor_class.get_name() if actor_class else ""
    label = actor.get_actor_label()
    return class_name in LEGACY_CLASS_NAMES or label.startswith(LEGACY_LABEL_PREFIXES)


def clean_map(map_path: str) -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.log_warning(f"CleanLegacySpawnActors: {map_path} does not exist, skipped.")
        return

    unreal.EditorLevelLibrary.load_level(map_path)
    removed = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if is_legacy(actor):
            removed.append(f"{actor.get_actor_label()} ({actor.get_class().get_name() if actor.get_class() else 'missing'})")
            unreal.EditorLevelLibrary.destroy_actor(actor)

    if removed:
        unreal.EditorLevelLibrary.save_current_level()

    unreal.log(f"CleanLegacySpawnActors: {map_path} removed {len(removed)} actor(s).")
    for entry in removed:
        unreal.log(f"  - {entry}")


for legacy_map in MAPS:
    clean_map(legacy_map)
