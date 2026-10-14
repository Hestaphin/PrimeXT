# K.11.4.1.4 — bounded physics scheduling

Step 4 of the six-step FPS improvement plan, based on K.11.4.1.3.

## Runtime settings

| Setting | Value | Behavior |
| --- | --- | --- |
| `sv_physics_hz` | `60` (default) | Medium; fixed 16.667 ms rigid-body/ragdoll steps |
| `sv_physics_hz` | `30` | Low; fixed 33.333 ms steps |
| `sv_physics_hz` | `100` | Previous 10 ms cadence for comparison |
| `sv_physics_max_steps` | `4` (default) | Maximum solver steps per server update |

Both cvars are archived. Only 30, 60 and 100 are supported; other rate values
resolve to 60. The step limit truncates and clamps to 1–8; nonfinite values
resolve to 4. Changing the rate takes effect on the next physics update and
preserves the outstanding fraction of simulated time. It does not recreate
actors, joints, ownership or physical save records.

`r_physics_showmenu` opens the client **Video / Physics performance** selector.
The Controls action list also includes **Physics performance**, which can be
bound to a key. The selector changes the local server's cvar through the client
cvar API; it sends no command to an online server. A missing/non-server-DLL
cvar displays the local-game requirement.

The supplied Xash ZIP contains an empty `3rdparty/mainui` submodule directory.
The stock Options → Video page lives in that separate menu module. This release
therefore provides the selector through PrimeXT's existing in-game ImGui window
system and console/control binding. It does not modify the stock Video page.
That exact page integration needs the matching mainui source.

## Bounded catch-up and time accounting

The old unbounded 100 Hz accumulator loop could turn one long frame into many
solver calls. The runtime now accepts at most 250 ms of input per update and
runs at most the selected step limit. Excess whole steps are discarded and
only the fractional step is carried forward. Discarded time is measured; it
cannot reappear as a catch-up backlog on the following frame. The SDK always
receives a supported fixed timestep, including after stalls or rate changes.
Ordinary under-budget updates account for elapsed simulated time independently
of the number of display/server calls. A float-rounded complete step runs
immediately, instead of waiting for a strict `>` comparison.

At the default four-step limit, the per-update simulation budgets are 133.333 ms
at 30 Hz, 66.667 ms at 60 Hz and 40 ms at 100 Hz. For example, with no carried
fraction, a 200 ms update at 60 Hz runs four steps and discards eight steps
(133.333 ms). No following-frame backlog remains. During sustained overload,
physical motion advances more slowly than the game clock. Raising the limit
trades more catch-up CPU cost for fewer skipped milliseconds.

Scene/server inactivity and whole-world teardown clear pending time. Map/
restore generation changes, backward game time and negative/nonfinite frame
input clear it too. Zero input contributes no time. Transient timing state is
not saved in game files; the existing articulated records/layout stay intact.

## Force timing and longer-step drives

Each step applies water, held-item and partial-ragdoll drives, then submits the
solver, waits for completion and captures articulated poses. Touch dispatch,
animation following and collision synchronization retain their server-update
cadence. Water buoyancy/drag uses current completed body state on each substep;
it no longer piles up on frames with zero steps or disappears after the first
step of a catch-up batch.

Held-item acceleration drives and partial-ragdoll force/torque drives use
backward-Euler PD gains at 60/30 Hz. Linear force gains account for body mass;
angular gains use the largest principal inverse inertia to bound every axis.
The original 10 ms drive gains remain at 100 Hz. Authored masses, inertia,
stiffness, damping, solver iteration counts, CCD settings and force/torque caps
are retained. Longer-step water drag uses the same damping policy to avoid
drag alone reversing a body's velocity.

Gameplay `AddForce`/`AddTorque` Force and Acceleration inputs integrate the
current server frame's duration into impulse/velocity-change units once. Their
accepted duration is capped at the same scheduling budget. Several fast server
calls before one solver step therefore contribute their elapsed time instead
of multiplying a force by the larger solver timestep. Explicit Impulse and
VelocityChange retain one-shot units, including on zero-duration frames.

