# Asset manifest and intake policy

Commercial-use licensing is allowed; CC0 is optional. This supersedes the brief's CC0-only wording for art, models, textures, audio and other third-party assets.

The [crossroads source art](SourceArt/Crossroads) is ready to import into Unreal. See the [import guide](Docs/CrossroadsArt.md).

For each asset record: name/version, creator, source URL, exact license/version, acquisition date, retained license evidence, attribution text, modification notes, game redistribution conditions, and permission to redistribute the raw source.

| Asset | Creator / source | License | Attribution | Raw source redistribution | Repository path |
| --- | --- | --- | --- | --- | --- |
| Animated Characters 1 (Survivors), 2019 pack | [Kenney's OpenGameArt upload](https://opengameart.org/content/animated-characters-1) | CC0 1.0 | Optional | Allowed | `SourceArt/Crossroads/Kenney/` |
| Asphalt021 | [ambientCG](https://ambientcg.com/view?id=Asphalt021) | CC0 1.0 | Optional | Allowed | `SourceArt/Crossroads/ambientCG/Asphalt021/` |
| Concrete034 | [ambientCG](https://ambientcg.com/view?id=Concrete034) | CC0 1.0 | Optional | Allowed | `SourceArt/Crossroads/ambientCG/Concrete034/` |
| Ground036 | [ambientCG](https://ambientcg.com/view?id=Ground036) | CC0 1.0 | Optional | Allowed | `SourceArt/Crossroads/ambientCG/Ground036/` |
| Defense sidearm | [9mm Pistol by TORI106](https://sketchfab.com/3d-models/9mm-pistol-43bc09f5aace4346a8a2b6e1580fc03f) | CC BY 4.0 | [Sidearm attribution](Docs/SidearmAttribution.txt) | Allowed with attribution | `SourceArt/Crossroads/Weapons/SM_CrossroadsSidearm.glb` |
| First-person arms and pistol animations | [FPS pistol animations by Cransh](https://sketchfab.com/3d-models/fps-pistol-animations-0d7a343dcb6f401197a73c91aee93f6d) | CC BY 4.0 | [Viewmodel attribution](Docs/SidearmViewmodelAttribution.txt) | Allowed with attribution | `SourceArt/Crossroads/Weapons/fps_pistol_animations.glb` |
| Station zombie | [Zombie (Rigged & Animated) by Aiden Studios](https://sketchfab.com/3d-models/zombie-rigged-animated-131688807ad444609ed9b2ed572dc4aa) | CC BY 4.0 | [Zombie attribution](Docs/ZombieAttribution.txt) | Allowed with attribution | `SourceArt/Crossroads/Enemy/zombie_rigged__animated.glb` |

Acquired November 20, 2026. Kenney's model, skin and animation files are unmodified and the Kenney directory includes the original license notice. The ambientCG preview maps were resized from the 1K-JPG releases to 512 px and re-encoded as JPEG for the first blockout. See [ambientCG's license](https://docs.ambientcg.com/license/) for the full terms.

Crossroads preparation imports the bundled GLB. The model was adapted into one mesh with two material slots. It retains the source triangle counts and includes 1K versions of the six embedded base-color, metallic/roughness, and normal textures. The Windows playtest ZIP includes the attribution notice.

The station zombie source is copied byte-for-byte from the `content` branch. Preparation combines its five skinned parts and imports the original skeleton, textures, and three bone animations. The facial morph tracks are omitted from the generated mesh.

This repository is public. Commercial game use alone does not establish permission to publish raw asset files. Check the actual terms before committing; use private delivery and reproducible import instructions when raw redistribution is restricted. Content/Restricted is ignored as a guardrail, not proof of compliance. Git LFS does not make assets private. Store required acknowledgments for inclusion in shipped credits.
