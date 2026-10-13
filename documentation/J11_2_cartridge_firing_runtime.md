# J.11.2: Cartridge-owned shot dispatch and damage logic

J.11.2 implements **stages 3 and 4** of the J.11.x five-release roadmap. Starting source: J.11.1. This is a source-level milestone; running the editor's new UI, the full Windows engine, native PhysX and live network gameplay is **not yet verified**.

## New v4 runtime rules

- `primary_behavior` / `secondary_behavior = fire_loaded_cartridge` means "fire whatever is already loaded in the magazine". Merely selecting a different cartridge via `ammo_cycle` doesn't change the fired ammunition until reload finishes.
- The **cartridge**, not the weapon button behavior, chooses `hitscan` or `ballistic` through `attack_logic`.
- **Hitscan + Legacy** means authored `default_damage` damage per projectile. For example, `60` passes **60**, not zero, to the original PrimeXT legacy trace. `legacy_bullet_type` selects a legacy impact classification; it never silently overwrites 60 with a skill-table value. This path keeps hitboxes, collision proxies, decals, impacts, hitgroup/armor and physical bullet reactions provided by the existing trace.
- **Ballistic + Configurable Base** means `base_damage * clamp(impactEnergy / initialEnergy, .05, 1)`, on an actual impact with positive energy. Zero-energy/no impact has **no phantom 5% damage**.
- **Ballistic + Energy-Based** means `damage_coefficient * impactEnergyJ / (pi * diameter_mm² / 4)`. Energy is calculated using the actual accepted weapon+cartridge muzzle velocity, then updated as the projectile flies and penetrates.
- The ballistic manager retains existing server-authored collision, penetration, ragdoll impulses, decals and impact sounds. v4 ballistic shots use a generic legacy bullet-impact compatibility class (9mm) until a dedicated ballistic impact-effects profile is supported; this does **not** select damage or speed.
- **J.11.5.1 update:** if server `sv_ballistics_enabled` is 0, v4 ballistic player/NPC shots temporarily use the previous hitscan trace rather than refusing to fire. Ballistic cartridges retain the configured damage logic, and hitscan damage uses the cartridge's muzzle damage at the accepted weapon+cartridge muzzle velocity. No simulated energy falloff or ballistic penetration applies while disabled. Old modular actions keep their established fallback.
- The client predicts animation/state for v4 ballistic shots, but does not generate bullet simulation/damage. The server chooses either ballistic simulation or the instant fallback based on its cvar, and remains authoritative. Live client/server prediction convergence when dynamically changing the cvar remains to be verified in multiplayer.
- v4 positive explicit HP no longer forces gibbing merely because damage > 16. The change applies only when the shot originates from a v4 modular damage scope. Original Half-Life/PrimeXT player/NPC weapons and older modular assets retain their original damage-flag behavior.
- Version-4 weapons/cartridges now pass `ValidateForRuntime` after ordinary ammo-slot/ID preflight. `RuntimeFingerprint()` adds a distinct v4 runtime marker to detect version mismatch.

## Stage boundaries

**Not yet implemented:** J.11.3 makes `default_fire_rate` (RPM), cartridge-specific `fire_rate`, `default_spread`, cartridge-specific `spread`, and sound-default/override precedence authoritative in firing and updates the property editor. J.11.4 upgrades the model viewer and per-ammo UI. J.11.5 adds transactional renaming and detailed beginner help. The new version-4 damage and attack fields may not yet be editable in the native UI; use source definitions for this milestone.

Original v1–3 modular definitions continue to work for now. Their old `fire_interval` behavior remains internal and for older authored schemas only. Version 4 only accepts RPM values in authored files, even though their conversion into active shot scheduling is deferred to J.11.3.

## Checks

Use `python3 game_shared/tests/verify_j11_2.py` for executable portable formula and player/NPC adapters, inherited context/ballistic manager save/restore and actual v4 manager impact, damage-model and save/restore. Use `python3 game_shared/tests/verify_j10_6_9_4.py --skip-inherited` for actual server/client C++ syntax and inherited networking source-path checks. `python3 game_shared/tests/verify_j10_6.py` runs the broader portable/source regressions. No successful link or live Windows engine run is implied.