Single-use push triggers, gib launch forces and conveyor wake kicks now specify
one-shot impulses explicitly. Continuous push fields use a constant force at
the previous 100 Hz / 100 FPS reference strength, replacing division by render
frame duration. Push behavior at other rates/frame frequencies intentionally
changes to remove the earlier timing dependence. Gameplay force callbacks are
still dispatched at their existing times, so contacts under different discrete
steps need native gameplay comparison.

## Presentation and retained behavior

Server collision and entity bounds use completed solver poses. They are not
extrapolated or moved to a visual interpolant. Ragdoll presentation and part
snapshots are published once per server update; trigger/portal relinking still
runs. Existing active snapshot target intervals and sleeping/frozen heartbeats
remain in place. The client's receipt-paced interpolation starts a new blend
from the currently displayed pose, keeps its bounded 30–100 ms active duration,
and renders each display frame. It does not assume a 10 ms solver interval.
Tests exercise this production path with 30/60/100 Hz and live rate changes.

Step 2 exact collision/pose caches and Step 3 sleep reuse remain active. Frozen
bullet/blast wakes, partial/full/death handoffs, authored joint/contact profiles,
savegame formats and pxmv Full/Partial controls remain covered by regressions.
The editor's reference preview cadence stays 100 Hz. Rendering budgets are
Step 5; the ballistic integration interval is Step 6 and is unchanged here.

## Profiling and comparison

The compatible `PERF_K11411` schema and off-by-default profiling remain. The CSV
extractor now accepts the new `hz` unit. New rows are:

| Metric | Unit | Meaning |
| --- | --- | --- |
| `physics_hz` | hz | Resolved solver rate |
| `physics_dropped_time` | ms | Input cap plus discarded catch-up steps in this update |
| `physics_remainder` | ms | Fractional time retained after this update |
| `physics_catchup_clamps` | count | Updates with discarded simulation time |
| `physics_clock_resets` | count | Generation/time resets observed by the scheduler |

Existing `physics_debt` records pending time before this update's cap/steps;
`physics_steps` records actual SDK submissions. Compare same-map Release
captures with `physics_medium.cfg`, `physics_low.cfg` and `physics_legacy.cfg`.
The 100 Hz option includes the cap and force fixes; use a separately built
K.11.4.1.3 for the complete previous-version comparison.

Capture active ragdoll piles, quiet settled bodies, partial drives, held props,
hinges, water/drag, push fields, gib launches, conveyor wakes, bullet/blast
wakes, pause/unpause, live rate changes and save/load. Compare `physics_steps`,
`physics_wait`, `physics_drives`, new skipped-time rows, and p95/p99 frame
intervals. Validate body/joint contact quality at 30 Hz before using Low widely.

## Validation

```sh
python3 game_shared/tests/verify_scheduling_k11414.py --retained
```

The runner tests the scheduler's elapsed-time accounting at multiple display
frequencies, stalls, boundaries, invalid cvars/input, rate switches and lifecycle
resets. It executes the production Update, PD drives, water/frame-force methods
and menu with recording services under ASan/UBSan at O0/O2. Independent motion
integration checks drive/drag convergence and frame-force accounting. Actual
client Studio/IK/matrix code checks receipt-paced interpolation, rate switches,
delayed/old packets and shared prediction/draw. Changed gameplay caller and UI
registration objects compile against native Linux project headers. Retained
sleep/cache/profiling/render/map/ballistic suites run too. LeakSanitizer is
disabled because this environment does not expose the required process files.

These checks do not link the full game/ImGui or the native PhysX SDK, simulate
real joint/contact behavior, run a native Windows build, exercise the actual
menu/GPU, or measure in-game FPS. Native stability and performance captures are
still needed on the target machine; the adapter convergence tests establish
controller arithmetic rather than certification of a complete ragdoll solver.
