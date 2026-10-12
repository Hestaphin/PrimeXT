# PrimeXT Milestone J.7 — I.10.10 physics + J.6 modular ballistics

## Baselines

- I.10.10 combined patch: latest supplied physics baseline (from original PrimeXT + Xash source archives).
- J.6 combined patch: earlier I.9.6 physics + modular weapons, cartridges, ballistics, NPC adapter, prediction hooks, persistence, and standalone editor.
- J.7 is constructed by adding only the J.6-versus-I.9.6 delta to I.10.10. The distinct modified file sets allow a clean merge.

## New modular source files

| Area | Files |
| --- | --- |
| Shared data/validation | `game_shared/modular_weapons.h/.cpp` |
| Ballistic math | `game_shared/ballistic_flight.h/.cpp` |
| Generic action context | `game_shared/weapons/modular.h/.cpp` |
| Server projectile entity | `server/modular_ballistics.h/.cpp` |
| NPC firing adapter | `server/modular_npc.h/.cpp` |
| Generic weapon/ammo entities | `server/weapons/weapon_modular.cpp` |
| Standalone authoring tool | `tools/weapon_editor/` (not pxmv) |

Small integrations remain in legacy weapon registration, inventory, client prediction and NPC entry points, plus CMake source manifests. I.10.10's PhysX and pxmv source files remain unchanged by J.7.

## Applying patches

- Incremental J.7 patch: apply to **I.10.10**, not to J.6.
- Cumulative J.7 patch: apply to the **original two source ZIPs** only.
- Do not apply both patches to the same source tree.
- No Xash3D source changes were required to merge the existing protocol-limited implementation.

## Remaining limitations

Source merge, portable tests and translation-unit syntax checks cannot replace full PhysX linking, engine startup, actual traces in maps, multiplayer prediction or save/restore testing. The modular runtime retains legacy weapon/ammo protocol capacity limits, and NPC integration remains limited. J.7 does not claim to fix all limitations previously documented in J.6.
