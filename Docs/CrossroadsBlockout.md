# Southern crossroads blockout

This solo test map covers 1 km × 1 km around the southern junction. It uses a flat placeholder floor, the two planned road alignments, and walk-in building shells. The site placements are a curated first pass. The warehouse is nudged east from the standard-world plan to leave room at the map edge.

1. In UE5.8, build SurvivalEditor and open Survival.uproject.
2. The editor imports the art, creates and saves `/Game/Maps/L_SouthernCrossroads` if needed, and places the gas station supply crate during its first launch. Later launches reuse the saved project assets. The Windows packaging script also runs preparation before cooking the game.
3. Press **Play**. You start outside the gas station, facing the shop. Walk into the shop, visit the houses to the northwest and barn to the northeast, then follow the west road toward the warehouse. Check jumping, road transitions, wall collisions, and door openings.
4. Edit the actors directly in the level. Preparation reuses existing levels and the tagged crate. If you author level changes, commit the edited map and any new content through the repository's Git LFS workflow.

The generator uses engine cubes and assigns the [starter materials](CrossroadsArt.md) when it first creates the map. Assign materials directly to actors in a previously authored level if needed.

The generated map is the standalone game's default map. Python runs in the editor during preparation. The native C++ character and game mode run the playtest.
