# PrimeXT Milestone J.9 — Model, Studio and Physics Penetration

**Baseline:** J.8 merged source, retaining PrimeXT I.10.10 physics and standalone Weapon/Cartridge Editor.

## Implementation

- New `server/modular_ballistic_exit.{h,cpp}`: target-only entry/exit validation, outward backface, thickness, hitgroup and world-occlusion safeguards.
- New `game_shared/ballistic_exit_geometry.h`: portable bounds/facing exit checks.
- `server/modular_ballistics.cpp`: penetration candidate now captured **before** applying damage; successful exit commits lower velocity, extra material travel time and recached trajectory. Material defaults for living tissue/unlabelled Studio props adjusted. Existing brush reverse trace retained.
- `server/physic.h`, `server/world.cpp`, `server/physx/physx_impl.{h,cpp}`, `server/physx/physics_asset.{h,cpp}`: optional target-only PhysX body/part/root geometry-exit method with null-physics implementation. No scene-wide guessing of other limbs as exit; v2 root collision and v3/v4 part shapes supported.
- `server/CMakeLists.txt`: builds the new server source.
- New portable regression and source-integrity tests plus `documentation/Milestone_J.9_model_penetration.md`.
- Xash3D source unchanged; the model trace uses Xash's existing targeted `TRACE_MODEL` API.

## What happens when a bullet reaches a surface

1. Existing I.10.10 closest collision supplies entity, hitgroup, exact `.phys` ragdoll body when applicable.
2. If enabled, J.9 locates a back surface in the same target and shape/body (or the same Studio hitgroup for characters). It rejects uncertain geometry, invalid normals, long/no exits or world obstruction.
3. The entry target takes damage and receives the usual effects/impulse, regardless of penetration result.
4. The material energy test decides whether to stop. For success, subtract energy, compute exit velocity, advance through material and rebuild the remaining precomputed trajectory. The shot cannot exceed J.8's four-penetration cap.

## Important I.10.10 `_cp` detail

Old sibling `_cp.mdl` runtime loading was replaced by `.phys` v2/v3 sections in the I.10.x physics branch. `pxmv` imports legacy `_cp.mdl` and saves those sections to `.phys`; J.9 supports penetration of those **runtime `.phys` proxy sections**, not an extra `_cp.mdl` runtime model.

## Validation and caveats

- Portable J.9 geometry/energy tests: PASS.
- J.9 source wiring tests: PASS.
- J.7/J.8 source compatibility and J.8 portable flight/penetration: PASS.
- I.10.x source regressions: PASS.
- Non-PhysX `server/world.cpp`, `server/modular_ballistics.cpp`, `server/modular_ballistic_exit.cpp` GNU C++17 syntax: PASS.
- Standalone editor, after compiling native validator: 13/13 tests PASS.
- PhysX translation units, full game linking, real gameplay traces/penetration/multiplayer: NOT VERIFIED (missing complete third-party SDK checkout).
- The living Studio path uses same **hitgroup** in forward/reverse traces, not a unique Studio hitbox index; overlapping/disconnected same-hitgroup hitboxes can yield ambiguous thickness. The system fails closed for many suspicious cases, but this limitation requires in-game QA or future hitbox-index engine support.
- Penetration resistance is a simplified gameplay energy budget, not physically validated terminal ballistics; no ricochet/deformation/armor-layer simulation.

## Acceptance tests needed in game

1. Standing and running NPC: front chest entry/back exit, hitgroup damage once, subsequent target damage if sufficient energy.
2. A living NPC killed by entry shot: geometry must be captured before converting to ragdoll; impulse persists.
3. Ragdoll with forearm/chest overlap: exit must be from the exact body index, never the next limb.
4. Animated proxy door, static imported legacy `_cp` proxy converted to `.phys`, v2 rigid root, v3/v4 moving bone/part: correct shape exit or conservative stop.
5. Wall immediately behind a character, thin wood/steel/concrete, grazing impacts and shot through multiple materials: no wall bypass, no repeated entry damage.
6. Test in multiplayer and under sustained automatic/shotgun firing; compare CPU time and restore mid-flight.

## Application

- Apply `PrimeXT_Xash_milestoneJ.9_incremental_from_J.8.patch` to J.8, **or** apply `PrimeXT_Xash_milestoneJ.9_combined.patch` to the original two source ZIP trees.
- A complete merged source ZIP is included as an alternative. Do not stack the cumulative patch over an already patched tree.
