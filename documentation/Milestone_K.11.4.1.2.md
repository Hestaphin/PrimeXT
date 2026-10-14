# K.11.4.1.2 — collision and pose caching

Step 2 of the six-step FPS improvement plan, based on K.11.4.1.1.

## Changes

- Server Studio queries share the last exact pose of each entity. A bone is
  evaluated once for that pose and reused by interactive-part sampling,
  articulated animation targets, movement collision and visible impact meshes.
  Cached results are raw engine pivots: each existing consumer still applies
  its own scale and authored body offset.
- Animated server collision retains its finished trace mesh while the exact
  model, .phys fingerprint, selection, transform, shape activation, ownership
  and actual animation/physical poses match. Inputs are sampled before the
  comparison and the mesh is built from those same snapshots. There is no
  movement tolerance, delayed refresh or substitution of a reference pose.
- Primitive source triangles are retained by their part and replaced when the
  .phys fingerprint changes. Moving a hinge no longer tessellates its source
  sphere/capsule/box again.
- Client prediction retains up to 16 moving collision meshes. It obtains the
  current shared CPU bone poses before comparing exact geometry inputs, so
  animation, interpolation, physical overrides and expired/unavailable poses
  can invalidate the mesh even within one prediction frame. Failed builds drop
  stale geometry and retry; the existing partial-geometry policy is retained.
- Visible impact meshes retain up to 32 entities. Repeated rays reuse the same
  finished current-pose mesh. Skin/body changes invalidate material data as
  well as geometry. A failed live build drops stale data and uses the existing
  reference-mesh fallback for that exact pose revision.

Impact and prediction mesh buffers each have a 16 MiB retention budget in
addition to their entity-count bound. Oldest payloads are evicted to make
room. A mesh larger than the budget stays owned by its current query and is
freed afterwards; its live geometry remains available for that query.

The server bone cache has a 128-entity LRU bound and stores at most the model's
128 supported Studio bones per entry. Eviction costs a future recomputation;
it does not change the query result. A newer exact pose replaces an entry.
Model index, model/header pointer, Studio model CRC, model name, edict serial,
entity identity, sequence/frame, controllers, blending, animation timing,
gait, scale, skin/body and local/absolute transforms participate in freshness.
Map/world teardown, server deactivation, restore and backward game time reset
server pose generations. Prediction mesh storage clears on map reset or a
backward client clock. Collision-state teardown also invalidates its stamp.

A failed server mesh finish now releases the previous pose mesh instead of
leaving stale collision geometry available. An unchanged empty selection is a
valid cached result; a later active selection rebuilds immediately.

## Measurement

The optional K.11.4.1.1 profiler remains off by default and retains its CSV
schema marker. New count metrics are:

| Metric | Meaning |
| --- | --- |
| `server_pose_cache_hits` / `server_pose_cache_misses` | Reused raw bones / engine bone queries through the shared cache |
| `collision_pose_cache_hits` / `collision_pose_cache_misses` | Exact trace-mesh reuse / refresh attempts, reported separately for client/server |
| `impact_pose_cache_hits` / `impact_pose_cache_misses` | Reused visible impact pose / changed-pose build attempts |

`server_bone_query` now measures actual engine calls rather than cache hits.
`collision_mesh_builds` and `collision_triangles` also count prediction builds.
Existing `mesh_cache_hits/misses` still refer to CMeshDescFactory, and
`pose_cache_hits` still refers to the client CPU bone evaluator.

For the same map, camera, entity counts and shooting pattern, compare
K.11.4.1.1 with this version using `r_profile 2` and `sv_profile 2`. Check
`collision_mesh_builds`, `impact_mesh_builds`, `server_bone_query`, the new
cache counts and p95/p99 `frame_interval`. Include opening/closing doors,
Free/Driven handoffs, bodygroup/skin changes, ragdolls and save/load.
Use Release binaries. A stationary scene and repeated hits should show fewer
builds; a changing pose is deliberately allowed to miss.

## Preserved behavior

Physics remains at 100 Hz with the existing accumulator/catch-up behavior.
Ballistic curve sampling, maximum 20 ms collision segments, damage, nearest
hit selection, solver settings, sleep/freeze, rendering passes and pxmv
Full/Partial preview behavior retain their existing implementations.
The cache is always active; profiling controls only measurement. There is no
new runtime switch or saved/network cache state. No Xash engine changes are
introduced by this step.

## Build and verification

Reconfigure the existing CMake build and rebuild client/server libraries.
The server source list now includes `studio_pose_cache.cpp`.

```sh
python3 game_shared/tests/verify_pose_cache_k11412.py --retained
```

The verifier compiles native Linux objects and runs extracted production cache,
pose and mesh methods using recording engine/mesh/SDK adapters, real project
matrix arithmetic and primitive tessellation. It compares server collision and
visible geometry exactly with frozen K.11.4.1.1 methods, and runs ASan/UBSan
with profiling on and off at O0/O2. LeakSanitizer is disabled because process
inspection is unavailable in the validation environment.

These checks do not link a complete game or execute a native PhysX solver,
real GPU, pxmv UI or an in-game FPS benchmark. Actual performance gains require
the repeatable in-game capture described above.
