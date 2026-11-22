# Survival Co-op

UE5.8 C++ survival-crafting project inspired by the mechanics and atmosphere of 7 Days to Die A16–A18. Original implementation; working title pending. `Survival` is a provisional code identifier.

## Play on Windows

Download `Survival-Windows-x64.zip` from [Releases](https://github.com/Fussile/Clone/releases) when a playtest is published. Extract the entire ZIP and open `Survival.exe`. GitHub's **Code → Download ZIP** and **Source code (zip)** downloads contain the Unreal project for developers.

Crossroads starts outside the Southern Crossroads gas station. Follow the [region map](Docs/CrossroadsRegion.svg) to the warehouse, the city loop, and General Hospital. Use **WASD** and the mouse to move and look, **Space** to jump, **Left Shift** to sprint, **C** to toggle crouch, **Z** to toggle prone, **V** to switch to third person, and **E** to search the building materials crate in the shop.

Press **B** to open the build menu. Choose timber or stone blocks, stairs, or a door frame. **Left-click** places a blueprint on the 1.05 m grid. Press **E** to finish a blueprint or **X** to remove it. Stairs rise one full cube. A grounded timber wall can carry five cubes out from its upper block. A door frame reserves two stacked cells and mounts on the edge nearest the aim point. Rotate the frame twice with **R** to reverse its swing direction while keeping the same edge. Craft a timber door in the pack and press **E** on the finished frame to install it. Press **E** again to open or close the door.

Loot the darker case inside the shop for the sidearm. With the build menu closed, **left-click** fires, **right mouse** aims down sights, and **R** reloads. Rounds damage finished structures. **E** repairs a damaged piece, and **K** dismantles it in build mode.

Scavenge food, water, and cloth from the provisions crates. Press **I** to open the pack and crafting menu. The pack holds stacks. Drag a stack to move or merge it, and **Ctrl**-drag to split it. **Right-click** uses an item, and **double-click** or **Shift**-click assigns it to a hotbar slot. You can also assign the selected item by pressing **1–6**. Select a slot with a number key or the mouse wheel in play, then left-click to use the selected item. Open the **Crafting** tab to search recipes for bandages, doors, and field dressings. Movement and cold increase food expenditure, and spending stamina increases water loss. A full day takes 60 real minutes. Daylight runs from 04:00 to 22:00, leaving 15 real minutes of darkness. Zombies investigate sight and sound, attack player-built obstacles, and can break through world doors and shutters. The first station horde becomes available at 22:00 on Day 1 when you are near the station. **F5** restarts Crossroads. In a development build, open the console with **~** and enter `SetTimeOfDay 21.5` to jump to 21:30 on the current day.

The dark locker at the back of the shop holds a bolt-action rifle, rifle ammunition and a builder's guide. Slot **5** selects the rifle. **Left-click** fires and cycles the bolt, and **R** reloads from the rifle's separate reserve. Hold **right mouse** for first-person ADS. The pistol aligns both iron sights, and the rifle magnifies the scene inside its scope. The survivor carries the weapon in third person. Spare stock, receiver, barrel, and optic upgrades are in supply caches. Open the Backpack, select the rifle and choose **Modify**. Drag a spare part from the pack onto its matching slot to swap it in. The part you replace goes back to your pack with its own quality. Rifle quality is the average of the installed parts and sets its base damage.

Open **I**, then **Skills**. The catalog uses rank caps and knowledge requirements from the archived [Alpha 16.4 progression data](https://github.com/tassoneroberto/7dtd-assets/blob/main/alpha16.4/Config/progression.xml). Physical, coordination, and basic crafting skills improve through practice. Each character level awards six points to spend in the knowledge trees. Use Practice for action skills. Read the builder’s guide, scavenger’s field notes, or field dressing schematic from the Backpack or skill detail pane to learn one rank directly. Field Dressings can also be unlocked by spending points in the Survival tree. Planned systems show their requirements in the appropriate tree.

**Caps Lock** toggles a 100 m/s standing movement speed in editor and Development builds. The HUD shows when test speed is enabled.

## Build a playtest ZIP

On Windows with Unreal Engine 5.8 and its supported C++ toolchain installed, run this from the repository root in PowerShell. Use your own UE 5.8 installation path.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\package_windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Pass `-Configuration Development` to the packaging script to enable the test speed in a standalone build.

The script builds the editor target, imports the starter art, bakes and saves the region if needed, and cooks a standalone Windows build that opens the region. It writes `Dist/Survival-Windows-x64.zip` with app-local prerequisites. Before publishing, extract the ZIP on a Windows PC without Unreal Editor and run the [region playtest](Docs/StandardWorld.md#m4-editor-playtest). Attach the verified ZIP to a GitHub Release for the commit you built.

For editor-only work, build and open `Survival.uproject`. The first editor launch prepares and opens the region. Wait for the map bake to finish before pressing Play. Subsequent launches reuse the saved region. See the [region guide](Docs/StandardWorld.md) for the route and the [blockout guide](Docs/CrossroadsBlockout.md) for the original defense test map.

See [architecture](ARCHITECTURE.md), [decisions](DECISIONS.md), [asset manifest](ASSETS.md), and [M0 checklist](Docs/M0.md). Proposed choices remain provisional until confirmed by the developer.
