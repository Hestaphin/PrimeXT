# PrimeXT/Xash Milestone J.11.2 — cartridge damage and shot dispatch

This release implements stages **3–4** on top of J.11.1 (stages 1–2).

## Behavior

| Cartridge attack logic | Damage logic | Effective behavior |
|---|---|---|
| Hitscan | Legacy | Existing trace/impact with explicit `default_damage` and hitscan `legacy_bullet_type` |
| Ballistic | Configurable Base | Physical projectile, damage scaled by retained kinetic energy (5% floor for a real nonzero-energy hit) |
| Ballistic | Energy-Based | Physical projectile, impact damage calculated from energy and projectile area |

Both player and NPC code pick the currently loaded cartridge. A disabled ballistic server rejects v4 ballistic fire rather than silently choosing hitscan. Old weapons and old v1–3 modular paths retain their previous behavior. Generic bullet effects on ballistic shots are retained; customizable ballistic effect profiles will need a subsequent improvement.

**The RPM, spread and sound selection hierarchy is J.11.3.** The new authored `fire_rate` fields are already RPM and parsed by J.11.1, but active cooldown scheduling still uses the older action interval until J.11.3. Editor property controls, the upgraded preview, IDs rename, and detailed guides are likewise later milestones.

## Patch selection

Apply **one** route from the parent directory containing `PrimeXT-master/` and `xash3d-fwgs-master/`:

- **Already at J.11.1:** `PrimeXT_Xash_J.11.1_to_J.11.2.patch`
- **Original ZIP source trees:** `PrimeXT_Xash_J.11.2_from_original_combined.patch`
- **Original PrimeXT only:** `PrimeXT_J.11.2_from_original.patch`

Use `git apply --check PATCH` followed by `git apply PATCH`, and avoid applying cumulative and incremental patches together. The patch changes PrimeXT source only, not Xash.

## Tests

```
cd PrimeXT-master
python3 game_shared/tests/verify_j11_2.py
python3 game_shared/tests/verify_j10_6_9_4.py --skip-inherited
python3 game_shared/tests/verify_j10_6.py
```

For the standalone native editor backend:

```
cmake -S PrimeXT-master/tools/weapon_editor -B build-pxwe -DPXWE_BUILD_TESTS=ON
cmake --build build-pxwe
ctest --test-dir build-pxwe --output-on-failure
```

See `documentation/J11_2_cartridge_firing_runtime.md` for the precise damage formulas and remaining runtime limitations.

**Validation boundary:** portable C++ and v4 ballistic-manager simulation testing, source syntax, and native backend CTest do not substitute for an actual full PhysX/Windows build or in-game player/NPC simulation test.
