# Crossroads starter art

`Tools/prepare_crossroads.py` imports the files from [SourceArt/Crossroads](../SourceArt/Crossroads) into Unreal during preparation. All source paths below are relative to that folder.

| Source | Use |
| --- | --- |
| Asphalt021 | Cracked roads and the gas station forecourt |
| Concrete034 | Foundations, curbs and warehouse walls |
| Ground036 | Dirt shoulders and farm ground |
| Basic Human Male by DNC44 | Rigged third-person survivor |

## Surfaces

For each surface, import Color, NormalDX and Roughness under `/Game/Art/Crossroads/Surfaces`. Leave sRGB enabled for Color. Use Normalmap compression and disable sRGB for NormalDX. Disable sRGB for Roughness. Connect the textures to Base Color, Normal and Roughness on an opaque material, then apply the materials to the blockout geometry. Adjust UV tiling at world scale when the geometry is in place.

## Survivor

Preparation imports `Survivor/SK_Survivor.glb` into `/Game/Art/Crossroads/Player`. The mesh, skin material, and eight locomotion clips share one rig. Press **V** to see the survivor in third person. [Survivor attribution](SurvivorAttribution.txt)

## Building

Preparation generates an original timber plank texture, its instanced construction material, and a translucent blueprint material in `/Game/Art/Crossroads/Building`. Stone pieces reuse `M_Concrete034`. Preparation enables instanced static mesh rendering for all three materials and the packaging configuration cooks their folders.

## Optional medicine sheet for editor playtests

Put the supplied 1215 × 765 sprite sheet at `SourceArt/Local/MedicineSheet.png`
and reopen the editor. The pack and hotbar use the wrapped bandage tile from
the first row. The local sheet stays outside Git and packaged builds use the
project's original bandage sprite.
