# Gas station scavenging playtest

The Windows playtest starts in L_SouthernCrossroads with the tagged crate already in the shop. For editor play, run `Tools/prepare_crossroads.py` as described in the [blockout guide](CrossroadsBlockout.md). You can move the tagged crate in the editor after placement.

Press Play. Walk into the shop, approach the crate, aim at it and press **E**. The supplies counter should move from zero to three. The empty crate should award nothing on a second press. From farther than 2.6 m or through a shop wall, the prompt should disappear.

The supplies counter and searched state reset on each play session. Supplies are a single prototype resource before the inventory and building systems.

The map script skips a crate it has already placed. The original blockout remains editable, including if you built it before this change.
