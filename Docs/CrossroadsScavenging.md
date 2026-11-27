# Crossroads building playtest

For the original Southern Crossroads test level, follow the [blockout guide](CrossroadsBlockout.md). The Windows playtest runs preparation as part of packaging.

1. Before looting, press **B** and aim at a clear spot on the forecourt within 5 m. Scroll through the building shapes and their costs. Select **Timber block**, and left-click to place a cyan blueprint. Walk through it and check that inventory stays at zero and **Still needed** rises to three timber.
2. Aim at the blueprint within 2.6 m. Press **E** with zero timber and check that it remains a ghost. Place another blueprint a few meters away, then aim at it and press **X**. Check that the ghost disappears and the totals drop by its recipe cost.
3. Enter the shop and press **E** on the default crate. It gives 24 timber and 12 stone once. The total planned cost stays visible and the shortage drops to zero for the remaining wall.
4. Aim at the first wall and press **E**. Verify that the ghost becomes a textured, collidable wall and three timber is deducted. Repeat with a stone piece. Verify that a completed door frame has a walkable opening, that stairs are climbable, and that **X** applies only to blueprints.
5. Try placing another plan in the same space as a ghost and verify the preview rejects it. Try finishing a plan while standing inside it and check that completion waits until you step clear. Verify a solid shop wall blocks interaction with a ghost behind it.
6. Press **V** and verify that the survivor is visible and animates when idle, moving and jumping. Verify aiming at the crate and ghosts works in both views.

## Defend the station

1. Take the sidearm with 12 loaded and 12 reserve rounds from the darker case inside the shop. Press **E** again to verify the case is empty. Check that left-click places plans with the build menu open and fires only after the menu closes.
2. Start the encounter with **G** while the shop entrance is open and head inside. Check that the zombie comes through the entrance. A hit shows a crosshair marker. Enemy durability increases with encounter stage. The sidearm displays in first person and attaches to the survivor's hand in third person.
3. Restart with **F5**. Build a timber wall centered in front of the shop doorway, slightly clear of the permanent front wall. Stand inside and press **G**. Watch the finished piece lose health. Press **E** to repair a damaged piece for one unit of its material. Allow a wall to reach zero and verify the zombie can move through the opening. Repeat with stone to compare its longer survival.
4. Take timber from the cache near the west pump and stone from the cache outside the east wall. Verify adjacent finished pieces keep their health and can still be repaired after one in the same chunk breaks.
5. Toggle third person and check the textured survivor. During preparation, the generated sun and skylight are switched to movable. Check that the generated map no longer shows the unbuilt-light warning.
6. On day one at character level one, press **G** on open ground away from the station. Check that the clock moves to 22:00 and the HUD counts up to four nearby attackers. Press **G** again during the fight to check that groups do not stack. At character level four or day four, expect up to six attackers including a hound.
7. Remove or finish the last plan and check that the requirements disappear from the HUD. Placing another plan should bring them back.
8. Check that the new zombie faces its direction of travel and switches from walking to a swing at a barricade. Watch for wall health to drop during the swing. Step out of reach during the windup and check that the hit can miss.
9. Compare zombie height with the player in idle, walk, and attack. The player and zombie should be roughly the same height. If a [sidearm WAV](../SourceArt/Crossroads/Audio/README.md) was supplied, verify a loaded shot plays once and an empty trigger or blueprint placement stays silent.
10. In first person, verify that the arms and pistol align at idle, while moving and while firing. Spend a few rounds and press **R** to reload. Verify the transfer happens when the reload animation finishes. Repeat from empty to check the longer reload. Toggle **V** mid-reload and verify the ammo transfers once. Open build mode and verify **R** rotates the selected piece.

The material counts, plans, completed pieces, health and loot reset when the solo session restarts.

## Structural playtest

