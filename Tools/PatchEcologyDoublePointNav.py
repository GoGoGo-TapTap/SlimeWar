"""Ensure the M7 map contains a runtime RecastNavMesh."""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_DoublePoint"


def main() -> None:
    unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    recast = None
    for actor in actors:
        if isinstance(actor, unreal.RecastNavMesh):
            recast = actor
            break

    if recast is None:
        recast = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.RecastNavMesh,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        recast.set_actor_label("RecastNavMesh")

    recast.set_editor_property(
        "runtime_generation",
        unreal.RuntimeGenerationType.DYNAMIC,
    )

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("Ensured RecastNavMesh on L_Ecology_DoublePoint.")


main()
