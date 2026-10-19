"""Inspect and repair navigation actors on the M7 map."""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_DoublePoint"


def main() -> None:
    unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    recast = None
    for actor in actors:
        class_name = actor.get_class().get_name()
        if "NavMeshBounds" in class_name:
            origin, extent = actor.get_actor_bounds(False)
            unreal.log(f"NAVINSPECT volume {actor.get_actor_label()} origin={origin} extent={extent} scale={actor.get_actor_scale3d()}")
        if "RecastNavMesh" in class_name or isinstance(actor, unreal.NavigationData):
            unreal.log(f"NAVINSPECT navdata {actor.get_actor_label()} class={class_name}")
            if isinstance(actor, unreal.RecastNavMesh):
                recast = actor

    unreal.log(f"NAVINSPECT recast_present={recast is not None}")
    if recast is None:
        recast = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.RecastNavMesh,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        unreal.log(f"NAVINSPECT spawned={recast}")
        if recast:
            recast.set_actor_label("RecastNavMesh")

    if recast:
        recast.set_editor_property(
            "runtime_generation",
            unreal.RuntimeGenerationType.DYNAMIC,
        )
        unreal.log("NAVINSPECT set runtime_generation=DYNAMIC")

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("NAVINSPECT saved")


main()