1. Stack stone blocks on the forecourt. Place a block on top of a column and extend the roof by snapping blocks to its side until the next block needs additional support.
2. Aim at the supporting column while in build mode and press **K**. Confirm the roof pieces fall and their original collision disappears. Dismantling spends no materials and gives no refund.
3. Walk east from the station to the authored canopy. Remove one pillar and verify that roof tiles close to the other pillar remain. Remove the remaining pillar and verify that the rest falls, including pieces across the chunk border.
4. Place a plan on supported terrain or on a supporting piece, then dismantle its support before completing the plan. Press **E** and confirm the plan stays unbuilt without spending materials.


## Wall corners, roof edges and projectiles

1. Place a timber block blueprint. Aim at its side to place a neighboring block. Rotate the second shape and verify that the common cell still lines up. Build a corner and check that its faces touch.
2. Aim at the side of an upper block to extend a roof at the same height. Chain another blueprint from its edge and finish the supporting pieces first.
3. Equip the sidearm and fire a round. Press **R** to check the partial reload. Empty the magazine and press **R** again to check the empty reload. Hold **right mouse** to check sight alignment and release to return to hip fire.
4. Close the build menu. Shoot a finished timber piece and check that its health drops on impact. Verify a completed wall stops a round and a blueprint lets it through.

## Doors and the shared grid

1. Verify that the build menu lists timber blocks, stone blocks, stairs, and door frames. Stack two blocks and verify the walls and frame reach the same 2.1 m height.
2. Aim near an edge of a cell and place a thin frame. Rotate it twice while aiming at the same spot. Verify the frame mounts on the same edge and the resulting door swings the other way.
3. Open the pack, filter recipes for **Building**, craft a timber door, then use **E** to install it in the finished frame. Confirm the item is spent only when the leaf can be installed.
4. Press **E** to open or close it. Block its swing and check that it stays in place. Damage or dismantle the leaf and verify that the empty frame can receive a replacement.

## M2 survival loop

1. A new run starts on Day 1 at 08:00. In the development console try `SetTimeOfDay 0`, `SetTimeOfDay 14`, and `SetTimeOfDay 21.5`. Check that the clock, sunlight, and temperature update while the day number stays the same.
2. Scavenge the station provisions and press **I**. Use a mouse or **Tab** and **Enter** to select an item and use it. Assign food to a slot by clicking that slot or pressing **1–6**. Close the menu, select the slot, and left-click to consume it.
3. Compare standing still at midday, standing still at midnight, and repeated sprinting. Food expenditure should rise with movement and cold. Thirst should drain slowly at rest, then accelerate while spending stamina.
4. Search for a bandage, choose a quantity, and craft from the listed cloth requirements. Take damage and use the bandage from the pack or hotbar. Approach the zombie east of the station, break line of sight, and verify it investigates the last known location. Sprint behind a block within hearing range and check that the zombie attacks blocking construction.


## M3 rifle, skills and night

1. Open the dark rifle locker at the back of the station and select slot **5**. Check the restored surface finish in hip and scoped views. Hold right mouse and verify the rifle pulls back below the centered scope. Release to exit ADS. Fire and reload to check bolt and magazine motion. Switch to the pistol, hold right mouse, and verify that both posts are upright and centered. Fire while aiming to check that recoil clears the camera.
2. Open **I**, then **Skills**. Compare Practice with the Scavenging and Survival trees. Read the construction guide or station field notes from the Backpack or skill details. At the house provisions cache, take the dressing schematic. Read it, then craft a field dressing using a bandage and two cloth. The same recipe can be unlocked by spending points in Survival once its prerequisites are met.
3. Use `SetTimeOfDay 21.9` to approach dusk and the first horde. Check that daylight begins at 04:00. A complete day takes 60 real minutes, including 15 minutes of darkness.
4. Select the rifle in the Backpack and note its quality. Loot the house provisions cache for a replacement receiver, then select it and press **Install rifle part**. Confirm the installed receiver, average quality, and base damage increase. Check that ammunition is still present.
