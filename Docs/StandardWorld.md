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

## Construction approach

Use a hybrid layout. A small set of structural recipes can produce houses, shops, and repeated tower bays. Landmarks can have their own exterior and interior art. Assign the landmark art to destructible sections with structural connections, so opening a wall changes both what the player sees and what blocks them. The underlying section can be replaced using shared parts when the attachment points line up.

Each generated site has a stable ID, a type, a position, and a proposed `construction` mode. `shared_kit` selects reusable prefab families. `custom_shell_shared_structure` identifies sites whose visual sections are hand-authored. Treat that output as input for editor placement and manual overrides. Later save data can store changes against site and piece IDs while leaving the curated baseline intact.

The playable prototype is the utility canopy east of the Crossroads station. It has one authored descriptor and uses the player-build stone pillars and timber floors. Both its rendering and collision use the chunk system. The permanent blockout buildings and custom landmark sections still need conversion.

## Support rules

- Only finished pieces carry support. Pieces with a full footing on tagged ground start with capacity five for timber or nine for stone. Ground and foundation slabs for authored sites can be marked `StructuralGround`.
- Contact on a top face spends one capacity step. Edge-to-edge contact between floors spends two. A piece takes its best viable path, capped by its own material capacity.
- A destroyed piece triggers reevaluation through its dependents, including across chunk boundaries. An alternate path can keep pieces standing. Unsupported pieces lose collision and collapse, with a cap of twelve short-lived debris actors per event.
