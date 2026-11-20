# Southern crossroads blockout

This solo test map covers 1 km × 1 km around the southern junction. It uses a flat placeholder floor, the two planned road alignments, and walk-in building shells. The site placements are a curated first pass. The warehouse is nudged east from the standard-world plan to leave room at the map edge.

1. In UE5.8, enable **Python Editor Script Plugin** in Edit > Plugins if needed and restart. Save any open level before running the generator.
2. Choose **File > Execute Python Script** and open `Tools/build_crossroads.py` from this checkout. It creates and saves `/Game/Maps/L_SouthernCrossroads`.
3. Press **Play**. You start outside the gas station, facing the shop. Walk into the shop, visit the houses to the northwest and barn to the northeast, then follow the west road toward the warehouse. Check jumping, road transitions, wall collisions, and door openings.
4. Edit the actors directly in the level. The script refuses to overwrite an existing map. Commit the generated `Content/Maps/L_SouthernCrossroads.umap` through the repository's Git LFS workflow when ready.

The prototype uses engine cubes and works without the starter art imports. To apply optional surface materials on generation, first import the textures using the [art guide](CrossroadsArt.md) and save the materials as `M_Asphalt021`, `M_Concrete034`, and `M_Ground036` under `/Game/Art/Crossroads/Surfaces`. If the map already exists, assign the materials to its actors in the editor.

Python is used only to author the level. The native C++ character and game mode run the playtest. The generated map is separate from the project's default startup flow.
