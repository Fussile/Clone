# Crossroads starter art

`Tools/prepare_crossroads.py` imports the files from [SourceArt/Crossroads](../SourceArt/Crossroads) into Unreal during preparation. All source paths below are relative to that folder.

| Source | Use |
| --- | --- |
| Asphalt021 | Cracked roads and the gas station forecourt |
| Concrete034 | Foundations, curbs and warehouse walls |
| Ground036 | Dirt shoulders and farm ground |
| Kenney characterMedium | Rigged low-poly placeholder for the player |

## Surfaces

For each surface, import Color, NormalDX and Roughness under `/Game/Art/Crossroads/Surfaces`. Leave sRGB enabled for Color. Use Normalmap compression and disable sRGB for NormalDX. Disable sRGB for Roughness. Connect the textures to Base Color, Normal and Roughness on an opaque material, then apply the materials to the blockout geometry. Adjust UV tiling at world scale when the geometry is in place.

## Survivor

Import `Kenney/Model/characterMedium.fbx` as a skeletal mesh under `/Game/Art/Crossroads/Survivor`. Create a new skeleton on this first import, then reuse that skeleton for the animation FBXs. Import `Kenney/Skins/survivorMaleB.png` and assign it as Base Color on the mesh material. Import the three FBXs in `Kenney/Animations` as animation sequences using the skeleton from the mesh import. Preview each clip on that skeleton before wiring locomotion.

The current first-person pawn has no assigned skeletal mesh asset. Attach the imported mesh to the pawn's existing Character mesh when adding its visual setup. Check floor alignment, facing and camera clipping before showing a full body in first person. The skeletal mesh, materials and animation playback still need UE5.8 editor validation.
