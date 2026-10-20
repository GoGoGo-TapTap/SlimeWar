import unreal


MAPS = [
    ("/Game/_SlimeWar/Maps/L_Rooftop_Court", [(0.02, 2910.001), (-630, 800)]),
    ("/Game/_SlimeWar/Maps/L_LV2", [(4060, -1260), (-4190, 1420)]),
    ("/Game/_SlimeWar/Maps/L_LV3", [(-4570, -2870), (5190, -2800), (-130, 3710), (1770, 3550), (560, -2920)]),
]


def trace_height(world, x, y):
    start = unreal.Vector(x, y, 3000.0)
    end = unreal.Vector(x, y, -3000.0)
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            world,
            start,
            end,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            True,
            [],
            unreal.DrawDebugTrace.NONE,
            True,
        )
        if hit:
            location = hit.get_editor_property("location")
            actor = hit.get_editor_property("actor")
            unreal.log(
                "  TRACE ({:.1f},{:.1f}) z={:.1f} actor={}".format(
                    x,
                    y,
                    location.z,
                    actor.get_name() if actor else "None",
                )
            )
            return
    except Exception as exc:
        unreal.log("  TRACE failed at ({:.1f},{:.1f}): {}".format(x, y, exc))
        return
    unreal.log("  TRACE ({:.1f},{:.1f}) NO HIT".format(x, y))


for map_path, points in MAPS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    unreal.log("MAP {}".format(map_path))
    for x, y in points:
        trace_height(world, x, y)
