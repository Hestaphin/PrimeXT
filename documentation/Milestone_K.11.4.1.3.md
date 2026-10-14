# K.11.4.1.3 — sleep efficiency

Step 3 of the six-step FPS improvement plan, based on K.11.4.1.2.

## Server

Fully sleeping or frozen ragdolls reuse their captured bone poses and aggregate
world bounds. Before reuse, the runtime samples every actor's exact current
transform and compares actor/shape identity, body-to-bone frame, bone index,
physics weight, root mapping, scale and the map/restore/time generation.
An explicit pose edit with autowake disabled still invalidates immediately.
There is no movement tolerance or reduced collision-query frequency.

The first settled sample captures the final pose. Later identical samples skip
the body-to-bone conversions and `getWorldBounds()` calls. Missing or invalid
bodies/bounds disable reuse. Active and animation-controlled/partial ragdolls
keep their existing capture path. Thaw, ownership changes, kinematic handoff,
animation snapping, destruction and validated restore invalidate the cache.
Cached state is temporary and is not written to savegames.

A pose revision also lets unchanged sleeping presentation reuse its entity
frame and local bounds. Model/body/skin, entity identity/serial, origin/angles,
velocity, scale, hull/bounds, physics generation and the authored root relation
participate in freshness. The final post-publication state is stamped.

Every-frame entity relinking remains in both paths. PrimeXT performs trigger
and portal work during relinking, including for nonsolid corpses, so skipping
it could change gameplay. Snapshot scheduling also remains unchanged: active
20 Hz, sleeping 2 Hz and frozen 0.5 Hz, with immediate state-transition and
clock-rewind handling. Clients entering PVS keep receiving full snapshots.

Ordinary rigid bodies already skip transform publication when the SDK reports
them sleeping. This step concentrates on the extra articulated capture and
presentation work that was still running for settled ragdolls.

## Client

Settled corpse bone results can be reused across client samples when their
current input state and root match. Eligibility requires a sleeping/frozen
physical pose, nonsolid/nonmoving corpse state, zero animation playback and
gait, completed packet/sequence blends, stable controller/pose interpolation,
and no external or active part overrides.

Models with IK, procedural/jiggle bones, autoplay or real-time sequences stay
on the normal evaluator. Model/header lifecycle and Studio CRC changes refresh
eligibility. Incoming accepted physics packets invalidate the shared pose as
before, including wakes, physical masks and clear/exit transitions. Both
prediction and drawing consume the same result; drawing still runs each frame.

This does not change the pxmv Full/Partial simulation controls, passive-bone
reconstruction, active animation or transition durations.

## Profiling and comparison

The existing `r_profile` / `sv_profile` controls remain off by default, with the
compatible `PERF_K11411` CSV marker. New count rows are:

| Metric | Meaning |
| --- | --- |
| `ragdoll_pose_captures` | Fresh articulated captures |
| `sleeping_capture_reuses` | Reused identical settled captures |
| `ragdoll_bounds_queries` | Actual actor-bound queries during capture |
| `ragdoll_presentation_updates` | Full entity-frame/bound publications |
| `sleeping_presentation_reuses` | Reused presentation geometry, with relink/snapshot work retained |
| `sleeping_bone_reuses` | Shared client bone reuse across settled samples |

Compare K.11.4.1.2 and this version in the same Release build configuration,
map, camera and corpse counts. Capture active drops, a quiet settled interval,
bullet/explosion wakes, partial ragdolls, trigger overlap and save/load. Compare
the new counts, `physics_capture`, `physics_presentation`, `studio_bones`, and
p95/p99 `frame_interval`. Models with live time-dependent stages can correctly
show zero client sleep reuse.

Physics remains at 100 Hz with the existing catch-up policy. Solver profiles,
sleep/freeze thresholds and grace periods, collision/penetration/damage,
ballistic integration, rendering passes and corpse lifetimes are preserved.
The note's suggested active-ragdoll count/lifetime limits are separate gameplay
settings; this step improves settled work without forcing active bodies to stop.

## Validation

```sh
python3 game_shared/tests/verify_sleep_k11413.py --retained
```

The runner executes production sleep/capture/presentation/network/impulse/save
methods with a recording SDK/math adapter under ASan/UBSan at O0/O2 and with
profiling enabled/disabled. It compares pose and presentation geometry with
frozen K.11.4.1.2 methods. Client CPU tests use the real Studio/IK/procedural and
matrix implementations and check cross-sample reuse plus wake/exit exclusions.
Retained collision/cache, profiling, rendering, transition and ballistic
regressions are included. LeakSanitizer is disabled for this environment.

These checks do not link a complete game with the native PhysX SDK, run a real
solver/GPU, exercise the pxmv UI or measure in-game FPS. In-game captures are
needed to quantify the gain.
