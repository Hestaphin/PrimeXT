# Milestone J.3 — physical bullets and cartridges (sample mod assets)

J.3 builds on **J.2**, preserving the existing I.9.6 `_cp`/`.phys` nearest-hit collision resolver. The Weapon & Cartridge Editor remains a **standalone application planned for J.5**, and does not run in `pxmv`.

## Installation

Copy the `scripts/` subtree from this example into your game/mod directory (do not copy `README_J2.md`; it describes the previous sample). The paths are read by virtual filesystem manifests, and identical scripts must exist on both client and server. Place a `weapon_modular` entity in a map with `weapon_id = test_pistol`. Add pickups using `ammo_modular` with `cartridge_id = test_fmj` or `test_jhp` and `amount = 25`.

The included models and sounds refer to existing Half-Life resources. The weapon and cartridge names are fictional configuration examples, not measured ballistics.

## Cartridge file (v2)

```
version = 2
id = test_jhp
chamber = 9x19
projectile = jhp
mass_grams = 7.45
diameter_mm = 9.02
drag_coefficient = 0.30
reference_velocity_mps = 345
projectile_count = 1
damage = 18
```

`damage` is the nominal muzzle damage for one projectile, scaled by its remaining kinetic energy at impact. A cartridge v1 remains loadable with fallback damage 20. `drag_coefficient` is a dimensionless quadratic Cd, with standard sea-level density of 1.225 kg/m³ by default. Bullet mass is **not** a multiplier on gravity.

## Weapon definition (v2)

```
accept = test_fmj:360
accept = test_jhp:335
primary_behavior = ballistic
primary_mode = semi
primary_ammo_cost = 1
secondary_behavior = ammo_cycle
secondary_mode = semi
secondary_ammo_cost = 0
```

The `accept` value is weapon-specific muzzle velocity in metres per second. A single cartridge file may be referenced by many weapons, each with a distinct velocity. With `ammo_cycle`, **secondary attack** selects the cartridge **for the next reload**, with center-screen feedback. Existing rounds remain the currently loaded type. Empty your clip before reloading a different cartridge: mixed magazines and partial tactical magazine exchanges aren't implemented yet. Reserve ammo is tracked separately for each cartridge ID, subject to the legacy engine's 32 ammo-slot limit.

The `hitscan` behavior is still available. Set `sv_ballistics_enabled 0` to temporarily use the old instant-hit function for ballistic actions too (physics values will no longer affect the hit in this mode).

## Server tuning & safety

```
sv_ballistics_enabled 1
sv_ballistics_gravity_scale 1
sv_ballistics_air_density 1.225
sv_ballistics_debug 0
```

Ballistics use 1 GoldSrc unit = 1 inch (39.37007874 units/m), meters internally, fixed max integrator step 5 ms, and swept collision between samples. A bullet has a 10-second lifetime, configurable weapon travel-range limit, a minimum-energy cutoff, and an active capacity of 256 projectiles. Each bullet is implemented as an invisible server-side thinker, *not* a simulated PhysX rigid body. J.4 will provide client projectile/tracer replication and complete cartridge reserve-state prediction.

## Known limitations / testing

- Player modular weapons can fire physical bullets. NPC/turret integration is J.4.
- Bullet hits reuse I.9.6 material, `_cp`, `.phys`, hitgroup, decal and PhysX impulse mechanisms. Full runtime behavior requires engine testing.
- Damage uses a tunable game damage value with kinetic-energy falloff, **not** an automatically derived real-world wounding model.
- Secondary-ammo selection and active magazine type are sent through weapon-data `iuser3`/`iuser4`. Only the active reserve ammo count is available to client prediction in the legacy protocol. Switching and reloads are server authoritative; client reconciliation and a dedicated ammo-selection HUD are scheduled for J.4.
- Per-round mixed-cartridge magazines, explicit chamber-only states, projectile save/restore, and HUD display of all reserve types are not yet implemented.
- Engine/game code was not fully linked or exercised in an actual multiplayer/PhysX runtime in the provided environment.

Portable tests:

```
c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp game_shared/ballistic_flight.cpp game_shared/tests/ballistic_flight_test.cpp -o /tmp/ballistic_test
/tmp/ballistic_test documentation/examples/modular-ballistics
```
