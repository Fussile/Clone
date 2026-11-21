# Crossroads region

The first M4 world pass translates the sketch into a 5 km east–west by 4 km north–south region. [The planning map](CrossroadsRegion.svg) shows the anchored locations, the east–west highway, proposed secondary roads, candidate bridge crossings, lakes L1 through L7 and the Great Fissure. The rectangle is 20 km². Mountains, water, the fissure and the radiation corridors reduce the usable land. At the current top walking speed of 6 m/s, a straight 5 km crossing takes about 14 minutes. A 1 km walk takes about three minutes.

The city center reserves plots for a general hospital, police headquarters, civic center, towers, a parking garage and a restaurant and bar strip. Outlying districts include the winter village and northern lakes, the Great Lake and wetlands, the southwest and southeast industrial districts, the airport and the dry southern towns. Site IDs carry through to the baked manifest. Travel routes, bridge sites and plot footprints are provisional so they can be revised as the districts take shape.

## Build the editor blockout

1. Run `python Tools/generate_world.py Saved/WorldBake/world.json --bake` at the project root. The command writes a manifest, a 16-bit Unreal Landscape heightmap and an overview map. Keep the authored edits in `Tools/world_layout.json`. The default seed varies the broad relief and keeps the district anchors.
2. Create an Open World level as `/Game/Maps/L_CrossroadsRegion`. Remove the template Landscape and Player Start from the new level. Keep its sky and lighting. Import `Saved/WorldBake/crossroads-height.png` as a level, unrotated Landscape. Use the X/Y/Z scales from the manifest (about `248.015873 / 198.412698 / 100` cm), 63 quads per section, 2 × 2 sections and 16 × 16 components. Import with the top of the PNG at negative world Y.
3. Load the entire Landscape in the World Partition editor for placement. Use Unreal's **Tools → Execute Python Script** to run `Tools/place_region_blockout.py`, then Save All. It places the first building masses and pads, tags the terrain for the build system, and supplies a Player Start at Southern Crossroads and three regional temperature markers. You can reopen the level and rerun placement with its actor cells loaded. Existing placements keep their edits.
4. Play the new level from its Player Start. Play the station, warehouse, and hospital route as an editor preview. The river channel has a crossing deck. Water rendering, vegetation, and radiation behavior still need a world content pass. The separate Southern Crossroads test map remains the packaged startup map.

## Runtime performance

Use World Partition to load around the player from cooked data. Start with 256 m cells and a 768 m loading range and profile before tuning. Those two values are planning defaults in the manifest. Configure the runtime grid when setting up the level. Give the skyline and other large scenery HLOD proxies. Use instanced meshes for recurring street props and foliage, and keep collision, interiors, loot, navigation, and AI in the nearby cells. Preserve the existing 16 m construction chunks for runtime pieces. They serve a different purpose from the larger World Partition cells.

Keep authoritative loot, destruction and construction changes keyed by stable site, section and piece IDs. A cell unload should preserve these deltas. Cook and test the baseline before implementing save migration or seeded world variants. Profile a drive into downtown, a bridge crossing, several cell unload/reload cycles, and two co-op players traveling apart. Record memory, frame-time spikes and the number of active AI during each route.

## Build a district with shared construction

Use kit walls, floors and stairs where the current grid fits. Give major landmarks their own art and divide each into purpose-built sections, such as a storefront panel, concrete bay or stair core. A section should own its render mesh, collision and health. A broken section must remove its blocking collision and expose attachment points for replacement pieces. Put the structural component in the support graph wherever its neighbors can depend on it. Bridge decks, terrain, and the first skyline blockouts can start as static geometry. The existing utility canopy is the test case for player-kit pieces, projectile damage and collapse.

The warehouse loading bay uses finished player-grid pieces above the tagged slab. Its stone columns support a two-row timber roof. The stone infill behind the canopy is a separate target. The imported warehouse exterior supplies scenery behind the test bay. An authored prefab ID prevents cell re-entry from reseeding the bay during the same session.

## First hospital wing and downtown art

General Hospital has an enterable ground-floor wing with four rooms, a central corridor, textured beds, and a dressing supply crate. The region placement script imports the included bed and places the wing on the hospital pad. Rerun it with the hospital cell loaded to upgrade an earlier blockout.

The downtown nightlife blockout uses four textured window bays assembled from James Ray Cock’s CC0 Modular Urban Apartments Facade. The source GLB is included in the project and the region placement script imports it automatically. The bays provide exterior art while the blockout shell provides collision.

Source and license details are in [DowntownFacadeAttribution.txt](DowntownFacadeAttribution.txt).

## Architectural direction

The region manifest assigns architecture per site. Low-income housing, row villages, farms and manufacturing use late-Soviet forms. Downtown uses modern commercial architecture. Wealthier estates use light stucco and dark metal trim. Middle-income neighborhoods favor brick and hardwood in an early-2000s Eastern European style. Civic buildings use classical porticoes, with marble and fictional regional heraldry for a later detail pass. The generated colors and trim are blockout cues for those styles.

The same station layout is used at each fuel stop. Southwest Apartments has an adjacent panel-housing exterior and a walkable two-story test building. The layouts share a stair landing and vary room entrances, debris, loot positions and walker locations. Placement uses the world seed, site ID and floor index so rebaking the same site is reproducible. [Model credits](M4WorldArt.txt)

## M4 editor playtest

Rebake the layout and run region placement after updating. Load the previous shell actors as well so the script can replace them. From Southern Crossroads, collect the starter sidearm and follow the station road. The east spur leads to the warehouse repair cache and runner encounter. The north leg crosses the river to General Hospital and its dressing supplies.

At the warehouse, damage the two-block infill and place a repair in the gap. Remove an end pillar and check that only the unsupported end of the roof drops. Return after streaming out the warehouse cell and confirm the bay layout is still in its damaged state. Runtime building chunks currently hold changes for the session. Cross-session saves and restoration of looted containers or defeated enemies on stream-in are pending.
