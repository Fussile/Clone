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
| D009 | Confirmed / developer | Start with grid-snapped, chunked building. Instancing, authority and support solver details remain implementation proposals |
| D010 | Proposed / brief | SteamSockets transport; no host migration |
| D011 | Proposed / agent | When co-op integration starts, validate local IP networking before packaged Steam networking |
| D012 | Proposed / agent | Create feature modules as implemented rather than empty modules now |
| D013 | Confirmed / developer | Windows initially. Target 4–8 players including the host |
| D014 | Confirmed / developer | Generate one standard world and edit it by hand before shipping. Player-selectable procedural worlds are deferred |
| D015 | Confirmed / developer | Build and iterate on the first playable slice in single-player. Integrate multiplayer later |

## Open decisions

- Working title.
- Minimum host hardware and performance budgets at 4 and 8 players.
- Day duration and whether seven-day assaults are fixed or configurable.
- Future terrain deformation scope.
- Save ownership, location and schema; whether player progress is world-specific or portable across hosts.
- Open implementation details include material/load-based structural integrity and part aggregation rules.

## Milestones

M0: project builds, minimal C++ character, editable southern crossroads blockout and solo movement playtest.
M1: grid building, destruction, support rules, and a local stress test.
M2: survival/time, inventory/crafting, one zombie that attacks building pieces.
M3: small playable world, first horde, one assembled rifle, practice skills and books.
Later co-op milestone: add gameplay replication, session handling, two-process host/join and packaged Steam validation.

M4+: traders, quests, content expansion, art, future terrain work.
