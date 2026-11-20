# Architecture

Status: proposed architecture, not approval of every Proposed item in the supplied brief.

## Requirements

UE5.8, C++ gameplay, Steam distribution, player-hosted co-op without developer-operated game servers. Original systems inspired by A16–A18. Full voxel terrain is deferred. Physical skills advance through use; knowledge comes from reading. Complex equipment is assembled from parts whose quality contributes to the result. Commercial-use assets are allowed subject to their actual licenses.

## Repository and module boundaries

Only the Survival composition module exists today. Add modules when their first system lands; avoid empty interdependent modules.

| Planned module | Responsibility | Project dependencies |
| --- | --- | --- |
| SurvivalCore | Stable IDs, shared value types, world-query contracts | None |
| SurvivalItems | Item/part instances, inventory, recipes, aggregation | Core |
| SurvivalBuilding | Chunk storage, grid edits, support solver, instancing | Core, Items |
| SurvivalSystems | Attributes, time, practice XP, knowledge | Core, Items |
| SurvivalAI | Budgeted zombies, threat and assault scheduling | Core; world queries rather than building internals |
| SurvivalNet | Session lifecycle, identity, snapshot orchestration | Core |
| Survival | Character, game modes, composition, UI and system wiring | Required feature modules |

Use Source/<Module>/Public and Private; Config for configuration, Content for distributable assets, Docs for implementation/testing notes. Tests live with their owning module. Keep mutable world state separate from asset definitions and rendering indices.

## World and building proposal

Use a small map for movement testing. For production, generate one standard world in the editor from a recorded seed and generator version, then bake it into editable level content. Keep hand-authored overrides separate from generated output so selectively regenerating an area cannot silently discard edits. Ship the curated result as the standard world. Defer player-facing world generation. Landscape/World Partition remains a candidate for streaming. Query contracts should cover ground support, obstruction and damageable targets without exposing terrain storage.

Building pieces use stable IDs in chunk managers, and instanced rendering. Do not use one actor per piece. Translate instance hit indices into stable IDs before gameplay operations. Validate placement distance, resources and occupancy in shared gameplay code.

Connectivity is only the first support check: a grounded path alone permits infinite cantilevers. Define load/span/material rules before calling structural integrity complete. Incremental processing still needs a bounded queue and a defined pending state when a removed support affects a large structure.

## Authority and persistence proposal

For the solo slice, gameplay calls the state-changing functions directly. Keep inventory and building rules in gameplay code that the future host can call. During co-op integration, the listen host validates gameplay changes; clients send intents. CharacterMovement provides the initial movement replication baseline. Building snapshots carry a revision, with queued deltas applied after that revision; cap snapshot bandwidth and test late joining during edits.

Keep platform transport outside gameplay. When multiplayer work begins, start with local IP/LAN tests, then separately validate SteamSockets sessions on packaged builds. No host migration is proposed. A host-owned save keyed by world ID and verified player identity is the simplest default, but cross-host inventory transfer is a separate unresolved design choice.

## Items and progression proposal

Use data assets for definitions and explicit schemas for instances. For later co-op, replicate part quality, durability and rolled modifiers, plus definition/version IDs. Deterministic recomputation requires identical content versions, canonical part order and defined rounding. Avoid client-authoritative quality or XP.

Average/sum quality is required intent; weighted per-stat formulas are proposals. Minimum reliability and minimum jam probability have opposite meanings: normalize favorable/unfavorable stats before selecting aggregation rules.

## Performance and scope

Measure host frame time, AI update cost, active zombies, replication bandwidth, building edits and navigation rebuild cost. No numeric shipping budgets are fixed until host hardware is chosen and measured at four and eight players. Defer Mass/Iris and production world streaming decisions until a measured need and UE5.8 validation exist.
