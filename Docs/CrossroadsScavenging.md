# Crossroads building playtest

For editor play, run `Tools/prepare_crossroads.py` and open Southern Crossroads. The Windows playtest runs preparation as part of packaging.

1. Before looting, press **B** and aim at a clear spot on the forecourt within 5 m. Scroll through the eight shapes and their costs. Select **Timber wall**, press **R** to rotate, and left-click to place a cyan blueprint. Walk through it and check that inventory stays at zero and **Still needed** rises to three timber.
2. Aim at the blueprint within 2.6 m. Press **E** with zero timber and check that it remains a ghost. Place another blueprint a few meters away, then aim at it and press **X**. Check that the ghost disappears and the totals drop by its recipe cost.
3. Enter the shop and press **E** on the default crate. It gives 24 timber and 12 stone once. The total planned cost stays visible and the shortage drops to zero for the remaining wall.
4. Aim at the first wall and press **E**. Verify that the ghost becomes a textured, collidable wall and three timber is deducted. Repeat with a stone piece. Verify that a completed door frame has a walkable opening, that stairs are climbable, and that **X** applies only to blueprints.
5. Try placing another plan in the same space as a ghost and verify the preview rejects it. Try finishing a plan while standing inside it and check that completion waits until you step clear. Verify a solid shop wall blocks interaction with a ghost behind it.
6. Press **V** and verify that the survivor is visible and animates when idle, moving and jumping. Verify aiming at the crate and ghosts works in both views.

## Defend the station

1. Take the sidearm and 12 rounds from the darker case inside the shop. Press **E** again to verify the case is empty. Check that left-click places plans with the build menu open and fires only after the menu closes.
2. Start the encounter with **G** while the shop entrance is open and head inside. Check that the zombie comes through the entrance. A hit shows a crosshair marker. Three hits defeat the zombie. The sidearm displays in first person and attaches to the survivor's hand in third person.
3. Restart with **F5**. Build a timber wall centered in front of the shop doorway, slightly clear of the permanent front wall. Stand inside and press **G**. Watch the finished piece lose health. Press **E** to repair a damaged piece for one unit of its material. Allow a wall to reach zero and verify the zombie can move through the opening. Repeat with stone to compare its longer survival.
4. Take timber from the cache near the west pump and stone from the cache outside the east wall. Verify adjacent finished pieces keep their health and can still be repaired after one in the same chunk breaks.
5. Toggle third person and check the textured survivor. During preparation, the generated sun and skylight are switched to movable. Check that the generated map no longer shows the unbuilt-light warning.
6. After clearing the first wave, repair and press **G** again. Verify the next two waves have two and three zombies, with six rounds added when each begins. Pressing **G** during a wave leaves it unchanged. After all three waves, check that the HUD reports victory.
7. Remove or finish the last plan and check that the requirements disappear from the HUD. Placing another plan should bring them back.
8. Check that the new zombie faces its direction of travel and switches from walking to a swing at a barricade. Watch for wall health to drop during the swing. Step out of reach during the windup and check that the hit can miss.
9. Compare zombie height with the player in idle, walk, and attack. The player and zombie should be roughly the same height. If a [sidearm WAV](../SourceArt/Crossroads/Audio/README.md) was supplied, verify a loaded shot plays once and an empty trigger or blueprint placement stays silent.

The material counts, plans, completed pieces, health and loot reset when the solo session restarts.
