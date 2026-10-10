# Milestone J.10.6.5 — conditional physics and shared pose timing

J.10.6.5 includes the ordinary monster death promotion introduced in J.10.6.3
and closes remaining gaps in the shared client physics-pose evaluator.

## Partial physics on death

When `sv_auto_death_ragdoll` is enabled, an eligible nongibbed monster calls
`EnableRagdoll(true)` even when its asset already owns a partial ragdoll. The
physics layer promotes every existing body to full physics weight, releases
kinematic anchors, stops animation control, and suspends the legacy entity hull.
Actors, joints and the saved entity/root relationship stay intact. Recent
impulses queued for animated anchors are transferred once. Repeated enable/death
calls preserve ownership and do not replay the killing impulse.

These server fixes are carried forward from J.10.6.3/J.10.6.4. J.10.6.5 adds
regressions that enter partial ownership through the production weight API with
drive weights 0, 0.0005, 0.25 and 0.998, then run the actual death path. Existing
cvar, gib, missing-asset, failed-creation and impulse-expiry policies remain
covered by the inherited method fixture.

## Prediction and drawing

Both consumers evaluate animation, IK/procedural bones, ragdoll ancestry,
descendants, part overrides and transitions through `StudioEvaluateCpuBones`.
J.10.6.5 uses engine client time for both consumers and one sample for each
packet's source capture, receipt and interpolation start. Renderer frame time
no longer shifts a physics blend or expires a fresh part override.

The draw publisher retains the CPU result and its actual sample stamp, allowing
a subsequent query to share the result. It continues to reject stamps for
parent-merged `MOVETYPE_FOLLOW` bones. Cache state includes any flags changed
while completing a transition or expiring an override, avoiding redundant
stateful evaluation by the next consumer at the same client sample.

An implicit clear from the entity lifetime bit reconstructs the physical pose
at the handoff sample before blending into animation. An idempotent shared
handoff preserves that source and start time when a delayed explicit clear
arrives. Repeated clears do not restart a completed exit. Reentry captures the
currently interpolated pose. Malformed snapshots are validated completely
before any evaluation or cache/ownership mutation.

If the animation source cannot be evaluated when a packet arrives, the first
successful CPU consumer captures its fresh source and starts the entry blend
at that sample. This prevents an unavailable sequence and a delayed draw/query
from skipping the blend by pairing a new source with an old receipt time.

## Verification

Run from `PrimeXT-master`:

```bash
python3 game_shared/tests/verify_j10_6_5.py --ubsan
```

The additional executable compiles actual production packet, pose, setup and
stamp-publishing methods with real Studio types, animation/IK/procedural and
matrix/vector code. Explicit engine-service/render-bookkeeping adapters supply
the unavailable engine services. Its eight cases cover delayed clears, clock
drift, single-sample packets, atomic malformed-input rejection, offscreen
handoff, reentry, shared IK/cache publication and delayed fallback capture.

The inherited death fixture compiles actual death, weight and physics-layer
methods against explicit entity/actor-service adapters. These are method-level
checks, not PhysX SDK substitute headers or a real solver/engine certification.

The acceptance driver also runs the inherited portable, source, native weapon
validator, editor and Linux translation-unit checks. Full PhysX SDK links,
Windows builds, and engine startup/live-map/save/network acceptance remain
separate. The supplied environment does not include the required PhysX SDK;
these checks do not establish real contact, sleep or solver behavior.
