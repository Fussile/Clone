# Crossroads building playtest

For editor play, run `Tools/prepare_crossroads.py` and open Southern Crossroads. The Windows playtest runs preparation as part of packaging.

1. Before looting, press **B** and aim at a clear spot on the forecourt within 5 m. Scroll through the building shapes and their costs. Select **Timber block**, and left-click to place a cyan blueprint. Walk through it and check that inventory stays at zero and **Still needed** rises to three timber.
2. Aim at the blueprint within 2.6 m. Press **E** with zero timber and check that it remains a ghost. Place another blueprint a few meters away, then aim at it and press **X**. Check that the ghost disappears and the totals drop by its recipe cost.
3. Enter the shop and press **E** on the default crate. It gives 24 timber and 12 stone once. The total planned cost stays visible and the shortage drops to zero for the remaining wall.
4. Aim at the first wall and press **E**. Verify that the ghost becomes a textured, collidable wall and three timber is deducted. Repeat with a stone piece. Verify that a completed door frame has a walkable opening, that stairs are climbable, and that **X** applies only to blueprints.
5. Try placing another plan in the same space as a ghost and verify the preview rejects it. Try finishing a plan while standing inside it and check that completion waits until you step clear. Verify a solid shop wall blocks interaction with a ghost behind it.
6. Press **V** and verify that the survivor is visible and animates when idle, moving and jumping. Verify aiming at the crate and ghosts works in both views.

## Defend the station

1. Take the sidearm with 12 loaded and 12 reserve rounds from the darker case inside the shop. Press **E** again to verify the case is empty. Check that left-click places plans with the build menu open and fires only after the menu closes.
2. Start the encounter with **G** while the shop entrance is open and head inside. Check that the zombie comes through the entrance. A hit shows a crosshair marker. Three hits defeat the zombie. The sidearm displays in first person and attaches to the survivor's hand in third person.
3. Restart with **F5**. Build a timber wall centered in front of the shop doorway, slightly clear of the permanent front wall. Stand inside and press **G**. Watch the finished piece lose health. Press **E** to repair a damaged piece for one unit of its material. Allow a wall to reach zero and verify the zombie can move through the opening. Repeat with stone to compare its longer survival.
4. Take timber from the cache near the west pump and stone from the cache outside the east wall. Verify adjacent finished pieces keep their health and can still be repaired after one in the same chunk breaks.
5. Toggle third person and check the textured survivor. During preparation, the generated sun and skylight are switched to movable. Check that the generated map no longer shows the unbuilt-light warning.
6. After clearing the first wave, repair and press **G** again. Verify the next two waves have two and three zombies, with six rounds added to reserve when each begins. Pressing **G** during a wave leaves it unchanged. After all three waves, check that the HUD reports victory.
7. Remove or finish the last plan and check that the requirements disappear from the HUD. Placing another plan should bring them back.
8. Check that the new zombie faces its direction of travel and switches from walking to a swing at a barricade. Watch for wall health to drop during the swing. Step out of reach during the windup and check that the hit can miss.
9. Compare zombie height with the player in idle, walk, and attack. The player and zombie should be roughly the same height. If a [sidearm WAV](../SourceArt/Crossroads/Audio/README.md) was supplied, verify a loaded shot plays once and an empty trigger or blueprint placement stays silent.
10. In first person, verify that the arms and pistol align at idle, while moving and while firing. Spend a few rounds, start wave two, and press **R** to reload. Verify the transfer happens when the reload animation finishes. Repeat from empty to check the longer reload. Toggle **V** mid-reload and verify the ammo transfers once. Open build mode and verify **R** rotates the selected piece.

The material counts, plans, completed pieces, health and loot reset when the solo session restarts.

## Structural playtest

1. Build a stone pillar on the forecourt. Select **Floor**, aim at the pillar side to snap to its top, and finish the floor. Aim at the edge of that floor to extend the roof until the preview rejects another tile.
2. Aim at the pillar while in build mode and press **K**. Confirm the roof pieces fall and their original collision disappears. Dismantling spends no materials and gives no refund.
3. Walk east from the station to the authored canopy. Remove one pillar and verify that roof tiles close to the other pillar remain. Remove the remaining pillar and verify that the rest falls, including pieces across the chunk border.
4. Place a plan on supported terrain or on a supporting piece, then dismantle its support before completing the plan. Press **E** and confirm the plan stays unbuilt without spending materials.


## Wall corners, roof edges and projectiles

1. Place a timber block blueprint. Aim at its side to place a neighboring block. Rotate the second shape and verify that the common cell still lines up. Build a corner and check that its faces touch.
2. Aim at the inside face of a block with **Floor** selected. The panel sits in the adjacent cell with both top surfaces at the same height. Chain another roof blueprint from its exposed edge and finish the supporting pieces first.
3. Equip the sidearm and fire a round. Press **R** to check the partial reload. Empty the magazine and press **R** again to check the empty reload. Hold **right mouse** to check sight alignment and release to return to hip fire.
4. Close the build menu. Shoot a finished timber piece and check that its health drops on impact. Verify a completed wall stops a round and a blueprint lets it through.

## M2 survival loop

1. A new run starts on Day 1 at 08:00. Watch the clock and sun advance. One full day takes 20 real minutes.
2. Scavenge the provisions crate in the station and press **I**. Verify food, water and cloth are added only once. Sprint until exhausted, recover stamina, and use **1** and **2** as hunger and thirst drop.
3. With the inventory open, press **4** to turn two cloth into a bandage. Take damage and press **3** to heal. Verify an empty meter eventually costs health.
4. Approach the lone zombie east of the station. Retreat behind a finished block and verify the zombie attacks it. The **G** defense encounter remains available separately.
