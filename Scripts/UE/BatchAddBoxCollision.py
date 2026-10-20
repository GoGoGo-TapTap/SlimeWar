"""Batch add box simplified collision to static meshes.

Usage:
  1. Select static mesh assets in the Content Browser, OR select actors in the
     level viewport that use static meshes. Both are picked up automatically.
  2. Console: py "Scripts/UE/BatchAddBoxCollision.py"
     (or Tools -> Execute Python Script... and pick this file.)
  3. Read the Output Log summary. Each modified mesh is saved automatically.

Only meshes with no simple collision get a box. Existing custom collision is
never removed, so re-running the script is safe.
"""

import unreal

# -- config -----------------------------------------------------------------

USE_SELECTION = True

# "auto"   = Content Browser assets first, viewport actors if none are selected
# "assets" = only Content Browser asset selection
# "actors" = only level viewport actor selection
SELECTION_SOURCE = "auto"

# Used only when USE_SELECTION is False.
FOLDER = "/Game/_SlimeWar/Environment"

# Skip meshes that already have simple collision (recommended: True).
SKIP_EXISTING = True

# Save each modified static mesh after adding the box.
SAVE = True

SHAPE = unreal.ScriptCollisionShapeType.BOX


def selected_asset_meshes():
    meshes = []
    for asset in unreal.EditorUtilityLibrary.get_selected_assets():
        if isinstance(asset, unreal.StaticMesh):
            meshes.append(asset)
    return meshes


def selected_actor_meshes():
    """Static mesh assets used by the actors selected in the level viewport."""
    meshes = []
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not actor_subsystem:
        return meshes

    try:
        actors = actor_subsystem.get_selected_level_actors()
    except Exception:
        actors = unreal.EditorLevelLibrary.get_selected_level_actors()

    for actor in actors:
        components = []
        try:
            components = actor.get_components_by_class(unreal.StaticMeshComponent)
        except Exception:
            components = []

        if not components:
            try:
                component = actor.get_component_by_class(unreal.StaticMeshComponent)
            except Exception:
                component = None
            if component:
                components = [component]

        for component in components or []:
            try:
                mesh = component.get_editor_property("static_mesh")
            except Exception:
                mesh = None
            if isinstance(mesh, unreal.StaticMesh):
                meshes.append(mesh)

    return meshes


def unique_meshes(meshes):
    seen = set()
    result = []
    for mesh in meshes:
        key = mesh.get_path_name()
        if key not in seen:
            seen.add(key)
            result.append(mesh)
    return result


def iter_meshes():
    if not USE_SELECTION:
        for path in unreal.EditorAssetLibrary.list_assets(
            FOLDER, recursive=True, include_folder=False
        ):
            mesh = unreal.load_asset(path)
            if isinstance(mesh, unreal.StaticMesh):
                yield mesh
        return

    asset_meshes = []
    if SELECTION_SOURCE in ("auto", "assets"):
        asset_meshes = selected_asset_meshes()

    if SELECTION_SOURCE == "assets" or (
        SELECTION_SOURCE == "auto" and asset_meshes
    ):
        unreal.log(
            "BatchAddBoxCollision selection: {} asset(s) from Content Browser".format(
                len(asset_meshes)
            )
        )
        for mesh in unique_meshes(asset_meshes):
            yield mesh
        return

    actor_meshes = selected_actor_meshes()
    unreal.log(
        "BatchAddBoxCollision selection: {} unique mesh(es) from selected actors".format(
            len(unique_meshes(actor_meshes))
        )
    )
    for mesh in unique_meshes(actor_meshes):
        yield mesh


def main():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if not subsystem:
        unreal.log_error(
            "StaticMeshEditorSubsystem is unavailable. Run this inside the editor "
            "(Tools -> Execute Python Script), not from a commandlet."
        )
        return

    scanned = 0
    added = 0
    skipped = 0
    failed = 0

    for asset in iter_meshes():
        scanned += 1
        path = asset.get_path_name()
        try:
            count = subsystem.get_simple_collision_count(asset)
            if count > 0 and SKIP_EXISTING:
                skipped += 1
                unreal.log("SKIP {} (already has {} simple shapes)".format(path, count))
                continue

            result = subsystem.add_simple_collisions(asset, SHAPE)
            if result > 0:
                added += 1
                unreal.log("ADD {} (+{} box shape)".format(path, result))
                if SAVE:
                    unreal.EditorAssetLibrary.save_loaded_asset(asset)
            else:
                failed += 1
                unreal.log_warning("FAIL {} (no collision added)".format(path))
        except Exception as exc:
            failed += 1
            unreal.log_error("FAIL {}: {}".format(path, exc))

    unreal.log(
        "BatchAddBoxCollision done: scanned={} added={} skipped={} failed={}".format(
            scanned, added, skipped, failed
        )
    )


main()
