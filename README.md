# Survival Co-op

UE5.8 C++ survival-crafting project inspired by the mechanics and atmosphere of 7 Days to Die A16–A18. Original implementation; working title pending. `Survival` is a provisional code identifier.

## Play on Windows

Download `Survival-Windows-x64.zip` from [Releases](https://github.com/Fussile/Clone/releases) when a playtest is published. Extract the entire ZIP and open `Survival.exe`. GitHub's **Code → Download ZIP** and **Source code (zip)** downloads contain the Unreal project for developers.

The solo playtest starts outside the Southern Crossroads gas station. Use **WASD** and the mouse to move and look, **Space** to jump, **V** to switch to third person, and **E** to search the building materials crate in the shop.

Press **B** to open the build menu, **scroll** to select a piece, and **R** to rotate it. **Left-click** to place a free blueprint. Aim at the blueprint and press **E** to finish it with timber or stone, or **X** to remove it. The HUD shows the total materials required and how many you still need to collect.

## Build a playtest ZIP

On Windows with Unreal Engine 5.8 and its supported C++ toolchain installed, run this from the repository root in PowerShell. Use your own UE 5.8 installation path.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\package_windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

The script builds the editor target, imports the starter art, generates the playtest map if needed, places the crate, and cooks a standalone Windows build. It writes `Dist/Survival-Windows-x64.zip` with app-local prerequisites. Before publishing, extract the ZIP on a Windows PC without Unreal Editor and run the [scavenging and building playtest](Docs/CrossroadsScavenging.md). Attach the verified ZIP to a GitHub Release for the commit you built.

For editor-only work, build and open `Survival.uproject`, then use **File → Execute Python Script** to run `Tools/prepare_crossroads.py`. See the [blockout guide](Docs/CrossroadsBlockout.md) for level editing and the [art guide](Docs/CrossroadsArt.md) for the imported assets.

See [architecture](ARCHITECTURE.md), [decisions](DECISIONS.md), [asset manifest](ASSETS.md), and [M0 checklist](Docs/M0.md). Proposed choices remain provisional until confirmed by the developer.
