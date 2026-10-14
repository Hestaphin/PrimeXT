# PrimeXT K.11.4.1.1 performance profiling

K.11.4.1.1 implements step 1 of the six-step FPS improvement plan. It adds
optional CPU timings, asynchronous GPU timings, frame pacing statistics and
work counters to establish where K.11.4.1 spends its frame budget.

This release retains the 100 Hz PhysX step, its current catch-up behavior,
10 ms precomputed ballistic curve samples, maximum 20 ms collision segments,
sleep/freeze rules, collision ownership, render passes and pxmv Full/Partial
preview behavior. The remaining optimization steps follow these measurements.

## Apply and build

For a clean checkout of the supplied original PrimeXT source, use
`PrimeXT_K.11.4.1.1_from_original.patch`. For an exact K.11.4.1 checkout, use
`PrimeXT_K.11.4.1_to_K.11.4.1.1.patch`. Apply only the route matching your tree,
from the PrimeXT repository root:

```sh
git apply --check --binary --whitespace=error PATCH_FILENAME
git apply --binary --whitespace=error PATCH_FILENAME
git diff --check
```

Reconfigure your existing CMake build so it includes the new renderer source,
then rebuild the PrimeXT client and server libraries for the same architecture.
Use Release builds for performance comparisons. There are no new Xash engine
changes in this release; retain the matching engine fixes already used with
K.11.4.1. This is a PrimeXT patch with repository-root paths.

## Console controls

All measurement controls are non-archived. Profiling is off by default.

| Control | Default | Meaning |
| --- | --- | --- |
| `r_profile` | `0` | Client profiling: 0 off, 1 readable summary, 2 CSV rows |
| `sv_profile` | `0` | Server profiling: 0 off, 1 readable summary, 2 CSV rows |
| `r_profile_interval` | `2` | Client report interval in wall seconds |
| `sv_profile_interval` | `2` | Server report interval in wall seconds |
| `r_profile_gpu` | `1` | Request GPU timestamps when client profiling is enabled |

Intervals are restricted to 0.25-60 seconds; invalid values use 2 seconds.
Non-finite profiling modes disable capture. Mode changes, game-clock rewind,
map changes, video initialization and server deactivation reset the relevant
capture. Reports are periodic; turning profiling off discards an unfinished
report window. Collect at least several complete windows before stopping.

For a readable initial inspection, enter these commands after loading a map:

```text
r_profile 1
sv_profile 1
```

For a retained capture, start Xash with `-log perf_K11411`, then enter:

```text
r_profile_interval 2
sv_profile_interval 2
r_profile_gpu 1
r_profile 2
sv_profile 2
r_render_report
```

The example `documentation/examples/performance_k11411/profile_capture.cfg`
contains those capture commands. The engine writes its console output to
`perf_K11411.log`. Stop with `r_profile 0` and `sv_profile 0`.

Use `r_profile_gpu 0` before starting a capture to collect CPU timings alone.
The client prints whether GPU timestamp capture is available. GPU support
requires desktop OpenGL 3.3 or `GL_ARB_timer_query`, the required query exports
and a 64-bit timestamp counter. Unsupported configurations retain CPU profiling
and emit no GPU timing rows; CPU duration is never presented as GPU duration.

## Read the measurements

Each row contains samples, mean, p50, p95, p99, maximum and calls. Time values
are milliseconds; counters use the `count` unit. A sample is the total for one
profiled CPU frame or one completed submitted GPU frame. A scope can be called
several times within that sample. Percentiles use nearest rank. Each metric
retains at most the latest 2048 samples in its report window, with a matching
mean and call total. No unbounded sample allocation occurs.

CPU timers measure elapsed main-thread spans, including waits. They do not
measure CPU utilization or the summed execution time of all PhysX workers.

