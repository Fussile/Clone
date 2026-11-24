"""POI dimensions and baked grades for the airport and community pitch."""
RUNWAY_HALF = 213
RUNWAY_Y = -122
TAXIWAY_Y = -65
APRON_Y = -45
TURN_OFFS = (-175, 175)


def facilities(airport, stadium, height):
    x, y = airport["plot_xy_m"]
    # A single shallow east/west grade ties the runway and aprons together.
    left, right = x-RUNWAY_HALF, x+RUNWAY_HALF

    def terrain_envelope(px):
        return max(height(px, y+dy) for dy in (-140, -122, -105, -80, -60, -40, -29))

    start = terrain_envelope(left)
    slope = (terrain_envelope(right)-start)/(right-left)
    allowance = max(terrain_envelope(px) - (start+(px-left)*slope)
                    for px in range(left, right+1, 3)) + .25
    runway = {"x": x, "y": y, "datum": start + RUNWAY_HALF*slope + allowance, "slope": slope}
    sx, sy = stadium["plot_xy_m"]
    # The source scan has uneven margins. Its walkable base sits above the existing terrain.
    pitch = {"x": sx, "y": sy, "floor": round(max(
        height(sx+dx, sy+dy) for dx in range(-33,34,2) for dy in range(-24,25,2))+.12, 3)}
    return {"airport": runway, "stadium": pitch}
