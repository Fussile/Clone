# Crossroads building playtest

For editor play, run `Tools/prepare_crossroads.py` and open Southern Crossroads. The Windows playtest runs preparation as part of packaging.

1. Before looting, press **B** and aim at a clear spot on the forecourt within 5 m. Scroll through the eight shapes and their costs. Select **Timber wall**, press **R** to rotate, and left-click to place a cyan blueprint. Walk through it and check that inventory stays at zero and **Still needed** rises to three timber.
2. Aim at the blueprint within 2.6 m. Press **E** with zero timber and check that it remains a ghost. Place another blueprint a few meters away, then aim at it and press **X**. Check that the ghost disappears and the totals drop by its recipe cost.
3. Enter the shop and press **E** on the default crate. It gives 24 timber and 12 stone once. The total planned cost stays visible and the shortage drops to zero for the remaining wall.
4. Aim at the first wall and press **E**. Verify that the ghost becomes a textured, collidable wall and three timber is deducted. Repeat with a stone piece. Verify that a completed door frame has a walkable opening, that stairs are climbable, and that **X** applies only to blueprints.
5. Try placing another plan in the same space as a ghost and verify the preview rejects it. Try finishing a plan while standing inside it and check that completion waits until you step clear. Verify a solid shop wall blocks interaction with a ghost behind it.
6. Press **V** and verify that the survivor is visible and animates when idle, moving and jumping. Verify aiming at the crate and ghosts works in both views.

The material counts, plans, and completed pieces reset when the solo session restarts.
