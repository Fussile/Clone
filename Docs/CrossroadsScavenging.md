# Gas station scavenging playtest

The Windows playtest starts in L_SouthernCrossroads with the tagged crate already in the shop. For editor play, run `Tools/prepare_crossroads.py` as described in the [blockout guide](CrossroadsBlockout.md). You can move the tagged crate in the editor after placement.

Press Play. Walk into the shop, approach the crate, aim at it and press **E**. The supplies counter should move from zero to three. The empty crate should award nothing on a second press. From farther than 2.6 m or through a shop wall, the prompt should disappear.

## Build a barricade

1. Loot the three supplies and walk outside. Press **B** and aim at level ground a few meters away. A green outline means the wall can be placed for one supply.
2. Left-click to place it, walk into it to check collision, and confirm the HUD reads two supplies. Try building inside that wall or the player capsule. Invalid placement displays in red and costs zero supplies.
3. Aim at the next grid position and place a second wall. Use **R** to rotate the third wall by 90 degrees before placing it on another clear spot. With zero supplies, left-click should place nothing. Press **B** to leave build mode.

The supplies counter, searched state and walls reset on each play session. This pass adds ground-supported wall placement and collision. Stacking, damage, repair and saves are later work.

The map script skips a crate it has already placed. The original blockout remains editable, including if you built it before this change.
