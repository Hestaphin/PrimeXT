# Version 4 modular weapon schema — J.11.1 authoring examples

This is a **parser-only** example until J.11.2 adds v4 runtime shot dispatch.
J.11.1 deliberately refuses `Registry::ValidateForRuntime` for v4 to avoid silently executing v4 assets with the old v3 firing rules. Do not install this example in a playable mod yet.

New **Attack Logic** is authored on each cartridge. `hitscan` requires `legacy` Damage Logic with `default_damage` and `legacy_bullet_type`. `ballistic` requires `configurable_base` with `base_damage`, or `energy_based` with `damage_coefficient`.

In the weapon definition `default_fire_rate` is rounds per minute (RPM), and `ammo.<id>.fire_rate` optionally overrides that value **for that weapon only**. Inherited values are absent, never encoded as zero. Convert RPM to seconds when firing: `seconds_between_shots = 60 / effective_fire_rate`. Thus 600 RPM fires once every 0.1 seconds, 300 RPM fires once every 0.2 seconds. A firing event can spawn more than one projectile; RPM measures events, not projectiles.

`ammo.<id>.spread` optionally overrides `default_spread`. An explicit 0 spread differs from absent spread. `ammo.<id>.fire_sound` overrides `default_fire_sound` (both paths relative to `sound/`).

The v4 action string `fire_loaded_cartridge` describes **what pressing the trigger does**, while each loaded cartridge determines how the projectile travels. That runtime behavior is reserved for J.11.2.

The file extensions, manifests, and model/sound paths follow existing PrimeXT conventions. Supply valid model/sound resources before full editor or game use. Older format 1-3 files may still be read during migration, but they are not the design target.


## J.11.2 gameplay update

J.11.2 turns these Version-4 cartridges into runtime firing choices.
`hitscan9` fires immediate legacy traces with 60 explicitly authored base damage.
`ballistic9` starts at base damage 40 and decreases damage with kinetic energy.
`energy9` calculates damage from current kinetic energy and diameter.
Switching selected ammunition does not change an already loaded magazine until reload completes.
As of J.11.5.1, `sv_ballistics_enabled 0` temporarily converts v4 ballistic
shots to instant hitscan, keeping the ballistic cartridge's muzzle damage.
Turning simulation back on restores ballistic flight without modifying the file.
RPM/spread/sound firing overrides became active in J.11.3. This sample uses illustrative model
paths, not packaged, game-ready artwork.
