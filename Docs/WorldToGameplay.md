# Crossroads world and gameplay handoff

## Structural handoff

Use the northern school-road junction to check the secondary school and stadium. Continue south along the same road to the galleria and dealership. The stadium's authored grass scan was aligned to its painted touchlines. The storefront displays are cropped from the uploaded pack so the gameplay doors have clear openings. The source has three shop fronts. The game uses the stone and metal display bays from two. [Storefronts with opening doors by Jungle Jim](https://sketchfab.com/3d-models/storefronts-with-opening-doors-empty-stores-151b488b26dd4ff4b15c4c03dc0711e6) · [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

Before hand dressing, run a clean editor bake and a second launch on the saved map. Walk from the cordon into a residential street, enter a mall tenant, loop through the school's courtyard and walk both stadium touchlines. Check interiors and collision on a second co-op client. Verify a packaged client streams these same sites. Hand-placed polish should use its own level or independent actors, outside the generated actor tags and reset script's backed-up map.

## Remaining production work

| Stage | Existing foundation | Next playable milestone |
| --- | --- | --- |
| Regional surfaces and foliage | The five-kilometre region blends seven ground surfaces. Birch stands, understory and rock clusters use the same regional data, with roads, sites, water and utility routes reserved. [Coverage preview](CrossroadsBiomes.jpg) | Verify rendering, ground contact and frame times on the airfield–downtown–winter village route in Unreal. |
| Shallow voxel terrain | The terrain importer produces an Unreal Landscape. Construction uses its own building chunks. | Prototype one diggable plot. Measure the bottom from the original local terrain surface, with about 10 m of editable depth above permanent bedrock. Verify shovel and mining hits, collision after edits, saved changes, two-player edits and late join. Align the surface material and foliage masks before replacing the region-wide terrain. |
| Consumable items | Food, water, bandages and field dressings have gameplay definitions. The content branch supplies bottle, energy drink, cigarette, pill and medkit art. | Give individual items inventory, held and loot representations. Add drink, smoke and medicine use clips for first and third person, cancellation, effects and server-side consumption. Source liquor art and verify hand/prop alignment on the player rigs. |
| Collection and crafting | Inventory, loot profiles, progression and timber/stone construction are in place. | Ship one complete salvage loop from an identifiable object to a component, recipe and usable result. Extend it with springs, gears, ores, metal/alloy ingots, abstract powder ingredients and tool-dependent yields. |
| Construction, tools and clothing | Structure placement, support and damage, wearable gear and firearm attachment systems exist. | Add tool actions, resource costs and repairs. Add clothing modification slots with inventory, stat and equipped-mesh updates. Playtest replication, persistence and progression costs together. |

The remaining art gaps most visible on foot are additional house variants, dealership inventory beyond the existing wrecks, school and mall dressing, biome vegetation sets, and consumable hand animations. These can be acquired independently of the shallow-terrain prototype.

## Bake checkpoints

Open the project normally to apply the landscape pass to the structural map. The first bake saves a structural checkpoint before placing vegetation. An interrupted vegetation pass can reuse it. Later gameplay-only art changes reuse both checkpoints when their inputs and required packages still match. A fresh map creates both. The landscape pass preserves the existing imported height field. If upgrading directly from a map baked before the enlarged northern parcels, use the existing world reset during that upgrade to refresh their terrain grades.

The preview shows the generated biome weights, terrain shading, birch positions, roads and reserved sites. It uses map colours. Check materials, leaf opacity, collision and streaming in Play in Editor and on a packaged client. The terrain remains a Landscape. The shallow voxel prototype is the next terrain milestone.
