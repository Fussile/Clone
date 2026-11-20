# Decision register

Source: developer's November 20, 2026 project brief and chat clarification. Proposed defaults are not confirmed requirements.

| ID | Status / owner | Decision |
| --- | --- | --- |
| D001 | Confirmed / developer | UE5.8; C++ gameplay; no Blueprint gameplay logic |
| D002 | Confirmed / developer | Steam; player-hosted co-op; no developer-paid game servers |
| D003 | Confirmed / developer | A16–A18-inspired survival/crafting atmosphere and mechanics; original implementation |
| D004 | Confirmed / developer | Full voxel engine deferred; allow later replacement of world storage |
| D005 | Confirmed / developer | Learn physical skills through practice and intellectual skills through reading |
| D006 | Confirmed / developer | Assemble complex equipment from quality-bearing parts; average/sum contributes to result |
| D007 | Confirmed / developer, supersedes brief sections 4 and 6.8 | CC0 is not required. Assets licensed for commercial game use are eligible |
| D008 | Provisional / agent | Survival is an internal module/project identifier, not a game title |
| D009 | Proposed / brief | Chunked instanced building, local support updates, host authority |
| D010 | Proposed / brief | SteamSockets transport; no host migration |
| D011 | Proposed / agent | First validate local IP networking, then packaged Steam networking |
| D012 | Proposed / agent | Create feature modules as implemented rather than empty modules now |

## Open decisions

- Working title.
- Maximum co-op size and minimum host hardware.
- Target platforms: Windows only, Linux, Steam Deck?
- Handcrafted versus generated production world.
- Day duration and whether seven-day assaults are fixed or configurable.
- Future terrain deformation scope.
- Save ownership, location and schema; whether player progress is world-specific or portable across hosts.
- Approval or revisions of proposed architecture, including material/load-based structural integrity and part aggregation rules.

## Milestones

M0: project builds, minimal C++ character, test map, repeatable two-process host/join test, followed by packaged Steam validation.
M1: grid building, destruction, support rules, bounded chunk replication and stress test.
M2: survival/time, inventory/crafting, one zombie that attacks building pieces.
M3: small playable world, first horde, one assembled rifle, practice skills and books.
M4+: traders, quests, content expansion, art, future terrain work.
