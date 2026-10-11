# J.10.6.2: plan sections 3 and 4

Apply this increment to J.10.6.1, or apply the cumulative J.10.6.2 patch directly
to the original PrimeXT/Xash sources. Do not stack cumulative patches.

## Shared CPU pose

Prediction, physical pose messages and drawing share the same per-entity model
instance. Creating or resetting CPU state does not submit a draw or allocate
materials. Drawing initializes materials when needed and retains received poses.
Model/header changes, stale handles, reset-latch callbacks, message-clock rollback
and map teardown invalidate the instance. Teardown visits every live linked-list
entry and clears the corresponding entity handles.

Classic blends use the current/previous blending bytes. Extended parameters use
their authored defaults for untransmitted slots and previous entity-state controls
for transmitted slots. Controllers, mouth, gait, sequence fades, autoplay, IK,
procedural/jiggle bones, external bones and physical reconstruction run through
the shared evaluator. Its cache includes source state, previous pose controls,
the actual root transform, compatibility mode and sequence-blend policy; unchanged
state at the same sample time does not advance stateful stages a second time.

Ragdoll and part messages establish interpolation origins from a CPU evaluation
at receipt, even before drawing. Physical reconstruction includes unsimulated
ancestors, descendants, scaled bases and exit blends. Both consumers use it.

## Typed inventory

Modular duplicate eligibility reports the actual reachable default/loaded typed
rounds that fit into reserves. It is a read-only query and follows the same
transfer order as the grant method. A first acquisition is eligible with full
reserves. Multiplayer weapon-stay and living-player checks still apply.

`TryAddPlayerItem` distinguishes attached, consumed, retained and rejected items.
The existing Boolean `AddPlayerItem` interface still means attachment. A partial
duplicate updates inventory information and remains available; it does not
create a replacement on each partial grant.

Weapon boxes visit each packed item once, preserve partial/rejected item links
and ownership, subtract only actually granted loose ammo, and remove themselves
only when empty. Pickup feedback requires a transfer.

Modular respawn allocates an entity, installs its definition and authored spawn
recipe, then calls `DispatchSpawn`. Paint metadata and relevant spawn fields are
carried over; physical instance UUIDs are fresh. Runtime consumed rounds and
cartridge selections are not a respawn recipe. The authored cartridge/count
fields are saved separately. Old saves lacking them use the ordinary definition
spawn defaults.

## Verification

```bash
python3 PrimeXT-master/game_shared/tests/verify_j10_6_2.py
```

The driver compiles unchanged production pose/packet/draw methods with real
project Studio types and animation, IK solver, procedural and math code. Engine
services, IK scene locks and render bookkeeping have explicit adapters. It
compares bone matrices and transformed geometry rather than only freshness keys.
Covered cases include classic two/four-way blends, extended controls/defaults,
controllers/mouth/gait/sequence fades, first-query initialization, one evaluation
per unchanged sample, procedural geometry, scaled ragdoll ancestors/descendants,
part/exit transitions and model/reset/map lifecycle.

Inventory fixtures compile the production modular class, rules, player insertion,
box touch, world touch and respawn methods with explicit entity/services and a
save-state adapter. The registry, ammo policy and UUID generator are actual code.
They cover typed eligibility, 0/3/10 grants, second pickup, several packed items,
new-item attachment, loose ammo, restored retained contents, authored respawn
cycles and missing/unregistered definitions. The adapter is not a native engine
save-file test. Source assertions additionally check the saved recipe fields.

The Linux checks use real project headers, PhysX disabled and client weapon
prediction enabled. Full dependency-correct linking, SDK builds, Windows, actual
IK scene locks, renderer effects/follow-model merging and live maps remain
unverified. CPU fixture agreement is not certification of the full render/physics
pipeline. No replacement SDK or graphics headers are used to claim a full build.

This increment retains J.10.6.1. The shape-selection ambiguity repair, partial
physics promotion on death, native/live acceptance and separately documented
feature limits remain outstanding.
