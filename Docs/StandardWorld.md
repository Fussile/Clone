# Crossroads region

The first M4 world pass translates the sketch into a 5 km east–west by 4 km north–south region. [The planning map](CrossroadsRegion.svg) shows the anchored locations, the east–west highway, proposed secondary roads, candidate bridge crossings, lakes L1 through L7 and the Great Fissure. The rectangle is 20 km². Mountains, water, the fissure and the radiation corridors reduce the usable land. At the current top walking speed of 6 m/s, a straight 5 km crossing takes about 14 minutes. A 1 km walk takes about three minutes.

The city center reserves plots for a general hospital, police headquarters, civic center, towers, a parking garage and a restaurant and bar strip. Outlying districts include the winter village and northern lakes, the Great Lake and wetlands, the southwest and southeast industrial districts, the airport and the dry southern towns. Site IDs carry through to the baked manifest. Travel routes, bridge sites and plot footprints are provisional so they can be revised as the districts take shape.

## Open the region

Build the UE 5.8 editor target and open `Survival.uproject`. On the first launch, Crossroads prepares its assets, bakes the terrain from `Tools/world_layout.json`, creates the World Partition map at `/Game/Maps/L_CrossroadsRelief`, and saves the roads, buildings, and external actors. Press Play once the editor logs that the region was stamped. The Windows packaging script runs the same preparation before cooking. The previous `L_Crossroads` bake can still be opened in the editor. The relief revision uses a new map asset so existing map edits are preserved.

The lowlands sit in a broad basin. The northern plain gains elevation toward a steep escarpment, with individual hills at the green markers from the map sketch. Most of the south stays nearly level, with cuts at the Great Fissure and in front of the military depot.

The revised road sketch is the basis for the regional highways and arterials. Downtown has an inner loop with west and east highway connections. Two elevated branches end at missing spans and one remains open. Short approaches connect the station, warehouse and General Hospital. Add local streets and alleys when the actual blocks and building entrances are laid out.

To export just the overview and the Landscape heightmap for editing, run `python Tools/generate_world.py Saved/WorldBake/world.json --bake` from the project root. The normal first launch needs no separate landscape import. Water rendering, vegetation, and radiation gameplay still need a world content pass.

## Runtime performance

Use World Partition to load around the player from cooked data. Start profiling with 256 m cells and a 768 m loading range. Those two values are planning defaults in the manifest. Configure the runtime grid when setting up the level. Give the skyline and other large scenery HLOD proxies. Use instanced meshes for recurring street props and foliage, and keep collision, interiors, loot, navigation, and AI in the nearby cells. Preserve the existing 16 m construction chunks for runtime pieces. They serve a different purpose from the larger World Partition cells.

Keep authoritative loot, destruction and construction changes keyed by stable site, section and piece IDs. A cell unload should preserve these deltas. Cook and test the baseline before implementing save migration or seeded world variants. Profile a drive into downtown, a bridge crossing, several cell unload/reload cycles, and two co-op players traveling apart. Record memory, frame-time spikes and the number of active AI during each route.

## Southern route and downtown stops

The gas stations share a forecourt kit with pump islands and protective bollards. The Warehouse District uses the original textured shell with twelve operable openings, including the modeled loading shutters and the staff entrance. The loading yard has marked lanes, pallets and cargo stacks. Inside are two supply caches. The warehouse walls are fixed scenery with mesh collision. The player construction system still uses its own structural rules. [Model credits](M4WorldArt.txt)

The northwest downtown access off the city loop leads to Crossroads Police, with a reception area, records room, medical supplies and two encounters. The highway frontage approaches reach two other stops. Freight Bar and Crossroads Diner have separate entrances, dining areas, local loot and walkers. The parking garage has a driveable ramp to the open upper deck and supply cases on both levels.

General Hospital has an accessible triage wing, four treatment rooms, textured beds, furnishings and dressing supplies. Its approach joins the hospital arterial. The downtown venues use James Ray Cock’s CC0 Modular Urban Apartments Facade for window detailing. [Facade credits](DowntownFacadeAttribution.txt)

## Regional buildings and doors

Every site with a building footprint now has an enterable layout. Cabins and farmhouses use split bedrooms and farm storage. Shops have stockrooms and counters. Clinics have treatment rooms, civic buildings have separate wings, and factory and utility sites have service rooms. Office and apartment blocks have usable upper floors and connected stairwell ramps. Three landmarks in the manifest describe terrain and do not receive buildings.

Aim at a door leaf or its hinge-side jamb and press **E**. Building entrances, interior rooms and utility shutters use the same interaction. A door stays put if a player or enemy is in its path. World doors belong to the fixed environment and their open state is shared by connected players. Player-built doors still use the construction system.

## Architectural direction

The region manifest assigns architecture per site. Low-income housing, row villages, farms and manufacturing use late-Soviet forms. Downtown uses modern commercial architecture. Wealthier estates use light stucco and dark metal trim. Middle-income neighborhoods favor brick and hardwood in an early-2000s Eastern European style. Civic buildings use classical porticoes, with marble and fictional regional heraldry for a later detail pass. The generated colors and trim are blockout cues for those styles.

The same station layout is used at each fuel stop. Southwest Apartments has a two-story residential building and a separate walkable apartment block. The layouts share a stair landing and vary room entrances, debris, loot positions and walker locations. Placement uses the world seed, site ID and floor index so rebaking the same site is reproducible. [Model credits](M4WorldArt.txt)

## M4 editor playtest

Open the generated Crossroads region and play from its Player Start. From Southern Crossroads, collect the starter sidearm and follow the station approach. The industrial spur leads to the warehouse repair cache and runner encounter. Follow the northbound arterial to the highway and the city loop for the hospital road and dressing supplies.

At the warehouse, check both entrances and the supply crates inside. From the highway, take the parking and nightlife approaches. Walk up the parking ramp, then visit the bar and diner. Use the downtown loop to reach the police precinct and check the rear records room.

### Airport and community pitch

The southeast airport has a 426 m runway at map scale with the uploaded threshold and centerline markings, a parallel taxiway, two turnoffs and a terminal apron. The airport road reaches the landside entrance. A walkable concourse and control room sit beside the uploaded damaged terminal wing and ruined shop. Hangars at the airport and the southern hangar site use the uploaded interior mesh, with operable shutters at the modeled opening. The existing terrain remains the base for the new local pavement grades.

The northern community pitch replaces the stadium placeholder. Take the local approach from the winter crossroad to reach the uploaded field scan and entrance sign. [Model credits](M4WorldArt.txt)
