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
| Defense sidearm | Original blockout made from Unreal Engine basic shapes | Project source and Unreal Engine content terms | No third-party model link required | Source construction code is included | `Source/Survival/Private/SurvivalSidearm.cpp` |

Acquired November 20, 2026. Kenney's model, skin and animation files are unmodified and the Kenney directory includes the original license notice. The ambientCG preview maps were resized from the 1K-JPG releases to 512 px and re-encoded as JPEG for the first blockout. See [ambientCG's license](https://docs.ambientcg.com/license/) for the full terms.

The [Blacktail listing](https://sketchfab.com/3d-models/blacktail-471d33746a374262ae775269ae3997c6) describes the upload as a port of Capcom's model. The project uses the original blockout sidearm pending verification of rights to the underlying model.

This repository is public. Commercial game use alone does not establish permission to publish raw asset files. Check the actual terms before committing; use private delivery and reproducible import instructions when raw redistribution is restricted. Content/Restricted is ignored as a guardrail, not proof of compliance. Git LFS does not make assets private. Store required acknowledgments for inclusion in shipped credits.
