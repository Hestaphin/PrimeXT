# PrimeXT Milestone J.3 — Ballistics & Cartridge Physics

Baseline: original PrimeXT + Xash3D ZIPs with cumulative J.2 source (which includes I.9.6). J.3 is the third of six phases. Xash3D source remains unchanged.

## Implemented

- Shared SI-unit quadratic-drag/midpoint projectile integrator in `game_shared/ballistic_flight.{h,cpp}`; swept ray trace between each ≤5ms physics sample.
- Server-authoritative invisible projectile entities in `server/modular_ballistics.cpp`, max 256 active, range and energy limits, 10-second projectile lifetime. Bullets use existing I.9.6 nearest-hit resolver for world, `_cp`, physical parts, animated hitboxes, and ragdolls.
- Delayed hit processing uses new per-hit `ClearMultiDamage()` / `TraceAttack()` / `ApplyMultiDamage()` context, existing hitgroup, material, sound/decal and ragdoll/physical-part impulse systems, and damage scaled by remaining impact kinetic energy.
- `ballistic` reusable primary/secondary action; `ammo_cycle` reusable secondary action. Legacy `hitscan` behavior and server cvar hitscan fallback preserved.
- Cartridge schema v2 (`damage`) while keeping v1 compatibility, existing `mass_grams`, `drag_coefficient`, `projectile_count` and per-weapon muzzle velocity overrides.
- Cartridge-specific reserve ammo registration and pickups (no previous legacy-ammo aliasing). Generic weapon default cartridge as ammo identity; alternate cartridge selected for *next* empty-clip reload, without re-labeling loaded ammunition.
- Compatibility/selected/loaded magazine indices synchronized via weapon-data `iuser3` / `iuser4`; shared reload completion hook.
- `sv_ballistics_enabled`, `sv_ballistics_gravity_scale`, `sv_ballistics_air_density`, `sv_ballistics_debug` server cvars, debug impact energy logging.
- Sample mod assets and dependency-free ballistic regression tests.

## Key limitations (DO NOT characterize as fully complete J.3 gameplay)

- No full game/PhysX runtime or engine-level multiplayer test was possible. Syntax tests are **not** runtime proof.
- The legacy client predicts only the active primary reserve-ammo slot. Cartridge switching is server-authoritative but alternate ammo reserves/animation can momentarily diverge in prediction; J.4 protocol work is needed to fully resolve it.
- The current magazine holds one cartridge type, no mixed-round magazine or distinct chamber mechanic. Changing type requires finishing the prior clip; explicit chamber/mixed-round support requires further work.
- This phase does not add NPC/turret weapon assignments, multiplayer visual/tracer replication, or projectile save/restore; those belong to later phases.
- Entity-per-projectile implementation is bounded and adequate for initial testing but should be profiled and potentially replaced by a pooled server manager for high fire rates.
- Projectile type (`fmj`, `jhp`, `ap`, etc.) is a descriptive ID with mass, drag and damage parameters; material-specific penetration/fragmentation algorithms are not yet implemented.

## Source and verification

Incremental patch is generated from exact J.2 source; cumulative patch against original PrimeXT source. Xash3D is unmodified. Patches are checked by applying onto clean copies and comparing changed trees. Actual compile/link and running engine/PhysX collision/gameplay remains unverified. See `documentation/examples/modular-ballistics/README_J3.md` for setup and test commands.
