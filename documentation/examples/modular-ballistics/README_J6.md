# Milestone J.6 — compatibility and validation notes

J.6 is the final integration/validation phase of the modular weapon prototype.
It **does not** certify the PhysX-enabled game build or actual multiplayer runtime.

## Source changes and bug repairs

* The modular registry now checks admission against existing legacy ammo names,
  available ammo slots and occupied weapon IDs before precaching any modular
  weapons. Case-insensitive ammo collisions are forbidden (legacy ammo lookup
  would otherwise alias two distinct cartridges). Admission failure invalidates
  the modular registry and prevents a false valid fingerprint.
* Shared cartridge reserve limits are computed across all compatible modular
  weapons rather than depending on whichever weapon appears first in the list.
* Ballistic sweeps restore exactly the previous global `trace_flags` bitmask;
  previously they cleared flags that might have been set by another caller.
* Each delayed ballistic impact clears the global multi-damage accumulator
  after applying damage, so a stale target cannot leak into later actions.
* Projectile restoration rejects additional invalid state and resumes visual
  tracer sampling from the restored projectile position.
* The standalone editor rejects cartridge ID case aliases in immediate and
  native validation.

## Legacy weapons and reference profiles

`../modular-legacy-profiles/` has four **optional, data-authored examples**:
Glock-like, MP5-like, Python-like and shotgun-style. These deliberately do
NOT replace the native `weapon_9mmhandgun`, `weapon_9mmAR`, `weapon_357`, or
`weapon_shotgun` entities. They reuse original model paths and some weapon
clip/timing settings, but new ballistic parameters are approximate samples.

Important non-equivalences: MP5 grenade launcher, original shotgun per-shell
reload and double shot, Python secondary zoom, individual weapon animations,
server prediction and audiovisual subtleties. Specialized Gauss, Egon, RPG,
crossbow and other native classes remain in place. Exact migration needs
in-game behavior comparison and further reusable behavior modules.

## Validation

From PrimeXT source root (requires a C++17 compiler and Python 3):

```sh
c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp game_shared/tests/modular_j6_test.cpp -o /tmp/modular_j6_test
/tmp/modular_j6_test documentation/examples/modular-ballistics
python3 game_shared/tests/modular_j6_source_tests.py
cmake -S tools/weapon_editor -B /tmp/primext-weapon-editor
cmake --build /tmp/primext-weapon-editor
/tmp/primext-weapon-editor/primext-weapon-validate documentation/examples/modular-legacy-profiles
```

For existing portable J.2/J.4 tests, use **their original fixtures**, not the
new J.3/J.5 files: those tests make exact assertions about past schema states.

## Required in-game acceptance tests (not performed by source checks)

1. Full PhysX-enabled server/client/engine builds with correct submodules.
2. Load a map with two distinct modular pickups and ammo entities; verify no
   name/ID conflicts and correct ammo inventory and weapon switching.
3. Fire every reference profile as a player and configured human grunt and
   compare spread, cooldown, collision, hitgroups, ragdoll impulses and decals.
4. With physics enabled, test ragdoll limbs behind cover, `_cp` static props,
   and interactive `.phys` objects. Compare nearest impact selection.
5. Host/client multiplayer: prediction, mismatched definitions, tracer effects,
   ammo/reload state and restore across save/load.
6. Stress hundreds of projectile updates and confirm no runaway entity count.

Not yet implemented: native 3D animated Studio mesh preview, generalized
NPC/turret integration, fully interpolated network projectile visuals,
mixed cartridge magazines, larger-than-legacy network IDs, or complete
legacy-equivalent weapon mechanics. Do not enable this system in production
until these runtime tests and any resulting fixes are completed.
