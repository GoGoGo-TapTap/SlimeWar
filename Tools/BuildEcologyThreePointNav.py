"""Legacy commandlet navigation rebuild for the M9 three-point map.

This commandlet does not complete Recast baking in this project. Prefer opening the map in the
editor and running the `SlimeBakeNav` console command.
"""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_ThreePoint"


def main() -> None:
    unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    world = unreal.EditorLevelLibrary.get_editor_world()
    for command in ("RebuildNavigation", "BuildPaths"):
        try:
            unreal.SystemLibrary.execute_console_command(world, command)
            unreal.log(f"NAVBUILD3 executed {command}")
        except Exception as exc:
            unreal.log_warning(f"NAVBUILD3 {command}: {exc}")
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("NAVBUILD3 saved")


main()
