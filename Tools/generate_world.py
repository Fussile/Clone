"""Generate editable planning data for the standard world using Python 3."""
import argparse
import json
import random
from pathlib import Path


def generate(seed):
    rng = random.Random(seed)
    # East and north are positive. Grid cells are 100 m square.
    # Rank by distance from downtown so the footprint stays contiguous.
    cells = [(x, y) for x in range(50) for y in range(40)]
    cells.sort(key=lambda p: ((p[0] - 26)**2 + ((p[1] - 19.5)*1.65)**2, p))
    zones = {}
    for index, (x, y) in enumerate(cells):
        if index < 500:
            zone = "urban"
        elif index < 1200:
            zone = "suburban"
        elif x < 9:
            zone = "western_hills"
        elif y > 19:
            zone = "north_rural"
        else:
            zone = "south_rural"
        zones[x, y] = zone

    roads = [
        {"id": "east-highway", "points_m": [[2650, 2050], [5000, 2050]],
         "irradiated_from_east_m": 4250, "hazard_half_width_m": 150},
        {"id": "north-country-road", "points_m": [[2650, 2050], [2650, 3900]]},
        {"id": "south-country-road", "points_m": [[2650, 2050], [2650, 100]]},
        {"id": "north-crossroad", "points_m": [[1050, 3750], [4000, 3750]]},
        {"id": "south-crossroad", "points_m": [[1050, 350], [4300, 350]]},
    ]
    sites = [
        {"id": "north-services", "kind": "gas_station_village", "xy_m": [2650, 3750]},
        {"id": "south-services", "kind": "gas_station_village", "xy_m": [2650, 350]},
    ]
    for index, x in enumerate((1550, 2150, 3250), 1):
        sites.append({"id": f"lake-village-{index}", "kind": "lake_house_village",
                      "xy_m": [x, 3850], "lake_center_m": [x, 3930], "lake_radius_m": 55})
        sites.append({"id": f"industrial-{index}", "kind": ("slaughterhouse", "warehouse",
                      "abandoned_factory")[index - 1], "xy_m": [x, 250]})

    for side in ("north_rural", "south_rural"):
        candidates = sorted(p for p, zone in zones.items()
                            if zone == side and 5 <= p[1] <= 34 and p[0] < 41
                            and abs(p[0] - 26) > 1)
        for index, (x, y) in enumerate(rng.sample(candidates, 6), 1):
            kind = "estate" if side == "north_rural" and index <= 3 else "farm"
            sites.append({"id": f"{side}-{index}", "kind": kind,
                          "xy_m": [100*x + 50, 100*y + 50]})

    return {"version": 1, "seed": seed, "size_m": [5000, 4000], "cell_m": 100,
            "cells": [{"x": x, "y": y, "zone": zones[x, y]} for x, y in sorted(zones)],
            "roads": roads, "sites": sites}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--seed", type=int, default=1729)
    args = parser.parse_args()
    args.output.write_text(json.dumps(generate(args.seed), indent=2) + "\n", encoding="utf-8")
