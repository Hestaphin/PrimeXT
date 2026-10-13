# PrimeXT K.11.4.2 — performance caches

K.11.4.2 builds on the supplied K.11.4.1 source. It optimizes inherited PrimeXT render/collision work and the custom `.phys` integration without changing the graphics defaults, 100 Hz simulation frequency, solver iterations, CCD defaults, or authored body/part settings.

## Changes

- GLSL arrays and single large matrices retain their last uploaded values per uniform/program. Identical bytes and counts skip another driver upload. Sampler binding side effects remain active. Reload/shutdown frees the caches; retention is capped at 64 KiB per uniform.
- `CMeshDescFactory` indexes its stable list of meshes by model name, body, skin, geometry type, and source fingerprint. Embedded area-node/facet pointers stay stable as the index grows.
- Moving client `.phys` collision retains meshes per entity and exact source geometry/pose key. Geometry changes, actual bone poses, model CRC, bodygroups, skin, scale, entity frame, and map lifetime invalidate it. Missing poses reject stale meshes. Nested IK sweeps use independent temporary meshes. Retention is limited to 128 entries and an approximate 32 MiB mesh/key budget; larger queries still work without persistent retention.
- Server animated trace meshes reuse exact selected-section/primitive poses. The construction consumes the same sampled transforms used to form the key, reducing duplicate bone queries. Full articulated ownership clears the proxy mesh.
- Shape simulation/query flags are assigned when their values actually change.
- CPU Studio poses can be reused across samples when they are demonstrably time independent. Multi-frame animation, blends, interpolating controls, IK, procedural/jiggle/autoplay stages, active part poses, and physics transitions continue to update. Fully settled frozen poses can reuse their result. In-place model CRC changes reinitialize pose state. Completed CPU pose revisions also avoid rebuilding GPU bone arrays for matching draws.
- Normal play captures ragdoll poses after the final completed physics step per server update. Requested diagnostics retain their per-step cadence. Frozen captures are reused until thaw or explicit restore invalidation.
- `p_speeds 2` reports physics step count and CPU time in update, simulate, fetch, and pose capture, plus mesh/shape counters. Timing is disabled in normal play. `r_render_report` adds client mesh, pose, publication, and uniform totals.

No shadows, mirrors, portals, monitors, water, HDR, ragdoll/part modes, unboxed-bone choices, networking, bullet interactions, or gameplay collision features are intentionally removed. This patch retains trigger relinking and existing solver/contact behavior.

## Verification and limits

The new executable checks use production methods and real project mesh/matrix/Studio code. Engine/GL services and the PhysX SDK are explicit recording/math adapters. Both `-O0` and `-O2` variants pass ASan/UBSan checks. Managed-runner `/proc` restrictions prevent LSan; direct allocation/free accounting covers retained mesh/uniform buffers in the client test.

Representative operation counts in the fixtures:

| Workload | Result |
| --- | --- |
| 100 unchanged client movement sweeps | 1 mesh build, 99 cache hits |
| 100 unchanged server collision updates | 1 mesh build, 99 cache hits |
| Repeated identical matrix arrays | 1 upload until values/count change |
| Repeated eligible static poses across frames | CPU pose and GPU publication revisions reused |

These counts demonstrate eliminated work in the tested paths; they are not an FPS benchmark on the user's computer.

Complete changed client/shared translation units compile as GCC objects with `-O0` and `-O2`. Inherited production-method checks cover cache corruption, subview allocation, CCD settings, pose/control transitions, ownership, map lifetime, and the supplied BSP classification/sky/water cases. A full CMake/native PhysX build is blocked in this workspace: CMake, native SDK/dependency installations, and a running game/GPU are unavailable. Windows/x86/native solver/game acceptance remains to be run in the user's build environment. See the bundle validation report for exact results and scope.

## Commands

For diagnostic captures:

```text
developer 1
cl_showfps 1
r_speeds 3
p_speeds 2
r_render_report
```

`p_speeds 2` times the game-side physics update, including synchronous `fetchResults`, and counts actual steps. Mesh build time and build/hit counts are cumulative since map clear. `r_render_report` totals count operations; it does not provide GPU timing. `p_speeds 1` retains the body-count display.

For comparisons, use identical release builds, map, assets, camera, resolution, entity count, and graphics settings. Allow shader compilation/loading to settle. Record empty-map, animated-prop movement, active-ragdoll, and sleeping/frozen-ragdoll scenarios. Verify mirrors/portals/monitors/sky/water and transitions before accepting the build.

Further work should follow measurements: GPU pass timing, shadow/subview reuse and budgets, batched server skeleton sampling, contact reporting, worker-count tuning, and an explicit physics overload-time policy. This milestone does not silently discard accumulated physics time or lower render/solver quality.
