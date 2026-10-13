# K.11.4.3 — ragdoll activation and PhysX workload refinement

K.11.4.3 addresses avoidable work when a character becomes a ragdoll and during
subsequent PhysX steps. It retains K.11.4.2's render effects, shader execution,
ragdoll geometry, joint definitions, body tuning, CCD and 100 Hz fixed stepping.
Native DLL linking, pxmv GUI operation and in-game FPS still require validation
on a machine with the project's actual dependencies and game installation.

## Shader clarification

K.11.4.2 caches identical uniform parameter writes. The linked shader program
already retains those parameters, so repeating the same write adds driver work
without changing its output. Draw calls and shader execution continue. Sampler
texture binding always runs. Changed parameters still upload; caches are freed
on shader reload/destruction. All 64 supplied GLSL sources/includes are byte
identical between K.11.4.2 and K.11.4.3. Render settings and effect passes have
not been reduced. New recording-GL checks cover program switches, exact matrix
values, partial array writes and reload invalidation.

## Source findings and changes

| Finding | Change | Scope |
| --- | --- | --- |
| `sceneDesc.flags = eENABLE_CCD` overwrites the SDK's default persistent contact manifold flag. | Add CCD with `|=`, retaining default contact caching. | Custom runtime, original PrimeXT runtime, pxmv preview. |
| Original and custom runtimes create one worker for every reported hardware thread for small synchronous 100 Hz steps. | Use an explicit bounded worker policy with an archived startup override. | Both runtimes. |
| Every simulation substep lacks reusable scratch workspace. | Retain a 256 KiB, 16-byte aligned SDK allocation across substeps; release after the scene. | Both runtimes; null allocation falls back to existing SDK allocation behavior. |
| Asset contacts request detailed notifications which `EventHandler::onContact` immediately discards. | Request solver/CCD contacts without those discarded asset reports. | Custom runtime; legacy entity notifications, triggers, conveyor modification and asset self-collision policy remain. |
| Sample, lazy creation and activation can evaluate the same complete bone pose three times in one timestamp. | Reuse an exact same-timestamp Studio input stamp. Frame, sequence, all controllers/blending, model identity, entity frame and rigid scale changes invalidate it. | Custom articulated assets. Another timestamp always resamples the engine. |
| Actors are inserted individually before construction completes. | Construct every body/joint outside the scene, then insert the group through `PxScene::addActors`. | Custom articulated assets; failed construction releases unattached bodies before retry. |

PCM changes the contact generation algorithm to the SDK default; bit-identical
physical trajectories are not promised. Bodies, collision coverage, solver
iterations, masses, damping, constraints, limits, physical animation, gameplay
queries and physics cadence remain in place. No elapsed physics time is dropped
or capped by this milestone. The supplied Barney path uses primitive bodies,
not convex cooking at death.

These are code-supported performance costs, not a measured attribution of the
user's PC slowdown. More efficient worker scheduling depends on scene size and
CPU; use the timings below to compare configurations.

## Controls and measurements

`sv_physx_workers` is read at PhysX initialization, not changed during a running
scene. Default `-1` uses zero workers on at most two reported threads, one on
three/four, and two on larger CPUs. `0` uses PhysX's synchronous dispatcher.
Positive values select that many workers, clamped to the reported hardware
count and 32. Nonfinite/negative inputs use automatic selection. Verify the
applied count with `p_speeds 2` after a fresh engine startup.

`p_speeds 2` adds actual worker count and last/peak full ragdoll activation
request timing to the existing simulation/fetch/pose/mesh counters. Activation
timing spans definition/instance lookup, creation, handoff, impulse transfer and
presentation. Requests include failed and already-enabled requests. Timing
uses no clock reads when disabled. Partial-weight activation through the weight
API has no separate activation timer. The existing fixed-step and diagnostics
behavior is preserved.

Use a Release engine/game/PhysX build, the same map and .phys files, resolution
and graphics settings, and the same Barney death setup for before/after runs.
Record pre-death FPS, activation peak, simulation/fetch times, step count and
the first several seconds after death. High activation peak points at the
handoff; sustained simulation/fetch cost points at active physics. Low physics
times alongside low FPS require render/driver profiling. `r_render_report`
retains the K.11.4.2 cache counters and reports K.11.4.3. Keep diagnostics
settings the same between compared runs.

## Verification

The supplied current v5 Barney loose template has 15 bodies. With the same
recording engine/SDK adapters, the production K.11.4.2 path makes 45 bone reads
and 15 scene insertions for sample + creation + activation. K.11.4.3 makes 15
bone reads and one batch insertion. Both pass the same initial pose, velocity,
mass, collision flag, CCD and solver setting checks. This is a call-count and
behavioral regression result, not a native solver/FPS benchmark.

Run `game_shared/tests/verify_performance_k1143.py` with `--work-dir` and `--json`.
Optional `--baseline-root` compares the actual K.11.4.2 activation functions;
optional `--original-root` checks the original performance port's setup/cleanup.
Tests use explicit engine/SDK adapters and the real asset parser/settings.
The K.11.4.2 runner additionally checks real mesh/math/Studio code and recording
GL uniform stores, with ASan/UBSan. Native linking is checked separately by
`game_shared/tests/verify_j10_6_10_1_native.py` and is not replaced by these tests.

## Primary SDK references

- [PhysX 4.1 scene defaults](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/PxSceneDesc_8h_source.html): the descriptor defaults to `eENABLE_PCM`.
- [PhysX best practices](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxguide/Manual/BestPractices.html): small scenes can be slower with excessive workers; scratch storage reduces temporary allocations.
- [PxScene API](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/classPxScene.html): scratch reuse follows completed fetch; batch insertion is outside simulation and retains connected constraints.
