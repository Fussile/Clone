# Standard world

The first planning pass covers 5 km east–west by 4 km north–south. The 100 m planning grid allocates 5 km² to the urban core, 7 km² to the suburban belt and 8 km² to rural land, foothills and the boundary. These are land-use areas. The eastern hazard corridor overlaps them.

| Area | Layout and character |
| --- | --- |
| Center | Closely packed city blocks, towers and apartment districts. Reserve broad arterials and smaller service streets. |
| Suburban belt | Broad tracts of housing broken by commercial strips and arterial roads. Density falls outward. |
| North | Estates, farms, three lake-house villages and a gas-station village at a country-road junction. |
| South | Farms, a gas-station village, a slaughterhouse, warehouses and a derelict factory. |
| West | Foothills rise into the mountain boundary. |
| East | A major highway starts downtown and runs through the suburbs into a bombed-out, irradiated strip near the eastern edge. |

Use the references for scale and mood. Develop original streets, landmarks and assets, with weathered concrete, overgrown asphalt, rusted metal and muted color.

Run `python Tools/generate_world.py standard-layout.json --seed 1729` from the repository root. Coordinates are meters east and north from the southwest corner. Seed 1729 is a provisional starting point. The district footprints and main landmarks are anchored. The seed varies secondary rural sites. Each site has an ID for manual revisions.

Keep the generated baseline and the curated copy separate. For a revision, generate a new candidate file and compare it with the existing edited copy. The JSON is a planning format. Terrain elevation, road meshes, buildings, radiation gameplay and the UE editor import and bake still need implementation.
