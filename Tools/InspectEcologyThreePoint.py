"""Read-only inspection for the M9 three-point map."""

import unreal


MAP_PACKAGE = "/Game/_SlimeWar/Maps/L_Ecology_ThreePoint"


def main() -> None:
    unreal.EditorLevelLibrary.load_level(MAP_PACKAGE)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not actor:
            continue
        label = actor.get_actor_label()
        class_name = actor.get_class().get_name() if actor.get_class() else "None"
        location = actor.get_actor_location()
        scale = actor.get_actor_scale3d()
        origin, extent = actor.get_actor_bounds(False)
        if label.startswith(("Floor", "Wall", "Divider", "Column", "Shelf",
                             "NavMesh", "Site", "Mineral", "PlayerStart", "Recast")):
            unreal.log_warning(
                f"INSPECT3 {label} class={class_name} loc={location} scale={scale} "
                f"boundsOrigin={origin} boundsExtent={extent}"
            )
            if isinstance(actor, unreal.StaticMeshActor):
                component = actor.static_mesh_component
                unreal.log_warning(
                    f"INSPECT3 {label} mesh={component.get_editor_property('static_mesh')} "
                    f"collision={component.get_collision_enabled()} "
                    f"profile={component.get_collision_profile_name()} "
                    f"mobility={component.get_editor_property('mobility')}"
                )
            if isinstance(actor, unreal.RecastNavMesh):
                for property_name in ("runtime_generation", "force_rebuild_on_load"):
                    try:
                        value = actor.get_editor_property(property_name)
                    except Exception as exc:
                        value = f"<{exc}>"
                    unreal.log_warning(
                        f"INSPECT3 {label} {property_name}={value}"
                    )


main()
