# PrimeXT J.9 — Studio and physics-geometry penetration

Base: J.8 (I.10.10 physics + modular weapons/ballistics). No changes to the standalone Weapon Editor or Xash3D.

## Purpose

Extend J.8's conservative brush-only penetration through **living Studio hitboxes**, **ragdoll `.phys` bodies**, **animated/static `.phys` proxy collision**, and **model collision** while preserving I.10.10's nearest-entry hit, hitgroups, materials, ragdoll impulse, and timed ballistic trajectory processing.

### The `_cp.mdl` distinction

In the current I.10.10 physics branch, `_cp.mdl` is an **editor import file**. Importing it in pxmv embeds its geometry in `.phys` v2/v3 collision sections. No separate runtime `_cp.mdl` loader is reintroduced. J.9's same-shape `.phys` exit queries support those imported collision sections.

## Flow

1. The J.8 ballistic manager sweeps the current due trajectory segment with the existing `BulletTrace()`/ragdoll resolver. The nearest impact wins.
2. **Before** any damage/impulse, J.9 attempts to identify a valid exit in the *same* collision representation that supplied the impact:
   - Brush: retains J.8's same-brush reverse scene ray, which stops when another obstacle intervenes.
   - Living Studio: the Xash target-only reverse model trace must hit the same entity and hitgroup, with an outward-facing back surface. An exact Studio hitbox index is not exposed by the current trace API; this is a conservative hitgroup approximation.
   - Active ragdoll: target-only PhysX query of **the exact body index hit at entry**; no scene-wide substitute limb.
   - `.phys` collision: target-only forward/reverse queries on the **same PhysX shape** for animated parts and v2 root collision sections. Includes converted `_cp` geometry.
   - Model collision without a `.phys` body: target-only Xash model reverse trace, not the rendered decal surface.
3. Reject invalid/ambiguous geometry, inverted normals, excessive thickness, offset exits, unknown body index, or world/brush obstructions.
4. Apply **one** normal impact: hitgroup damage, sound, decal, appropriate physics impulse. The target may die/change state during this call; the penetration geometry was captured beforehand.
5. `ComputePenetration()` uses thickness, angle, resistance, projectile factor, and minimum exit speed. If the shot cannot penetrate, it terminates. On success it advances to just outside the captured exit, deducts energy, consumes traversal time, and recomputes the remaining cached flight curve.
6. No more than four penetrations per bullet; existing lifetime, range, projectile-cap, and free-flight limits remain.

## Material defaults

- A material name containing glass, wood, metal/steel, stone/concrete, flesh/body uses J.8's established approximate gameplay parameters (flesh resistance reduced to 1500 J/m to make thin-tissue penetration feasible).
- When a model has no named material, unlabelled Studio/physics props receive a **medium** default (8000 J/m; minimum 120 m/s); living characters receive flesh default (1500 J/m; minimum 80 m/s).
- Unknown world/brush remains hard (90000 J/m, 600 m/s); authored material hashes retain priority. These are not real-world measurements and will need game balancing.

## Critical limits / acceptance requirements

- **No assumption of successful penetration**: PhysX open triangle meshes, inconsistent geometry, very thick hulls, or incomplete authoring may legitimately stop at the entry.
- Studio hitbox traces provide *hitgroup*, not unique hitbox index. More complex same-hitgroup overlapping/disconnected shapes require a future hitbox-indexed trace API for full guarantee.
- A material-name resistance table is not equivalent to a proper per-surface penetration-authoring feature; weapon/cartridge editor parameterization remains future work.
- Static root `.phys` collision, moving `.phys` parts, and ragdoll bodies require **PhysX SDK and in-game validation**. Source-only syntax checks do not compile PhysX files in the provided environment.
- The new source integration is verified, but multiplayer, ragdoll body exit, Studio reverse-hitgroup accuracy, decal correctness, animation transitions, full link, and runtime collision results remain unverified.
- No separate child bullet entity or tick timer is reintroduced. J.8's central manager and cached flight remain.

## Portable/regression tests

```
g++ -std=c++17 -O2 -Igame_shared game_shared/tests/ballistic_j9_tests.cpp \
  game_shared/ballistic_penetration.cpp -o /tmp/j9_geometry_test
/tmp/j9_geometry_test
python3 game_shared/tests/modular_j9_source_tests.py
python3 game_shared/tests/modular_j8_source_tests.py
```

See also `documentation/docs/eng/physics_assets.md` for conversion of legacy `_cp.mdl` authoring assets.
