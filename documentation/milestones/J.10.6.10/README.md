# J.10.6.10 — R01 phase F0 diagnostics

This milestone reconstructs J.10.6.9.9, records the available baseline, and adds
optional measurements for physics stability and Studio pose reconstruction.
It does not repair the human generator, adopt the future format, tune the
solver, or certify Barney's ragdoll as stable. F1–F5 remain subsequent work.

## Baseline and scope

- Starting point: the two original source archives plus the J.10.6.9.9 combined
  cumulative patch, applied once. Earlier cumulative patches are alternatives,
  not a stack to apply before that patch.
- Source archive hashes match the J.10.6.9.9 bundle's provenance. Preserve ZIP
  executable permissions when extracting on POSIX; Python's default extractall
  drops them. Native `unzip` normally preserves the archived permissions.
- `F0_BASELINE.json` records original source hashes and unchanged physics
  functions/files. The portable verifier checks those against this milestone.
- Production changes are in PrimeXT. Xash production code is unchanged from
  J.10.6.9.9. The combined-from-original patch still includes previous Xash work.
- `.phys` remains version 4 in F0. The authorized breaking replacement is future
  work, not a partially introduced format in this diagnostic milestone.
- The matching `barney.mdl`, reference `barney.phys` and symptom recordings are
  absent from the available inputs. Their contents and scale cannot be inferred
  from the source archives. See the acceptance and defect-register files.

## Server capture

`sv_ragdoll_diagnostics` defaults to 0 and is not archived.

| Value | Behavior |
| --- | --- |
| 0 | Disabled; no diagnostic actor/joint getters, scene statistics or formatting. |
| 1 | Read after every completed 100 Hz physics step; print first sample, then peaks approximately once per simulated second per asset instance. |
| 2 | Print every completed physics step. This is intentionally verbose. |

The existing `sv_ragdoll_debug 1/2` visualization is independent. Server console
records begin with `[ragdoll_f0]`, identify the entity and asset at capture start,
print a CSV `header=`, and then emit `data=` rows. A new header denotes a new
capture epoch; entity indices can be reused. Capture time starts at enablement
and advances by simulated substeps, not by render frames or wall time.

The recorder reads body poses/velocities, sleeping versus kinematic counts,
joint local anchors transformed into world space, relative angular velocity,
joint angles/limits and scene statistics. It also compares cached server bone
poses with `actor_world * body_to_bone`. It changes no actor, shape or joint.
Disabling, destroying actors, or leaving ragdoll state resets recorder state.

## pxmv capture

1. Load the exact model and existing reference `.phys`; inspect without saving.
2. In Physics Asset Editor, enable **Diagnostics**, then **Simulate**.
3. Inspect existing body wireframes and the F0 status. The model's final normal
   rendering pass supplies the pose measurements; reflected floor passes do not.
4. **Save F0 log** exports the recorded rows to the first unused sibling name,
   such as `models/barney.phys.f0.1.csv`. Exclusive creation protects previous
   recordings and the `.phys` file. The directory must be writable.
5. Stop/reset or disable to flush remaining physics samples. A new enabled
   simulation/capture clears the prior in-memory recording, so export first.

The CSV includes the current resolved asset as comments, including unsaved
authoring edits, stable body/joint order, root indices, normalization span and
the preview settings. Reports retain physics peaks between 0.25 simulated-second
boundaries and rendered-pose peaks observed between those reports. Storage is
bounded to 4096 rows; further reports increment `dropped_rows`, and the status
announces the limit. Exported files include that count. No real measurements
are bundled as Barney evidence; the test fixtures are explicitly synthetic.

## Client pose capture

`cl_ragdoll_diagnostics` also defaults to 0 and is not archived. Value 1 prints
at most approximately once per client second for each visible ragdoll whose
bone pose is rebuilt; value 2 prints every rebuilt pose. Cached poses do not
create extra rows. `[ragdoll_f0_pose]` identifies entity, client time and received
snapshot sequence. Measurements are taken immediately after CPU reconstruction.

