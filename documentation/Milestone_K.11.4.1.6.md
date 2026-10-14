# K.11.4.1.6 — ballistic curve integration

Step 6 completes the six-step FPS improvement plan, based on K.11.4.1.5.
The central modular ballistic manager now builds ordinary free-flight curves
at 50 Hz (20 ms of simulated time), while collision sweeps remain at most
20 ms. Projectile processing still runs from server EndFrame; the new setting
does not delay impacts to a 50 Hz Think or change server/display frame limits.

## Rate and accuracy policy

`sv_ballistics_hz 50` is the archived server default. `sv_ballistics_hz 100`
selects the original 10 ms integration and linear interpolation for comparison.
Only these two tested rates are supported: finite values at least 75 select
100 Hz; lower or nonfinite values select the 50 Hz policy.

The 50 Hz policy uses a conservative drag guard. The maximum possible speed
over remaining lifetime is bounded by current speed plus gravity × remaining
time. If `k × maximum speed × 0.02 > 0.05`, the curve uses the original 10 ms
path instead. Here `k = density × Cd × frontal area / (2 × mass)`. This avoids
coarsening unusual high-drag/lightweight/large-diameter cartridges. Invalid
physics is still rejected before building a curve. The guard requires a few
scalar calculations once per curve; it does not build two curves for comparison.

The selected interval stays with the cached curve. A setting change affects
newly fired, post-penetration and restored curves. It does not re-integrate or
resample every active shot on a console setting change. Physics and rendering
presets remain separate from this server ballistics setting.

## Coarse math and cached sampling

Simply doubling the previous trapezoidal displacement can amplify drag errors.
The coarse path instead integrates the drag displacement analytically:

`displacement = kicked_velocity × dt × log1p(q) / q`, with the limiting factor
one at `q = 0`. The existing stable drag velocity and centred half-gravity
kicks remain. This is exact for gravity-free quadratic drag and for vacuum
gravity. `log1p` retains precision for small drag amounts.

Between coarse cached samples, cubic Hermite positions use the stored endpoint
velocities. Gravity therefore produces a curved position between samples
without running the integrator in the frame loop. Velocity and accumulated
range remain bounded linear interpolations; collision contacts remain on the
actual trace segment. The 100 Hz and guarded fallback paths preserve the old
integration and interpolation exactly. Shared helper defaults retain the old
10 ms math for existing callers.

Curves are still precomputed once per uninterrupted flight. They rebuild when
penetration changes velocity or a save is restored. Lifetime remains ten
seconds, active shots remain capped at 256, penetrations remain capped at four,
and final range/lifetime samples remain clipped. A full-lifetime coarse curve
has 501 points instead of 1,001: 500 integration calls instead of 1,000. Short
range and energy expiry reduce both counts further; fallback curves retain the
original cost.

## Collision, damage and saves

The maximum collision sweep is still 20 ms regardless of curve rate or physics
preset. Each server frame may use shorter segments. Trace flags, current moving
geometry queries, nearest ragdoll hit selection and PreparePenetration's exact
geometry/material queries are retained. Candidate geometry is captured before
damage callbacks can replace or detach a target.

Material traversal still consumes simulation time before exit. Unused catch-up
time stays on each shot's clock. There is no pre-birth movement, early exit
teleport or extra damage while a shot remains inside a material. Configurable
base/energy-based formulas and physics impulse routing are retained.

The compact ShotDisk layout and version 3 remain unchanged; version 2 remains
readable. The interval is transient curve data, not a new save field. Restore
keeps scalar position/velocity/age/range and pending material transit, rebases
the shot clock, and rebuilds free flight using the current rate/guard when
appropriate. Float snapshot precision and curve rephasing are assessed by
continuation tests rather than assumed to give bit-identical future samples.

## Profiling and comparison

Compatible `PERF_K11411` output adds count metrics:

- `curve_builds_20ms`: successful coarse curves.
- `curve_builds_10ms`: successful original/fallback curves.
- `curve_accuracy_fallbacks`: successful curves forced to 10 ms by the guard.

Existing `curve_samples`, `curve_build`, `ballistics`, `bullet_sweeps`,
`bullet_impacts` and `active_shots` remain. The CSV extractor now accepts digits
after the first letter/underscore in metric names, so the new interval-labelled
rows are retained. Profiling remains off by default.

The supplied `ballistics_50hz.cfg`, `ballistics_100hz.cfg` and
`profile_capture.cfg` provide a controlled comparison. Test the same weapon,
cartridges, map, camera, shot count, server/display limit, physics rate and
render preset. Compare curve counts/build CPU time and frame p95/p99; also
check hit/penetration behavior with moving characters, physical props and
ragdolls. A sample-count reduction is not a claim of an equal FPS gain.

## Validation

```sh
python3 game_shared/tests/verify_ballistics_k11416.py --retained
```

The runner checks production curve/integrator/manager/BeginPenetration/impact/
save/restore methods with ASan/UBSan at O0/O2, recording collision/entity
services and native Linux objects. Its independent RK4 reference covers six
projectile profiles, three directions, gravity ×0/1/10 and density 0/1.225/3:
162 scenarios and 2,894 sample comparisons. The reference is independently
refined to half its integration interval. Coarse position error is below
1 cm over that envelope; ordinary horizontal 9 mm/rifle cases at normal gravity
and density remain below 1 mm. Peak sampled energy error is approximately
0.116%. Guarded curves retain the old samples exactly; their peak reference
position error in the envelope is approximately 2.87 cm.

Manager cases check rate/guard profiling, simulated-time equivalence at
20/30/60/100/144/240 server FPS, 2.54 mm barriers at 100/900/10,000 m/s,
energy loss and pre-damage geometry capture, four-penetration limits, mid-
material saves, free-flight restore continuation, rate changes, both v4 damage
formulas and retained catch-up time. Recording traces test manager scheduling
and sweep limits; additional retained tests cover production BSP/shape exit queries.
Native manager/game/shared objects compile at O0/O2. Windows min/max macro
compatibility is exercised separately.

With the matching patched Xash source, run the additional ballistics checks:

```sh
python3 game_shared/tests/verify_ballistic_regressions_k11416.py --xash-root ../xash3d-fwgs-master
```

These retain the original SI, curve, range/time, penetration, API and shape-
selection tests at O0/O2, plus the production engine first-solid query and game
Prepare/Begin methods with recording 16/32-bit BSP hull/material/entity services.
The engine query headers come from the supplied K.11 engine patch; they are
unchanged by this release. Omitting `--xash-root` runs the portable subset.

The full earlier rendering, physics, sleep, exact cache, profiling, map-
transition and ballistics regressions run unchanged. Both patch routes must
apply with strict whitespace checks and reproduce all tracked file bytes and
executable modes. The release bundle includes reports, selected logs, configs,
checksums and the source manifest.

These checks do not link/run a full native game, real PhysX solver, live GPU or
Windows build, and do not measure in-game FPS. LeakSanitizer is disabled because
process introspection is unavailable; ASan/UBSan remain enabled for the stated
adapters. The isolated curve microbenchmark excludes collisions, renderer,
solver and manager overhead. Target-machine Release comparisons are still
needed to establish actual frame-pacing and FPS gains.
