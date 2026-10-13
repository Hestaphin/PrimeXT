# J.10.2 — penetration and PhysX ownership repairs

Baseline: J.10.1 source with I.10.10 physics and J.9 model penetration. No Xash engine changes.

## World/brush exits

A world BSP brush does not have a unique edict: separate brushes share world entity 0. The old long reverse ray could select a later concrete brush as the exit of the entry glass brush, charging only glass resistance. J.10.2 samples BSP point contents along the forward ray, finds the **first contiguous solid-to-empty transition**, refines its position, and reverse-traces only from directly outside that boundary. It requires the reverse hit to match the first boundary, requires a forward-facing exit surface, rejects differing known entry/exit material hashes, and checks that the small post-exit offset remains outside world solid. The same-entity check remains a secondary sanity check, **not** proof of brush identity. Unknown/too-thin/complex geometry fails closed.

Limits: BSP contents sampling is discrete at 0.25 game units (plus boundary bisection). Co-located brushes with no empty gap cannot be uniquely identified through this API; the material mismatch guard rejects detectable exposed material changes, but hidden internal layers cannot be certified by point contents. This is a conservative approximation, not an exact BSP brush-ID system. Non-world brush-model entities use the existing target-only model exit path. Production maps still need multilayer and adjacent-brush tests.

## PhysX target exits

For non-ragdoll custom collision, the query no longer requires a root collision mesh or enabled root bodygroup. The target's active query-enabled child primitive shapes and root shapes are considered separately. Every candidate is tested for a valid *entry* at the original contact point. Selection is resolved **before** any candidate is reverse traced; coincident entry shapes are rejected as ambiguous. The exit reverse ray is run only against the selected exact `PxShape`/pose. If it fails, penetration stops; it never falls back to another nearby shape. Ragdoll exit queries remain bound to the specified body.

## Zero-shape root lifecycle

`ToggleCollision` now still propagates requested state to children with a zero-shape root. `UpdateEntityAABB` can union children without root shapes and does not call `getWorldBounds()` on a shape-less root. `MoveKinematic` sets the root target and collision state even when root geometry is absent; `EnableCollision` similarly toggles child shape flags. Bodygroup updates refresh child flags and bounds in the zero-root case. Root mesh replacement remains unavailable without an actual root shape; this case is handled conservatively.

## Validation

- `ballistic_j10_2_tests.cpp`: disconnected glass/concrete solids, missing/too-thin/world-solid boundaries, first exit, exact entry shape selection and ambiguous overlap.
- `modular_j10_2_source_tests.py`: real integration wiring for world exit, PhysX selection, child actors and root lifecycle.
- Updated old J.9 raycast wiring and I.9.1 flag-count assertions to reflect the new method without allowing invalid PhysX flags.
- Non-PhysX `modular_ballistics.cpp` and `modular_ballistic_exit.cpp` compile at syntax level with PrimeXT's Linux build definitions.

PhysX SDK headers (`PxConfig.h`, `PxPhysicsAPI.h`) are **not** supplied, so the PhysX translation unit cannot be compiled/linked or validated in a running map here. Required follow-up: compile and run with the real SDK, and test ragdolls, primitive doors, zero-root children, touching BSP layers, and oblique/material-boundary penetration.

The unrelated J.10.3+ ammunition, prediction, NPC, death-ragdoll and editor defects are unchanged.
