# Milestone J.10.1 — ballistic range, timing, transit and restore repairs

**Baseline:** J.9 merged PrimeXT I.10.10 + J.9 ballistic manager. No Xash source changes.

## Repairs

1. `PrecomputeBallisticCurve` clips its final position/velocity/elapsed time at **exactly** the configured max distance or ten-second life, even when the first integration sample would overshoot. `SampleBallisticCurve` uses cached sample timestamps instead of assuming the last step is always 10 ms. `AdvanceShot` also prevents collision sweeps beyond the requested range.
2. Every shot has a separate `lastSimulatedTime` set on creation. The manager computes its frame budget using that shot's clock. Unused capped catch-up time is not discarded. A shot created halfway through an active manager frame cannot inherit another shot's elapsed time.
3. A successful penetration begins a `BallisticTransit` phase **without immediately moving to the exit**. The manager advances distance, velocity and age inside the material only as simulation time is consumed, and rebuilds the subsequent free-flight curve only after the exit time. It terminates if its range or lifespan is exhausted inside the object. Transit entry/exit, progress and duration are saved and restored.
4. Save validation permits kinetic energy gained from gravitational descent: the increase is bounded by `mass * gravity * traveledDistance` plus a small numerical tolerance. Finite parameters, age, range, and other safety checks remain.
5. Compact shot save records have version `2`. Incompatible J.9 in-flight records are discarded rather than misinterpreted; continuing an old game with no in-flight bullets still requires validation in a live engine.

## Tests

Portable `ballistic_j10_1_tests.cpp` covers the 16-unit example at 385 m/s, exact terminal cache time, later shot birth, catch-up accounting, incomplete material transit, stopping inside material at max range, and a downhill gravity-assisted restore. `modular_j10_1_source_tests.py` checks the server integration. Legacy J.6 and J.9 source checks were updated to recognize the centralized manager and deferred penetration, preserving their existing collision and geometry assertions.

## Out of scope / required follow-up

- **J.10.2:** a reverse world trace may select a second world brush as the first brush's exit. PhysX shape identity and primitive-only/disabled-root exit checks also need repair. No claim of correct multilayer-material penetration yet.
- **J.10.3–J.10.5:** ammunition pickup/restore/usability, NPC charging, renderer-only collision prediction, registry lifetime, death ragdolls, and editor Windows path issues remain.
- Precision of PhysX penetration exits, net timing, actual save/load, and full PhysX-enabled game linking cannot be certified without an SDK-complete build and live game test.
- No ricochet, mixed-cartridge magazines, full projectile interpolation or general NPC adapters added here.

## Applying

Use the J.9 -> J.10.1 incremental patch **or** the original-source cumulative patch, never both. The full source ZIP is an alternative.
