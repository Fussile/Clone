# Gas station scavenging playtest

Build SurvivalEditor, restart the editor, and open the existing L_SouthernCrossroads level. Run `Tools/place_crossroads_supplies.py` from File > Execute Python Script, then save the level. You can move or replace the tagged crate in the editor after placement.

Press Play. Walk into the shop, approach the crate, aim at it and press **E**. The supplies counter should move from zero to three. The empty crate should award nothing on a second press. From farther than 2.6 m or through a shop wall, the prompt should disappear.

The supplies counter and searched state reset on each play session. Supplies are a single prototype resource before the inventory and building systems.

The map script skips a crate it has already placed. The original blockout remains editable, including if you built it before this change.