`driven_pos`/`driven_deg` compare with the newest network endpoint, so intentional
interpolation and entry blending contribute to them. They are target lag, not
proof of a renderer error. `child_*` and `ancestor_*` separately compare the
required animation-relative links with final bones. A settled target is useful
for isolating reconstruction from interpolation. This does not inspect GPU
skinned vertices, decals or all render-cache paths.

## Metric interpretation

| Metric | Meaning / limitation |
| --- | --- |
| Linear speeds / positions | Engine world units and world units per second; no new unit conversion. |
| Angular speed | Radians per second. Angle and angular-axis excess are degrees. |
| Anchor separation | Distance between actual world joint anchors, not actor centers. Broken joints are counted and excluded. |
| `bind_span` | Largest axis extent of the simulated bind-bone pivots. Constant per capture; not full mesh height or fallen AABB height. |
| `anchor_pct_bind_span` | 100 × separation/span; -1 if span is unavailable. Calibrate thresholds using the actual model. |
| `axis_excess_deg` | Conservative per-axis overshoot. It does not fully measure coupled D6 swing-cone violations or solver impulses; zero does not imply valid anatomy. |
| `scene_*pairs` | Scene-wide non-CCD narrow-phase/contact counters, not per-ragdoll contact counts. |
| `sampling_ms_peak` | Diagnostic getter/reduction time, excluding CSV formatting, console output and GPU/GUI work. It is not frame time/FPS. |
| `frame_input_s_peak` / `backlog_s_peak` | Largest incoming update delta and remaining accumulator at sampled substeps. Preview input is recorded before its existing clamp; these expose frame stress/backlog without changing the stepper. |
| Pose link deltas | Position/orientation discrepancy of non-driven descendants/ancestors from preserved animation-relative transforms. |
| `edge_length_delta` | Largest Studio parent-child length change from evaluated animation; a warning measurement, not a certified skinning error. |
| `pose_valid` / invalid counts | Unavailable is distinct from zero. Check these before interpreting small values. |
| Worst indices | Body/joint/bone indices for tracing a peak; pose indices identify the most recent position or angle peak and may differ from the bone owning the other peak. Use the recorded asset and exact model. |

Counts normally describe the latest sample; speed/error/timing fields retain
window peaks. Invalid counts retain the largest count observed in the window.
Metrics report observations without imposing arbitrary pass thresholds, freezing
a corpse, correcting a mesh, or modifying angular motion.

The getter API was checked against NVIDIA's published PhysX extension API and
the [PhysX 4.1 simulation-statistics reference](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/classPxSimulationStatistics.html).
The SDK is not installed here; the getter facade is not a native SDK build.

## Automated replay

From the PrimeXT root, with Python 3 and a GCC-compatible C++17 compiler:

```sh
python3 game_shared/tests/verify_j10_6_10.py --ubsan
python3 game_shared/tests/verify_j10_6_10.py --ubsan --inherited
```

The focused suite uses real production matrix/math code and actual recorder/
export method bodies. A getter-only PhysX facade and explicit UI/engine adapters
verify read-only behavior, broken/missing references, offsets, peaks, invalid
samples, export exclusivity and capture bounds. They do not simulate PhysX or
provide a native GUI. Prior editor-method tests cover control bounds/overlap and
repaint behavior with the actual declarations.

Full native builds remain necessary with the project's real vcpkg, PhysX 4.1,
Qt/Win32 and OpenGL dependencies. The supplied environment has a C++ compiler
but no CMake/Ninja, PhysX SDK, Windows SDK/MSVC, Qt/OpenGL development stack or
live game installation. Individual syntax/method checks are not target links.

Each delivery includes automated evidence, native acceptance checklist and
incremental/cumulative patch routes. Actual reproduction, in-game recordings
and FPS baselines remain pending; F0's live exit gate is not marked passed.