| Measurement | Boundary or interpretation |
| --- | --- |
| Server `frame_cpu` | StartFrame entry through EndFrame completion, before profile reporting; includes engine work between those callbacks |
| Client `frame_cpu` | PrimeXT HUD_RenderFrame callback, including backend setup and completion; excludes engine presentation and other work outside that callback |
| `frame_interval` | Wall time between consecutive profiled frame entries; includes external work, presentation waits and prior report output |
| `game_delta` | Server input frame delta, or change in client game time; distinct from wall frame pacing |
| `profile_report` | Time spent sorting and printing the previous report, recorded in the next report window |
| `physics` | Whole PhysX Update method |
| `physics_submit` | Time inside simulate(), including work it performs synchronously |
| `physics_wait` | Time inside blocking fetchResults(true), including completion/callback work |
| `physics_capture` | Simulation pose capture and existing optional ragdoll diagnostics |
| `physics_debt` | Accumulator milliseconds due immediately before the current catch-up loop |
| `physics_steps` | Actual fixed 10 ms steps taken in that server frame |
| `collision_mesh` | Animated trace mesh rebuilding, including early exits |
| `collision_mesh_builds` | Attempts reaching animated trace mesh allocation |
| `collision_triangles` | Requested triangle totals for those attempts |
| `impact_mesh` | Building the posed visible Studio mesh for impact effects |
| `server_bone_query` | Server bone-query helpers used by collision/animation and posed impact meshes |
| `studio_bones` | Shared client CPU bone evaluator during a profiled render callback |
| `bone_evaluations` and `pose_cache_hits` | Actual evaluator executions and reuse within the existing CPU pose cache |
| `ballistics` | Manager update; curve building and impacts may also occur elsewhere in the server frame |
| `curve_build` and `curve_samples` | Free-flight curve construction and successful generated sample totals |
| `bullet_sweep` and `bullet_sweeps` | Ballistic collision segment queries and their call count |
| `active_shots` | Shots present when the manager update starts |
| `awake_bodies` and `dynamic_bodies` | PhysX active dynamic body and total dynamic body statistics after Update |
| Ragdoll counters | Mutually exclusive active, sleeping and frozen instances with created actors |
| Mesh cache counters | CMeshDescFactory lookup hits/misses, not a claim that all mesh construction is cached |

GPU `gpu_frame` covers the timestamp interval around the custom renderer
callback. The other GPU rows cover world drawing, subviews, shadows,
post-effects and the viewmodel. They are elapsed GPU-timeline intervals;
gaps caused by CPU command submission can appear in them. They do not include
engine work or swap/presentation after the custom callback.

CPU and GPU categories are inclusive: subview rendering contains its world
and shadow work, and physics contains its component operations. Do not add
all rows together to derive frame time. Recursive entry into the same category
counts the calls while retaining only the outer timing interval.

GPU results arrive later than CPU samples. Their sample counts can therefore
differ within a reporting interval. The recorder owns eight pending frame
slots and up to 64 timestamp pairs per captured frame, including the complete
frame pair. It checks result availability before reading; it never adds
glFinish, glFlush, a blocking query-result wait or a rendering-rate change.
If the frame pool is busy, `gpu_skipped_frames` records omitted captures.
If the detail pool fills, `gpu_dropped_scopes` records omissions and only the
complete frame time is published for that frame. Reversed or implausible timer
results are counted as `gpu_invalid_frames` and discarded. Pending samples
from previous captures/maps are invalidated; owned queries are released at
renderer shutdown while the context is available.

## Capture a useful baseline

Use the same resolution, graphics settings, FPS cap, VSync setting, map,
camera position and repeatable workload for each comparison. Warm the map,
shaders and assets before enabling profiling. Record at least 60 seconds for
each workload: an idle scene, many corpses, rapid firing, animated physical
parts, and mirror/cubemap/portal/monitor scenes.

Record `r_render_report`, hardware, build configuration and workload beside
each capture. Keep unrelated per-step debug logging disabled and avoid
`-physxdebug` for the main performance run. Look at p95/p99 and maximum frame
intervals alongside average time and work counters. Profiling itself has a
cost; compare enabled and disabled captures and use `profile_report` to
recognize sorting/output spikes. GPU readback/setup overhead remains inside
the measured client callback.

Extract the CSV rows without changing or averaging their percentiles:

```sh
python3 tools/performance/extract_profile_csv.py perf_K11411.log perf_K11411.csv
```

CSV columns are `side, window, metric, unit, samples, mean, p50, p95, p99,
max, calls`. CPU/client/server and delayed GPU populations differ. Per-window
percentiles cannot be averaged into an overall p99. Compare equivalent windows
and workloads, preserving the sample count.

The GPU mechanism follows the Khronos ARB_timer_query specification:
https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt

## Verification and remaining runtime checks

Run the repeatable validation from the PrimeXT repository root:

```sh
python3 game_shared/tests/verify_performance_k11411.py --work-dir /tmp/primext_k11411_checks --json /tmp/primext_k11411_checks/results.json
```

The validation builds native Linux client and non-PhysX server objects at O0
and O2; tests the actual profiler with controlled clocks and delayed queries;
checks min/max macro compatibility; exercises the production GL adapter;
compares the production PhysX Update method against its K.11.4.1 baseline with
profiling disabled/enabled; and runs the retained renderer, map transition and
ballistic-manager regressions. Service adapters use the actual profiling
implementation. AddressSanitizer/UBSan checks are included. LeakSanitizer is
disabled because the restricted execution environment does not provide the
process inspection it requires.

Complete native Windows builds, a linked game with the actual PhysX SDK,
real GPU query execution, enabled/disabled gameplay comparisons and FPS
measurements on the user's system remain required. The bundle's validation
report distinguishes object compilation and recording-adapter tests from
those runtime checks. No FPS improvement number is asserted for this step.
