# PrimeXT J.8 — Cached ballistic manager (base: J.7 / I.10.10)

## Architecture

- Weapon/cartridge editor and registry remain external to `pxmv` and to the engine.
- `game_shared/ballistic_path_cache.{h,cpp}` precompute SI free-flight samples once per bullet and interpolate by time. Flight is recomputed only following a successful penetration.
- `game_shared/ballistic_penetration.{h,cpp}` calculates energy loss, exit velocity, and minimum velocity eligibility. It is a gameplay approximation, not measured terminal-ballistics data.
- `server/modular_ballistics.cpp`: one `modular_ballistic_manager` entity exists only for map lifetime and engine-compatible save/restore. There are **no per-bullet edicts**, per-bullet `Think()` calls, or PhysX actors. Shots are vector records (up to 256).
- `server/client.cpp` calls `UpdateModularBallisticManager()` **once per server EndFrame after WorldPhysic->Update**. This uses the current positions of players, interactive parts, and ragdolls. Each due piece of the cached trajectory is swept against current geometry; no hit list is baked at firing time.
- Collision remains the I.10.10 trace/material/nearest-physics-body path. Successful contacts call the existing damage, sound, decal and physics impulse pathways with estimated impact-time energy.

## Penetration (conservative initial implementation)

`sv_ballistics_penetration 1` enables it; `0` returns to first-impact stop. Only **brush models** with two verified faces and a reverse trace identifying the same entity can be pierced. This intentionally excludes skinned `_cp`, `.phys`, ragdolls, living characters and unknown custom geometry until proper inside/outside and material thickness APIs exist.

For candidate entry and exit:

1. First impact is processed normally (HP, sound, decals, physical impulses).
2. A reverse trace from at most 96 game units beyond the entry attempts to prove the same brush's exit face; intervening obstacles cause a conservative stop.
3. Map the current material description to gameplay energy-per-metre and minimum velocity. Unknown assets default to high resistance. Wood/glass are more permeable than metal/concrete.
4. Compute energy loss based on thickness and incidence. Cartridge projectile IDs `ap` and `jhp` currently scale the required energy (0.7 and 1.4); `fmj` / unknown types use 1.0. This is an initial approximation, not true terminal physics or externally editable penetration material assets.
5. If insufficient energy, or an unverified exit, stop. Otherwise calculate the lower exit speed, include transit time, move just past the exit face, and precompute the remaining trajectory.
6. Limit to four successful penetrations per bullet, finite 10s lifetime, bounded 256 bullets, <=1002 trajectory samples each. This avoids infinite loops in overlapping brushes.

## Save and restore

Only a single manager entity is saved. It stores serialized scalar projectile states and engine-restorable `EHANDLE` owners, plus the registry fingerprint. Flight samples are rebuilt from the current state after restoring; incompatible/invalid shots are discarded. Mixed-round magazines and full in-game restore tests are still outstanding.

## CVARs

```
sv_ballistics_enabled 1
sv_ballistics_gravity_scale 1
sv_ballistics_air_density 1.225
sv_ballistics_penetration 1
sv_ballistics_tracers 0
sv_ballistics_debug 0
```

## Important performance/correctness tradeoffs

- The trajectory calculation is moved to shot spawn/penetration; collision traces still occur per active due segment, once per server frame or more frequently only when a frame spans >20ms of flight. Dynamic targets cannot be handled correctly by a fully precomputed list of hits.
- The shared cache is bounded and avoids unnecessary numerical integration every frame. It consumes RAM and imposes some upfront CPU cost, particularly for large pellet bursts. **No measured in-game speedup is claimed.**
- Large server hitches clamp catch-up to 0.1s per frame; real-time synchronization under stalls requires later refinement.
- Tracers use existing temporary entities; there is no interpolated physical projectile client actor.
- Material resistance values and projectile-type factors should become editable assets in a future milestone. Bullet impulse still uses the previous damage-based PhysX reaction rather than transferred physical momentum.
- Does not validate full PhysX compile/link, in-game multiplayer, NPC correctness, or save/load without full third-party SDK checkout.

## Checks

Build `game_shared/tests/ballistic_j8_tests.cpp` against `ballistic_flight.cpp`, `ballistic_path_cache.cpp`, and `ballistic_penetration.cpp`; run `game_shared/tests/modular_j8_source_tests.py`; syntax-check `server/modular_ballistics.cpp`, `server/client.cpp`, and `server/game.cpp` with project Linux flags. Full in-engine testing remains required.
