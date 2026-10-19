"""Force an editor navigation rebuild for the M7 map."""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_DoublePoint"


def main() -> None:
    unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    world = unreal.EditorLevelLibrary.get_editor_world()
    for command in ("RebuildNavigation", "BuildPaths"):
        try:
            unreal.SystemLibrary.execute_console_command(world, command)
            unreal.log(f"NAVBUILD executed {command}")
        except Exception as exc:
            unreal.log_warning(f"NAVBUILD {command}: {exc}")
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("NAVBUILD saved")


main()
