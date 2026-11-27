# Survival Co-op

UE5.8 C++ survival-crafting project inspired by the mechanics and atmosphere of 7 Days to Die A16–A18. Original implementation; working title pending. `Survival` is a provisional code identifier.

## Play on Windows

Download `Survival-Windows-x64.zip` from [Releases](https://github.com/Fussile/Clone/releases) when a playtest is published. Extract the entire ZIP and open `Survival.exe`. GitHub's **Code → Download ZIP** and **Source code (zip)** downloads contain the Unreal project for developers.

Crossroads starts outside the Southern Crossroads gas station. Follow the [region map](Docs/CrossroadsRegion.svg) to the warehouse, the city loop, and General Hospital. Use **WASD** and the mouse to move and look, **Space** to jump, **Left Shift** to sprint, **C** to toggle crouch, **Z** to toggle prone, **V** to switch to third person, and **E** to search the building materials crate in the shop.

In deep water, look and move to swim, hold **Space** to rise, or hold **C** to dive. Watch the **O2** meter when your head goes under. Surfacing restores oxygen. Wet clothing dries gradually on land and increases the calorie cost of cold weather. Search fallen logs and desert cacti for timber, or exposed geodes for stone.

**Salvage and workshop crafting.** Search vehicles and building-supply containers for scrap iron, tin, springs, gears, and salvage tools. Each geode also yields iron or copper ore when first searched in a loot cycle. In **I → Crafting**, make a salvage tool roll and a field crucible. Carry the tool roll to restore 50% more structural health per repair. Refine ore into ingots, cast bronze, then assemble a hand-crank filter pump. The recipe panel shows the tool you need to carry and the ingredients spent per batch.

Drinking bottled water returns an empty bottle when there is room in your pack. Food containers can also hold empties. Stand in a lake or river with your head above water and use an empty bottle from your backpack or toolbelt. Craft **Bottled water** with the pump, one filled bottle, and one cloth per batch. You keep the pump.

Press **B** to open the build menu. Choose timber or stone blocks, stairs, or a door frame. **Left-click** places a blueprint on the 1.05 m grid. Press **E** to finish a blueprint or **X** to remove it. Stairs rise one full cube. A grounded timber wall can carry five cubes out from its upper block. A door frame reserves two stacked cells and mounts on the edge nearest the aim point. Rotate the frame twice with **R** to reverse its swing direction while keeping the same edge. Craft a timber door in the pack and press **E** on the finished frame to install it. Press **E** again to open or close the door.

Search the darker case inside the shop for the sidearm, then move it from the backpack into a toolbelt slot. With the build menu closed, **left-click** fires, **right mouse** aims down sights, and **R** reloads. Rounds damage finished structures. **E** repairs a damaged piece, and **K** dismantles it in build mode.

Press **E** to search a container. Walk away to pause the countdown and press **E** again when you return. Once it opens, drag stacks to organize the loot grid, **Shift**-click to transfer a stack, or press **R** to take everything that fits. Press **I** to open the backpack alongside the container and drag individual stacks into specific slots. Press **I** to open the pack and crafting menu. The pack holds stacks. Drag a stack to move or merge it, and **Ctrl**-drag to split it. **Right-click** uses an item, and **double-click** or **Shift**-click moves an eligible stack into the toolbelt. Drag between the backpack and toolbelt to move or swap stacks, or press **1–6** with a backpack stack selected. Select a slot with a number key or the mouse wheel in play, then left-click to use the selected item. Open the **Crafting** tab to search recipes for bandages, doors, and field dressings. Movement and cold increase food expenditure, and spending stamina increases water loss. A full day takes 60 real minutes. Daylight runs from 04:00 to 22:00, leaving 15 real minutes of darkness. Zombies investigate sight and sound, attack player-built obstacles, and can break through world doors and shutters. The first station horde becomes available at 22:00 on Day 1 when you are near the station. **F5** restarts Crossroads. In a development build, open the console with **~** and enter `SetTimeOfDay 21.5` to jump to 21:30 on the current day.

The dark locker at the back of the shop holds a bolt-action rifle, rifle ammunition and a builder's guide. Move the rifle to the toolbelt and select its slot. **Left-click** fires and cycles the bolt, and **R** reloads from the rifle's separate reserve. Hold **right mouse** for first-person ADS. The pistol aligns both iron sights, and the rifle magnifies the scene inside its scope. The survivor carries the weapon in third person. Spare weapon parts appear in supply caches. Select either weapon and choose **Modify** or press **M**. Drag a matching part onto a slot to install or swap it. Remove a part to return it to the backpack. Every required slot must be filled to aim, fire, or reload. Rifle quality is the average of the installed parts and sets its base damage.

Open **I**, then **Skills**. The catalog uses rank caps and knowledge requirements from the archived [Alpha 16.4 progression data](https://github.com/tassoneroberto/7dtd-assets/blob/main/alpha16.4/Config/progression.xml). Physical, coordination, and basic crafting skills improve through practice. Each character level awards six points to spend in the knowledge trees. Open **Stats** to see rank and progress for skills learned through practice. In **Character**, drag the portrait to rotate it. Read the builder’s guide, scavenger’s field notes, or field dressing schematic from the Backpack or skill detail pane to learn one rank directly. Field Dressings can also be unlocked by spending points in the Survival tree. Planned systems show their requirements in the appropriate tree.

**Caps Lock** toggles a 100 m/s standing movement speed in editor and Development builds. The HUD shows when test speed is enabled.

Loot rolls when the first player finishes searching a container. The **Character** tab shows loot GameStage, which is character level plus completed days survived during the current life, with the day bonus capped at the character's level. Each source has a weighted item pool and a point budget that grows with GameStage. The budget also uses the source's difficulty multiplier and minimum value. The roll spends as close to its target as the available items and slots allow. Weapons and parts use quality **1–999**. Each weapon has its own parts, loaded ammunition, and mods.

| Source | Stage 1 budget | Typical contents |
| --- | ---: | --- |
| Mailbox | 15 | Cloth and reading material |
| Vehicle | 36 | Provisions, medical supplies, and clothing |
| Checkpoint supply crate | 92 | Supplies and pistol parts |
| Police armory | 250 | Commonly two low-quality pistols with supplies |
| Military armory | 120 minimum | A guaranteed pistol, with stronger weapons and parts at higher stages |

Item values, pools, and profile defaults are in `Source/Survival/Private/SurvivalLootBudget.cpp` and `SurvivalGear.h`. A placed crate can raise its minimum with `MinimumLootValue` and change the regional scaling with `LootDifficultyMultiplier`. Authored contents count toward the budget and duplicate limit. A second copy of a non-stackable item rolls at one-eighth weight, regardless of quality. Random loot stops at two copies per item. Precise spending depends on the available loot and grid space.

Press **E** to scavenge containers and props. Destroying a prop forfeits its contents until the next refresh. Leaving and returning to a World Partition cell preserves remaining loot and slot arrangement until the shared server refresh, every three in-game days of active world time by default. Set `LootRefreshDays` in `Config/DefaultGame.ini` to change the interval.

## Build a playtest ZIP

On Windows with Unreal Engine 5.8 and its supported C++ toolchain installed, run this from the repository root in PowerShell. Use your own UE 5.8 installation path.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\package_windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Pass `-Configuration Development` to the packaging script to enable the test speed in a standalone build.

The script builds the editor target, imports the starter art, bakes and saves the region if needed, and cooks a standalone Windows build that opens the region. It writes `Dist/Survival-Windows-x64.zip` with app-local prerequisites. Before publishing, extract the ZIP on a Windows PC without Unreal Editor and run the [region playtest](Docs/StandardWorld.md#m4-editor-playtest). Attach the verified ZIP to a GitHub Release for the commit you built.

For editor-only work, build and open `Survival.uproject`. The first editor launch prepares and opens the region. Wait for `Crossroads: region ready. You can press Play.` in the Output Log on the first launch. Subsequent launches open the saved region. If preparation fails, the editor displays the error and writes the traceback to `Saved/Logs/Survival.log`. The full region retries the next time you open the editor. See the [region guide](Docs/StandardWorld.md) for the route and the [blockout guide](Docs/CrossroadsBlockout.md) for the original defense test map.

To update an existing checkout from a GitHub source ZIP, close Unreal Editor and merge the ZIP into the project root, overwriting matching files. Keep `Content/CitySampleBuildings`, `Content/OvergrownFactoryENV`, and the existing generated content. Rebuild the **Development Editor** target and reopen the project. Crossroads detects the changed bake scripts and refreshes the existing map.

If an incremental bake leaves broken buildings, close Unreal Editor and run this from the project root before reopening the project.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\reset_crossroads_world.ps1
```

The script moves the region map, its World Partition packages, generated world art and bake state into `Saved/CrossroadsWorldBackup-*`. It leaves both Fab packs, the source art and the playtest log in place. Add `-WhatIf` to preview the files it will move. The next editor launch rebuilds the region. To restore hand edits, close the editor, move the newly generated copies aside, and copy the backed-up paths to their original locations.

See [architecture](ARCHITECTURE.md), [decisions](DECISIONS.md), [asset manifest](ASSETS.md), and [M0 checklist](Docs/M0.md). Proposed choices remain provisional until confirmed by the developer.
