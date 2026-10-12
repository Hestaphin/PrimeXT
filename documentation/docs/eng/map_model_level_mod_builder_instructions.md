# Current update — J.10.6.10.3 / F3

**Current runtime format is `.phys` v5 only.** Regenerate all older assets from their matching Studio MDLs using the updated pxmv. Versions 1–4 are rejected; do not relabel them. Profiles/overrides, D6 velocity resistance, projection, sleep/solver/CCD/margins and selective pairs are described in [the current physics guide](physics_assets.md) and [F3 schema](../../milestones/J.10.6.10.3/SCHEMA_V5.md). Preserve collision/bodygroup/hinge/part functionality when authoring new v5 assets. Xash has no incremental F3 change.

The historical milestone sections below retain design/API details. Their old version/compatibility/no-conversion statements are superseded by the current v5 requirement. Native PhysX/Qt/Windows builds, actual-model drops, game/FPS and F4 rendering repairs remain pending; follow [F3 acceptance](../../milestones/J.10.6.10.3/ACCEPTANCE.md).

---

# PrimeXT Map / Model / Level / Mod Builder Instructions

**Current milestone: I.9.6 — editor preservation, accurate hinge preview and positive-uniform articulated scale.** `.phys` format **v4**, Xash3D-FWGS source unchanged. This guide is maintained alongside each engine, physics, or authoring update. The sections for earlier milestones remain below for migration and compatibility.

## Milestone I.9.6 — Editor and scaling corrections (after I.9.5)

**Source boundary:** The original PrimeXT and Xash ZIPs plus the combined I.9.5 patch are the baseline. The supplied milestone notes do not define *B10* and *B11* individually, so this section documents the concrete repairs without claiming those labels are fully satisfied. No Xash engine source edits or `.phys` format/protocol changes were needed.

### Physics Asset Editor

- Pending body or joint numeric edits are committed when switching list selections rather than being lost as lists refresh. Invalid values block that operation and keep the selection.
- Non-finite and malformed dimensions, offsets, angles, damping, mass, drive settings and hitgroup values are rejected rather than silently replaced/clamped when applying fields. A positive finite shape dimension and mass are required; hitgroups must be integers in `[0,255]`.
- **Simulate** creates PhysX revolute joints for `hinge` definitions and D6 joints for normal ragdoll constraints; the revolute joint uses authored child-local axis, anchor and limits, matching server hinge-frame construction. Preview still does not emulate driven motor/gameplay damage/release policy.
- Auto Generate asks before replacing existing authored bodies/joints; Reload warns before losing even unapplied text fields.
- New **Asset resize / Scale .phys** control permanently scales saved `.phys` collision vertices, primitive extents, per-body offsets and hinge anchors together. Angles, hinge axes, joint limits, authored mass, damping, and damage thresholds stay unchanged. Scale .phys **once** after permanently resizing the source MDL's mesh *and* bones; do not use it for an entity's runtime scale because runtime applies that already.

### Runtime scaling

- A positive *uniform* scale now applies consistently to ragdoll reference bones, live Studio body pivots, body-local offsets, primitive shapes and hinge anchors, so the articulated pose and constraints use the same dimensions.
- Interactive hinged parts also scale their reference skeleton, hinge anchor and primitive fallback shapes. Convex/triangle meshes retain their existing `PxMeshScale` transformation.
- **Non-uniform, zero, negative, non-finite and mirrored scaling remain unsupported for articulated dynamic hinges/ragdolls.** The engine rejects those constraints instead of pretending they match. Non-uniform scale of non-articulated static collision geometry remains on the existing I.9.5 path.
- Changing the entity's scale while its PhysX actors are already active is not supported; destroy/recreate them after changing scale, and validate networking/rendering in a full engine build.

### Acceptance checks

1. Use a two-bone hinged door with both mesh and bone positions permanently scaled by a factor of 2, apply **Scale .phys** with `2`, then compare preview hinges, contact wireframe, and Studio artwork. Reload the asset and verify the numeric fields persist.
2. Test an unmodified `.phys` with *runtime* `scale 2` and `scale 0.5` for a ragdoll and an interactive door. Do **not** use the editor scale control for these tests. Compare limb contacts, nearest-hit bullet ordering, joint anchors and client/server poses.
3. Try model scale `2 1 1` on a dynamic hinge/ragdoll: simulation must not create an unscaled/partially scaled articulated constraint; report the warning. Existing animated/static mesh behavior is separate.
4. In the editor, modify one joint and one body without pressing Apply, then switch to another body, Save and Reload: changes should persist. Input `NaN`, negative radius, `1.5` hitgroup or an invalid hinge axis and verify the asset is not silently corrupted.
5. Run a full PhysX SDK + GL tool build and in-game multiplayer tests. The supplied ZIP does **not** contain the PhysX SDK/vcpkg installation, so portable tests and editor non-PhysX syntax checks are not final runtime certification.

---

## Milestone I.9.2 — Full-ragdoll collision ownership and bodygroups (B4)

**Source repair:** A full/partial articulated ragdoll now suppresses the **root .phys collision**, bone-bound animated child shapes, and their query flags while articulated bodies own contacts. Switching the visible model's bodygroup while ragdolled changes the selected root geometry but does **not** reactivate root PhysX contacts. Likewise, a generic collision-enable request made during ragdoll does not reactivate the superseded root shape. After leaving the ragdoll, the root is restored according to gameplay's requested collision state and the **currently selected** bodygroup; disabled/empty variants remain off. If neither the selection nor the gameplay request changed, the pre-ragdoll simulation and scene-query flags are restored separately rather than normalized to one value. The previous gameplay collision enable/disable intent is kept separate from temporary physics suspension. No new Xash3D changes are required.

**Model builder impact:** No `.phys` file conversion is necessary. `.phys` v4, bone names, `bodygroup`, collision sections, `disabled 1`, hinge/drive settings and the Physics Asset Editor authoring workflow are unchanged. For assets mixing `collision` sections and articulated `body`/`joint` definitions, make sure the collision shapes are not separately duplicated by your mod's gameplay code.

**Map / level / mod acceptance checklist** (requires a complete PhysX-enabled engine build):

1. With a model having both root collision/bodygroups and ragdoll bodies, activate a full ragdoll and send a moving box across the *old root position*: it must not hit a ghost root collider.
2. While the ragdoll is active, change root bodygroup to a different mesh, then to a `disabled 1` variant, then back to an enabled variant. Neither simulation nor scene queries may see the root until articulated ownership ends.
3. Toggle gameplay collision off then on **during** articulated ownership. This must preserve future intent but must not enable the hidden root actor until physics ownership returns.
4. Exit articulated mode and check that the currently selected variant becomes solid only if gameplay collision was enabled. Both `disabled 1` and explicit collision-off must remain non-solid. Repeat on a model with no `.phys` mesh (metadata-only legacy root).
5. Test a failed ragdoll activation, an entity model switch and actor deletion for stale shape flags/contacts, including active animated door proxies. The I.9.2 portable tests verify the ownership policy and source wiring, **not** live contact generation.

**Not changed by I.9.2:** Existing issues B2/B3/B5–B9/B11/B12, including predicted client geometry, partial-ragdoll hitbox authority, ragdoll persistence and scaled animated pivots. A full PhysX/SDK server build and in-engine contact/trace verification remain necessary.

## Milestone I.9.1 — PhysX raycast compatibility (B1)

- **What changed:** removed the unsupported PhysX 4.1 `PxHitFlag::eDISTANCE` request from three server raycasts (ragdoll body queries and two physics-part bullet paths). Position/normal requests and the returned hit distance remain in use.
- **Builder impact:** **none**. No change to `.phys` v4, Studio bones, animation, map entities, collision setup, textures/materials, save formats, or builder/editor UI. Existing I.9 `.phys` assets need no conversion.
- **Verification:** the source regression check `python3 game_shared/tests/physx_hitflags_i91_tests.py` verifies these flag combinations, but a full PhysX-enabled server build and real-game tests are still necessary. Other bugs from the independent H→I.9 review are **not** addressed by this focused B1 patch.
- **Recommended tests when SDK is available:** compile the PhysX server; shoot a ragdoll limb, an animated door and a dynamic/free hinged part; confirm position/normal/distance values, nearest-hit wall priority, decals and single-impulse policy.

## Milestone I.9 — Interactive articulated-prop authoring

### Scope and validation status

Milestone I.9 adds actual Physics Asset Editor fields for interactive physics parts, their animation/physical drive parameters, hinged joints, local pivot/axis, and break/damage limits. It also adds a tested **schema-only** car-door example at `documentation/examples/interactive_car_door.phys`, and editor guardrails. It does **not** change the version-4 `.phys` file format, protocol, server gameplay API or Xash3D source. The example file cannot run alone: you must have a compatible `car.mdl` with correctly named bones. In-game PhysX, multiplayer and savegame behavior require a full PhysX-enabled build and runtime testing; the SDK is absent from the supplied ZIP.

### Model builder: make a car door

1. Rig `car.mdl` with a stable chassis/root Studio bone, such as `car_root`, and a separate **rigid** door bone `door_l`. Weight the complete door mesh to `door_l`; the hinge origin/reference skeleton must remain stable across sequences. The `documentation/examples/interactive_car_door.phys` sample uses those two bone names. Change them for your own model.
2. Load the car in **pxmv → Physics Asset Editor**. If converting an earlier `_cp.mdl`, use **Import _cp**, inspect the animated collision wireframe and save its new `.phys`. The `_cp` file is an editor import source only; do not ship it as runtime collision.
3. Add a **chassis body** with bone `car_root` and a simplified box/convex collision shape, positive mass and an appropriate size. Add one **door body** with bone `door_l`, one physical actor per rigid bone, and a smaller simplified box. Do not duplicate its dynamic collider as a second overlapping proxy.
4. Select the door body and press **Make Part**, or fill its interactive panel manually. Give it a **stable unique Part name** (`left_door`), set **Drive mode** to `Animated`, `Driven` or `Free`, and author **Strength**, **Damping**, and **Max force**. `Driven` needs strength greater than zero. The shortcut starts with *illustrative* drive values 120/15/850. `None / ragdoll` remains for legacy body editing.
5. Select the door body and press **Add** in the Joints section. If it is already an interactive part, the editor starts a hinge template. Otherwise select the new joint and press **Make Hinge**. Confirm **Parent** is the chassis body and **Child** is the door. Enter the limits, **Anchor XYZ** and **Axis XYZ** in the **child Studio bone's local coordinates**. The template axis `0 0 1` is an example only; use the axis of the actual door hinge, and keep the hinge axis nonzero.
6. Set **Release dmg**, **Break dmg**, and **Break impulse** as needed (`0` means the threshold is disabled). Example values `30`, `100`, `900` are tuning placeholders, not universally correct damage scales. For a typical progression, use `release_damage` smaller than `break_damage`.
7. Apply joint changes, apply body changes, **Save** and **Reload**. The editor validates the model bones, part names, hinge graph/limits/axis and numeric fields. Saving no longer silently discards unsaved joint changes. Use the editor wireframe preview while your animation plays. Be aware that pxmv's optional **Simulate** preview is primarily a ragdoll preview, **not** an end-to-end test of bullet-driven car doors.
8. **Auto Generate** replaces bodies/joints. I.9 blocks it when interactive parts or authored hinges are present, to protect this manual work. Back up and deliberately remove those definitions if regeneration is desired. **Reload** asks before discarding unsaved edits.

### Mode selection and mod/mapper behavior

| Authored `prop_drive` | Intended runtime ownership | Practical use |
|---|---|---|
| `animated` | Studio pose controls the collider | Scripted/non-reactive door |
| `driven` | PhysX dynamic actor and motor target from current Studio pose | Reactive hinged door that springs toward the animation |
| `free` | PhysX dynamic actor without motor | Unpowered/damaged swinging door |

The model builder chooses the default in `car.phys`. The **server mod's gameplay logic** chooses mode changes (`SetInteractivePartDriven`, `SetInteractivePartFree`) and applies bullets/explosions/damage (`DamageInteractivePart`). Xash does not choose this mode for you. No generic per-map-entity mode override is introduced by I.9; write mod/entity code for instance-specific behavior where needed.

### Map/level test checklist (requires real PhysX SDK and running engine)

- **Animation ownership:** Open and close the door through Studio sequences before impact; debug the visual and collision poses together, including intermediate frames and moving/rotating chassis.
- **Hit occlusion:** Shoot the open door, the chassis, a door behind a wall, and several shotgun pellets. A wall/closer hit wins; each accepted pellet has one impact/impulse and effects still sample the visible `.mdl` surface.
- **Drive response:** Shoot a `driven` door at the edge and near the hinge and check natural torque, damping, motor force limits and no snaps when changing `animated ↔ driven ↔ free`.
- **Break behavior:** Damage below threshold, across `release_damage`, then across `break_damage` or `break_impulse`. The hinge detaches but the same PhysX actor continues; no duplicate collision actor remains.
- **Client state:** Run a server plus two clients. Verify `PartPose`, first-snapshot blending, interpolation, off-PVS/late-join behavior, sleep/wake, ragdoll takeover, model swaps and entity removal. Check only `door_l` and its proper children override animation, not the chassis.
- **Savegames:** Save and reload intact, Driven, Free and detached doors; verify part names, damage, hinge state, physical transform, angular/linear velocity and cleanup. **At most 8 interactive-part records per entity are persisted** in I.8/I.9; more can simulate, but only the first 8 are saved. The editor warns after saving more than eight. Savefiles are not migrated across arbitrary mod binary revisions.
- **Lifecycle:** Remove the car, unload the map, rebuild the bodygroup, toggle collision and respawn the entity. Assert no dangling hinge/part pointers or duplicate dynamic bodies. A detached door still lives with its owning car entity, not as a separate persistent game entity.

### Compile/runtime caveats

The SDK-independent `.phys` regression suites, editor syntax checks and patch replay can be verified from these source packages. They do **not** prove PhysX solver operation, tool GUI behavior on a real display, multiplayer render quality or save/restore in a live game. For those checks obtain PrimeXT's recursive PhysX/vcpkg dependencies and build and launch the full client/server and pxmv toolchain. The supplied ZIP's `external/vcpkg` checkout is empty and does not provide `PxPhysicsAPI.h`.

The example asset is intentionally text-only and requires a separate matching `car.mdl`. Keep `.mdl` in charge of visible meshes/materials/decal/sounds and `.phys` in charge of the authored collision and physical bodies. If a model has no `.phys`, PrimeXT legacy behavior remains the fallback.

---


**Current:** Milestone I.8 / `.phys` version 4. Update this guide with every future feature.

## Milestone I.8 — Multiplayer part poses and bounded save/restore

**Current milestone: I.8; `.phys` version 4 (unchanged).** PrimeXT handles this feature without changes to Xash3D-FWGS source. The I.8 network protocol is new; server and client binaries must be built from compatible I.8 source. The PhysX translation units and actual in-game behavior **have not been compiled or tested in this environment** because the complete PhysX SDK is absent.

### For model builders

- Keep your car chassis and door in the same `car.mdl`. Give each rigid moving door its own named Studio bone (for example `door_l`). Skin the entire rigid door to that bone; keep a correct reference pose and hinge pivot. Animated root/chassis geometry remains animated normally.
- Author `car.phys` v4 as in I.7. Give the moving door a `body "left_door"` whose `bone "door_l"` and `part "left_door"` names resolve, with a single kinematic/dynamic part actor and an authored hinge `joint`. `prop_drive animated` does not generate physical overrides; `driven` and `free` can. Your damage thresholds remain effective after I.8.
- If a physical door is detached, I.8 removes its hinge on the server and overrides the `door_l` bone on clients using its **world-space PhysX pose**. The part remains owned by the car entity, not an independently instantiated/pickup-capable model/entity. Parent entity removal or model replacement still destroys the door. A detached door can only render while its car entity is visible and replicated.
- A nested moving bone whose animation is not rigid (multi-bone deforming panels) should not be exported as one rigid physics part. Verify bone hierarchy and collision geometry in the Physics Asset Editor.

### For mappers / level builders

1. Put the animated car entity in the map and set its normal model to `models/car.mdl`, with `models/car.phys` next to it. No `_cp.mdl` runtime file and no new Xash engine patch are needed.
2. Drive scripted opening by Studio animation as usual. The mod can activate the interactive part with `SetInteractivePartDriven("left_door")` or `SetInteractivePartFree("left_door", true)`; bullets and explosions can activate eligible driven/free parts through the I.5–I.7 policy.
3. Check the door moving beyond the chassis: both its server-side hit area and its **client render bone** should follow its current physical pose. Shots should not pass through a nearer wall, and a single pellet must not generate duplicate impulses.
4. Save a game while a door is Driven, Free, or Detached. Reload and check mode, cumulative damage, hinge breakage, actor position/orientation and linear/angular motion. The engine saves up to **eight parts per animated entity**; beyond eight, later records are omitted with a warning. Keep critical persistent parts within the first eight author-defined parts for now.
5. Test two network clients entering/leaving PVS, doors sleeping/waking, a door detaching, reconnecting, parent entity deletion, map changes and model changes. A stale pose on the client expires rather than permanently overriding animation.

### For mod programmers

The I.8 server sends a fixed-size `PartPose` version-1 message containing entity index, sequence, bone index, flags (clear/detached), and uncompressed world-space translation/quaternion for **one physical Studio bone**. Live parts are refreshed approximately at 20 Hz and sleeping parts at 2 Hz while visible. A change to Animated sends a clear. Clients discard old packets and interpolate current bone transforms; child bones inherit their animated local transforms. Ragdoll snapshots are separate and take presentation priority.

I.8 uses normal `CBaseAnimating::Save`/`Restore` and a compact per-part record containing stable name, mode, detached flag, damage, pose and velocities. State is reapplied to the existing recreated part actors and surviving joints; a saved detached hinge is released again on restore. This is **same-entity savegame persistence**, not persistent detached-door entities across parent deletion or arbitrary level transitions. Ensure custom entities overriding `Save`/`Restore` call the inherited implementation. Changes to `.phys` part names between save and restore prevent matching that record.

### Version/compatibility guidance

- `.phys` versions 1–4 still load, with no schema bump for I.8. The standalone patch requires the I.7 source baseline and has no Xash modifications.
- Update both server and client libraries together when deploying the new network user message.
- Do not expect PhysX solver tests, real multiplayer behavior, detached-door independent entity lifetime, level-transition persistence or engine savegame compatibility across different game/mod versions to be confirmed by portable syntax tests alone.
- I.9 remains responsible for editor controls, shipped examples and final PhysX-enabled integration testing.

---

## Milestone I.7 — Damage, motor release and detachable hinges

**Implemented in PrimeXT server source on top of I.6.** `.phys` format remains **v4**; Xash3D-FWGS remains unmodified. Full PhysX SDK compilation/in-game simulation and networked visual pose remain **unverified**; I.8 is still needed for client rendering, persistence, and independently networked detached parts.

### Model builder — author joint failure policy

Keep the single `car.mdl` with a rigid door bone `door_l`. Author ONE `.phys` interactive door part with a convex or primitive collider, mass and a hinged joint to `chassis`:

```text
physics_asset
{
    version 4

    body "chassis"
    {
        bone "car_root"
        shape box
        half_extents 40 18 12
        mass 1000
    }

    body "left_door"
    {
        bone "door_l"
        part "left_door"
        shape box
        half_extents 16 2 14
        mass 18
        hitgroup 20
        prop_drive driven
        drive_strength 120
        drive_damping 15
        drive_max_force 850
    }

    joint "left_hinge"
    {
        parent "chassis"
        child "left_door"
        kind hinge
        hinge_anchor 0 0 0
        hinge_axis 0 0 1
        hinge_min -5
        hinge_max 75
        release_damage 30
        break_damage 100
        break_impulse 900
    }
}
```

The hinge pivot/axis remain authored in **child-bone reference-pose local space**. Model scale must be 1 1 1 for the current interactive hinge implementation. The above sizes, mass, gains and thresholds are **illustrative starting points**, not calibrated production values.

### Thresholds and who chooses them

- **Model/asset builder:** authors defaults in `.phys` joint block. `release_damage` is cumulative damage at which a Driven/Animated hinged part releases to **Free**. `break_damage` is cumulative part damage at which the joint detaches. `break_impulse` is **one bullet/blast impulse magnitude**, not cumulative damage and not a PhysX solver-force threshold. All are optional; `0` disables the respective trigger.
- **Mapper/mod programmer:** may use an entity configured with this asset, and trigger scripted part damage via `pCar->DamageInteractivePart("left_door", 25.0f);`, independently of normal entity HP. The scripted API accumulates damage but does not create a physical impulse.
- **Runtime:** bullets count their already-computed actual damage **once** against the selected part; explosives count their per-part radial falloff while keeping the I.5 single impulse budget shared among parts. A crossing of `release_damage` removes the motor constraint only (hinge remains). A crossing of `break_damage` or `break_impulse` releases the **joint**, not the single door actor. The door keeps its collision shapes and existing linear/angular motion and continues in **Free** mode.

### Mode transitions

```text
Animated (no thresholds) --hit--> Animated (no physical reaction)
Driven --normal bullet--> Driven + motor reaction/recovery
Driven --release_damage reached--> Free, still hinged
Free --break_damage or break_impulse reached--> Detached, dynamic rigid body
Animated --authored release/break threshold reached--> Free / Detached
```

Important: If `break_damage` is lower than `release_damage`, the joint can detach directly. Once detached, `SetInteractivePartDriven` or `SetInteractivePartFree(..., false)` cannot reattach it; it remains freely simulated until the owning car entity/model is removed/rebuilt. No separate detached-door game entity is spawned in I.7.

### Map and mod integration checklist

1. Test each part name (case-insensitive) and Studio bone. Keep the car/chassis and door on separate logical physics parts; avoid duplicate door colliders.
2. Fire several low-damage shots: confirm damage accumulates and triggers **one release** when the authored threshold is reached. Then confirm subsequent damage detaches only once.
3. Try one powerful shot meeting `break_impulse`, including while the door is mid-swing. Confirm its pose/velocity are retained and the former hinge motor ceases to act.
4. Compare bullets near the hinge and outer door edge, and shoot a door behind a wall: world occlusion must win; each pellet must produce no more than one impact against that part.
5. Test explosions with two doors, disabled collision bodygroups, moving chassis, and a door already detached. The entity-level blast discovery/LOS limitation from I.5 remains.
6. Test changing collision variants and explicit collision enable/disable during movement and after detachment. **Do not** use a model replacement as a persistence mechanism: it reconstructs physics actors and resets unsaved part damage/detached state.
7. Verify removed entities, level change and scene reset release actors and joints without double release. Full in-game verification still requires a PhysX-equipped build.
8. For multiplayer, the server may simulate a detached door while clients still draw the Studio animation. **Do not ship network-visible damage/detachment as a finished visual feature until I.8 pose networking and save/restore are implemented.**

**Backward compatibility:** v1–v3 remain readable; old v4 assets without new thresholds continue to work as before (zero means no automatic release/detach). The Physics Asset Editor UI for the new thresholds is planned for I.9; builders can edit the text asset manually now.

---

## Milestone I.6 — Driven hinge motor and animation recovery

**Current status:** I.6 adds a server-side `Driven` runtime mode, independent of
`Animated` and `Free`. `.phys` remains version **4**. PrimeXT changes only;
**Xash3D-FWGS stays unmodified**. No final PhysX SDK compile/gameplay or
client-visible network bone-pose verification has been performed.

### Model builder: car-door authoring

Keep one rigid `door_l` bone, one dynamic-compatible convex/primitive `left_door`
part, and one real `hinge` referencing the chassis. Set unit model scale and
correct hinge axis, anchor and angle limits in the Studio *reference pose*.
Provide an animation sequence that defines the **target** swing angle.

```text
body "left_door"
{
    bone "door_l"
    part "left_door"
    shape box
    half_extents 16 2 14
    mass 18
    hitgroup 20
    prop_drive driven
    drive_strength 120
    drive_damping 15
    drive_max_force 850
}

joint "left_hinge"
{
    parent "chassis"
    child "left_door"
    kind hinge
    hinge_anchor 0 0 0
    hinge_axis 0 0 1
    hinge_min -5
    hinge_max 75
}
```

`drive_max_force` is **optional** for `.phys` v4; a missing or `0` value
uses the server's fallback motor force limit of **4000** physics-force units.
Positive values specify a maximum motor torque (clamped at runtime to
1,000,000). Invalid negative/non-finite values are rejected on asset load.
`drive_strength` is a positive proportional target-angle gain and
`drive_damping` dampens relative angular speed; the motor command is capped
at 20 radians/second. These values are **tuning parameters, not a guarantee of
settling time**. Small angular errors should not produce giant impulses.

### What happens after a shot or explosion

- `prop_drive animated`: stay kinematic; impact detection, damage and effects
  still work, but no automatically applied physical impulse.
- `prop_drive driven`: first qualifying impact enables the existing dynamic
  hinge actor **with the motor enabled**. Impacts displace the door, and the
  joint motor attempts to move it toward the current Studio animation target.
  The door remains in `Driven`; the system does **not** teleport it back.
- `prop_drive free`: first qualifying impact enables the dynamic hinge actor
  **without motor control**, so it can swing freely within hinge limits.
- An already-physical Driven or Free part accepts additional bullet impulses
  on the same authoritative part actor. Physics collision, query and material
  behavior established in I.5 are preserved.

### Mapper / mod programmer

Use `pCar->SetInteractivePartDriven("left_door")` to request driven physical
recovery. Use `pCar->SetInteractivePartFree("left_door", true)` to explicitly
release the motor, or `SetInteractivePartFree(..., false)` to request
**Animated**; that final transition still refuses to snap the door when the
pose is not near its animated target or the part is moving too fast.
The motor follows the ongoing Studio bone animation, not a fixed closed pose.
The current server physical pose can still be queried via
`WorldPhysic->GetPhysicsPartWorldPose(...)`.

### Validation checklist for map and mod builders

1. Animate the door closed / partly open / fully open and confirm the hinge
   motor pursues each current target rather than pulling it to a hard-coded
   zero angle.
2. Compare `animated`, `driven` and `free` on otherwise identical doors.
3. Hit the edge and hinge with small and large rounds. Driven should deflect
   and attempt recovery; Free should not recover; Animated stays on sequence.
4. Test a moving chassis, repeated impacts, obstacles and simultaneous doors
   for solver oscillation, penetration and unexpected force amplification.
5. Disable a bodygroup collision variant: the corresponding motor must stop
   while collision is inactive, then resume when restored.
6. Watch the PhysX joint limits in debug mode; verify angle sign and anchor.
7. Check walls and nearer objects still block bullets and explosions still
   obey I.5 visibility/impulse-budget rules.
8. Do not deploy network-visible physical parts in multiplayer until the I.8
   client bone-pose sync/restore tests are completed. Detached hinges remain I.7.

**Build limitation:** portable schema/motor tests can run without PhysX; the
actual motor/constraint simulation must be compiled and tested with the full
PhysX/vcpkg SDK. This document records source behavior, not demonstrated
real-time engine behavior.

---

## Milestone I.5 — Bullet and explosion reactions (historical I.5 behavior) for interactive `.phys` parts

**Status:** Server-side impact dispatch and physical impulse sources are implemented in PrimeXT. The `.phys` schema stays at version **4** (v1–v4 assets remain loadable); no Xash3D-FWGS source modifications are needed. Complete PhysX linkage and in-game validation still require the dependency-complete engine build. Client-side physical bone visualization remains a future I.8 task.

### Model / physics asset builder — configuring a car door

- Author a rigid `door_l` Studio bone in `car.mdl`. Keep the door collider rigidly attached to this bone. Author *convex* collision geometry (or the v4 primitive body) for parts expected to become dynamic; several convex sections may share the `left_door` part/actor.
- Assign a v4 `body "left_door"`, `bone "door_l"`, `part "left_door"`, a valid positive `mass`, and optionally an application-specific `hitgroup` (e.g., `20` for the left door). Connect it to the chassis with a real `joint kind hinge`, correct `hinge_anchor` and `hinge_axis` and angle limits. Model scale must be `(1,1,1)`.
- `prop_drive animated`: receives precise bullet collision, effects and normal game damage, but **does not automatically move physically** on impact. Use for scripted doors that must follow animation.
- `prop_drive driven`: at I.5 a bullet/explosion released this hinged part into Free. I.6 supersedes that policy; see the section above.
- `prop_drive free`: as of I.5, also starts in the animation-controlled pose and transitions to a freely hinged PhysX body when a qualifying impact occurs. This is **not an automatic initial free spawn**; game code can request early Free mode using the I.4 API.
- For collision-only v3 moving parts with no v4 hinged body, bullet precision still works, but the part cannot respond dynamically until you author the body/joint. If geometry cannot be safely simulated dynamically, no release/impulse is applied.

Minimal **illustrative** v4 metadata (for full syntax, refer to the complete I.3/H examples below):

```text
body "left_door"
{
    bone "door_l"
    part "left_door"
    shape box
    half_extents 16 2 14
    mass 18
    hitgroup 20
    prop_drive driven
}

joint "left_hinge"
{
    parent "chassis"
    child "left_door"
    kind hinge
    hinge_anchor 0 0 0
    hinge_axis 0 0 1
    hinge_min -5
    hinge_max 75
}
```

If the model also contains a v3 collision section for the door, that section and the v4 body must **refer to the same Studio bone and logical part** so I.2's single actor owns all shapes. The primitive `shape box` is used for the dynamic collider only if no v3 collider is attached to the part. Do not author a second distinct collider actor over the door.

### Level and map builder

- Use a PrimeXT entity capable of creating its PhysX root body and loading `car.phys`; ensure `.mdl`/`.phys` paths and bodygroups match.
- Check bullet impacts with the door closed, halfway open and fully open. A primitive-only hinge door or a door protruding beyond the root hull now participates in a supplemental nearest-hit query. A **wall or closer object always wins**; the door must never be struck through intervening geometry.
- Verify disabled collision bodygroups and explicit gameplay collision-off states stay non-colliding. Use the debug visualization to check one collider per part, hinge axes, and active bodygroups.
- Explosions use the map entity's radius/visibility/water rules as coarse discovery; extremely large articulated objects with their entity origin outside the radius may still be missed.
- Author-driven physical doors will not visually track server simulation on remote clients until I.8 is implemented, so do not yet ship multiplayer-visible physical doors on production maps.

### Mod builder / programmer

- Player and monster bullet paths now call **one** `ApplyPhysicsBulletImpulse()` per pellet after the normal trace. It selects the nearest part without bypassing the existing world result; hits are not duplicated by the physics pass.
- At I.5, eligible `driven`/`free` hinged parts were promoted with `SetPhysicsPartMode(..., Free)` and receive `PxRigidBodyExt::addForceAtPos` at the actual physical impact. Already-Free parts receive another impulse. `animated` parts continue tracking Studio animation until a mod explicitly changes mode.
- Explosions apply one impulse **budget per hit entity**, divided across eligible part bodies by distance falloff. An entity can react physically even with `takedamage = DAMAGE_NO`; ordinary damage behavior remains unchanged.
- Tuning cvars: `sv_prop_bullet_impulse_scale` (default **8**) and `sv_prop_blast_impulse_scale` (default **12**). They are clamped to a finite range `[0,100]` at the gameplay call site; set zero to suppress physics reactions. These are separate from ragdoll impulse cvars.
- The existing explicit `SetInteractivePartFree("left_door", true/false)` and `WorldPhysic->GetPhysicsPartWorldPose(...)` APIs remain available. The `.phys` part is authoritative; no extra scripted collision proxy should be created.
- I.6 supplies a driven velocity-servo, I.7 breakable hinges/detached parts, I.8 visual pose networking and persistence, I.9 final editor/integration hardening.

### Milestone I.5 regression checklist

1. Compare a `prop_drive animated` car door with a `driven` version: both register hits, but only the latter releases and physically swings.
2. Shoot near the hinge and near the far edge: impulses should be applied at their actual contact points, producing different torque.
3. Shoot a door behind a wall and one behind the nearer chassis: neither should be hit through the blocker.
4. Shoot a primitive-only door, including outside the car's original root hull; it should be detected as a real part hit.
5. Fire shotgun pellets: each pellet may apply **one** separate impulse, not two from the legacy/supplemental traces.
6. Explode near two doors: one total budget is split by falloff, including when `takedamage` is disabled. Check water/visibility gating.
7. Toggle bodygroups, remove the prop, reload the model, change levels and test the NullPhysics backend.
8. After I.8, test multiplayer visual bone positions and saved games; those are not validated in I.5.

**Validation boundary:** Portable hit-policy and legacy format tests can run independently in this environment; actual PhysX joint simulation, runtime raycast results and in-game door visuals are **not** verified without SDK/runtime dependencies.

---

## Milestone I.4 — animation / free-hinge ownership handoff

I.4 adds a **server-side explicit mode switch** on top of the I.3 `PxRevoluteJoint`.
The `.phys` file format remains **version 4**; existing v1–v4 assets continue to load.
No Xash3D-FWGS changes are needed.

### Preparing a car door

1. Author a rigid Studio bone `door_l` on `car.mdl`, with a hinge pivot and axis aligned with the door's actual rotation.
2. Make `body "left_door"` reference `bone "door_l"`, set positive `mass`, and give the body a stable `part "left_door"` name.
3. Give the door a convex or primitive collision shape. Multiple convex parts may share one bone/part; do not use an animated triangle mesh for a door that will become dynamic.
4. Connect the `left_door` body to a `chassis` body using `joint kind hinge`, `hinge_anchor`, `hinge_axis`, `hinge_min` and `hinge_max`.
5. Use model scale `(1,1,1)`. Ensure the chassis entity has its normal PrimeXT PhysX root actor.
6. Test closed, intermediate and open Studio animation frames with the collision overlay before switching the part to Free.

### Server-side mod programmer API (implemented in I.4)

```cpp
// On a CBaseAnimating instance after its PhysX actors and hinge exist:
if( pCar->SetInteractivePartFree( "left_door", true ))
{
    // Studio animation no longer drives this existing hinged actor.
    // Its most recent world pose and measured velocity were transferred.
}

// Return ownership ONLY when the door is almost exactly aligned and settled:
const bool backToAnimation = pCar->SetInteractivePartFree( "left_door", false );
// false here is expected if it is still moving or far from its Studio target.
```

The underlying interface is `WorldPhysic->SetPhysicsPartMode(entity, name,
IPhysicLayer::PhysicsPartMode::Free)` or `Animated`, with `GetPhysicsPartWorldPose`
for a server-side current part transform. Invalid names, absent hinges and unsafe
(non-convex/non-primitive) dynamic geometry return failure instead of creating
another rigid actor or forcing invalid PhysX shapes into dynamics.

The handoff is explicit in I.4, but I.5 adds optional bullet/explosion-triggered
release for `driven`/`free` authored hinge parts. Motor-assisted recovery is planned
for I.6, detachment for I.7 and client bone-pose synchronization for I.8. The server physical collider and server trace geometry can follow the
free physical actor; **clients are not guaranteed to render the resulting bone
pose until the I.8 presentation/network layer is implemented.**

### Testing checklist

- Shoot/sweep the door at closed/open animation poses; test after switching to Free, too.
- Verify the door has one actor with all of its convex shapes (no overlapping duplicate proxy actors).
- Switch during a nonzero animation velocity; confirm it inherits linear/angular motion.
- Request Animated while the hinge is far from the target: request must be rejected, not teleported.
- Confirm bodygroup swaps and gameplay collision-off state remain respected.
- Remove the car, replace its model and change levels with an active hinged body; there must be no dangling hinge/actor.
- Recheck visual bones on multiplayer clients once I.8 is implemented.

**Validation note:** portable tests validate velocity handoff math and legacy asset parsing.
The uploaded ZIP does not include the recursive PhysX/vcpkg SDK, so real PhysX
compile/link, collision behavior, hinge dynamics and client rendering still need
an in-engine test on a dependency-complete checkout.

---

## Milestone I.3 — Live PhysX hinge constraints (NOT yet bullet-reactive)

I.3 builds **real PxRevoluteJoint constraints** on the I.2 one-actor-per-bone
part registry. It adds two optional `.phys` version 4 joint keys:

- `hinge_anchor x y z`: pivot expressed in the **child Studio bone's
  reference-pose local coordinates** (default `0 0 0`).
- `hinge_axis x y z`: nonzero local rotation axis in the same bone frame
  (default `1 0 0`). For a vertical vehicle door hinge, this is commonly
  `0 0 1` **if the bone's local Z axis is vertical**. Verify in the editor.

The solver constrains motion between `hinge_min` and `hinge_max` **degrees**.
The frames are calculated from the Studio *bind/reference* hierarchy, not from
whatever animation frame was playing when the entity was instantiated. A door
already open when the map loads therefore does not redefine the hinge origin.

### Authoring example: car door

```text
physics_asset
{
    version 4

    # Chassis: use the normal PrimeXT entity/root collider.
    body "chassis"
    {
        bone "car_root"
        shape box
        half_extents 40 20 15
        mass 400
    }

    # Single part actor. Collision-only mesh sections can also be added;
    # if supplied for this hinged child they must be CONVEX, not triangle_mesh.
    body "left_door"
    {
        bone "door_l"
        part "left_door"
        shape box
        half_extents 16 2 14
        mass 18
        prop_drive driven
        drive_strength 120
        drive_damping 15
    }

    joint "left_hinge"
    {
        parent "chassis"
        child "left_door"
        kind hinge
        hinge_anchor 0 0 0
        hinge_axis 0 0 1
        hinge_min -5
        hinge_max 75
        break_impulse 900
    }
}
```

**Note:** `.phys` uses token-style fields, not the `#` comments above as part
of the format. Remove those explanatory comment lines when writing the actual
file unless your tool confirms it accepts them. For the chassis use your
original `car.mdl` root collision (or an authored root collision section), not
a second physics-part actor on `car_root`.

A hinge child with bone-bound convex `collision` sections keeps **all those
shapes on one actor**. When no collision section exists for the child, I.3 can
create its authored primitive `body` shape on that same actor. It does not
spawn a duplicate door physics body. Root/chassis actor creation still
requires the entity's normal PrimeXT PhysX setup. If that actor is missing,
the hinge cannot be created; check engine logs.

### Limits and requirements

- **Both parent and child must be named authored bodies**. The root parent
  body may have no `part` or `prop_drive`: it resolves through the root Studio
  bone and existing entity PhysX actor. A non-root parent must have an actor.
- Hinged child collision geometry must be **convex or primitive**. Triangle
  meshes are not eligible for eventual free/dynamic mode. Separate bodygroup
  variants remain supported; final runtime inertia/variant transitions require
  I.4 QA.
- Use entity scale `1 1 1`; scaled articulated joints are intentionally
  refused until shape dimensions, offsets and inertia can be scaled together.
- `break_impulse` is recorded but **does not break joints yet** (I.7).
- `prop_drive driven`/`free` still does **not** switch the prop's owner or
  activate motor control. The child is kinematic and follows Studio animation
  in I.3; switching ownership and inheriting velocity is I.4.
- Bullets/explosions don't yet impart physical hinge torque (I.5), motors/
  recovery are I.6, damage/detachment I.7, networking I.8, editor workflow
  and full integration I.9. Thus a valid hinge asset **does not yet imply**
  a bullet will swing the door in an I.3 build.
- `.phys` versions 1–3 and existing v4 hinge files (without new keys) still
  load; omitted anchors/axes use safe defaults, which may require manually
  specifying the axis to match the actual Studio door orientation.

### Practical testing with a full PhysX-enabled build

Inspect the hinge pivot with `sv_ragdoll_debug 2` / PhysX visualization,
then open and close the door through a full animation cycle. Check joint
alignment for closed/part-open/open poses, chassis-to-door self-collision,
multiple convex shapes, model replacement, root collision toggles, bodygroup
changes, scale rejection, and map/world shutdown. Repeat with moving or rotated
car entities. A parser test alone does not verify PhysX constraint stability.

## Milestone I.2 — physics-part identity and collision ownership

**Current format stays `.phys` version 4.** No asset migration or Xash engine
modification is required. I.2 prepares an authoritative *part registry*; it does
**not** yet enable physical hinge motion, bullet-driven mode transitions, or
detachment. Those are later Milestone I stages.

### Who defines what

- **Model builder:** supply a stable Studio bone for each rigid moving item
  (`door_l`, `hood`, `turret_yaw`) and keep chassis triangles separate.
- **Physics asset builder:** use one or several bone-bound `collision` sections
  per moving bone. If that part will later be interactive, add a v4 `body`
  referring to that **same bone** and give it a unique `part` identifier.
- **Level builder:** place the normal `car.mdl` as before; no new entity key or
  runtime drive-mode override is provided by I.2.
- **Mod developer:** stage I.2 stores stable part identities and an explicit
  animated-proxy vs articulated-body ownership boundary in PrimeXT. Stage I.3+
  will implement the PhysX joints and state transitions that use it.

### Multiple shapes on one moving part

You may divide the `door_l` collision into multiple simplified sections:

```text
collision "left_door_outer"
{
    type convex
    bone "door_l"
    // vertex/triangle data authored in door_l bone-local coordinates
}
collision "left_door_inner"
{
    type convex
    bone "door_l"
    // another convex piece, same bone-local coordinate system
}
body "left_door"
{
    bone "door_l"
    shape box
    half_extents 16 2 14
    mass 18
    part "left_door"
    prop_drive driven
    drive_strength 120
    drive_damping 15
}
```

This is a **partial excerpt** from a `physics_asset { version 4 ... }`
file; the `collision` sections need real `vertex` and `triangle` lines to be
valid. To physically connect the door later, the asset also needs a chassis
body and joint (see the existing Milestone H example below).

**New runtime organization:** both `collision` sections belong to the same
bone and therefore to **one** kinematic PhysX actor, with two shapes attached.
Their individual bodygroup variant/disabled flags still work independently.
The v4 `body` provides the future physical identity/parameters; **I.2 does
not create a second collider from this body while animation owns the door**.
If there are no bone-bound `collision` sections, a metadata-only v4 body is
registered but it provides **no I.2 animated-door hit/collision surface**.

### Compatibility and gameplay expectations

- v3 bone-bound sections without an interactive `body` still work; their
  fallback part identity is the Studio bone name.
- Root (no `bone`) collision remains on the car/chassis actor and is not
  merged into a moving-door actor.
- Empty bodygroup variants can disable an individual shape, not the whole
  car. Do not include a copy of the closed door in the root mesh.
- The root collider must remain enabled when a `.phys` has only interactive
  metadata and no collision sections.
- A full ragdoll/physical-animation activation claims articulated ownership;
  its old animated collision proxy is suppressed instead of double-colliding.
- `prop_drive animated/driven/free` still expresses **authoring intent**, not
  an automatically enabled motor or simulated hinge in I.2.

**QA requested with a complete PhysX build:** test two convex sections on
one door, different bone name capitalization, a hidden/visible bodygroup
variant, animation at closed/half-open/open, model swapping, collision toggle,
ragdoll activation/deactivation, and a metadata-only .phys with a legacy root.
Source-level layout/parser tests cannot prove these runtime outcomes.

## Milestone I.1 — baseline audit and compatibility notes

This is a **correctness/stability update**, not the interactive hinge runtime.
The `.phys` **current file version remains 4** and all v1/v2/v3 assets
remain readable. `prop_drive animated/driven/free`, `kind hinge`, `part`,
`drive_strength`, `drive_damping` and `break_impulse` still describe **future
runtime behavior**; a bullet does not yet make an animated car door swing under
PhysX control.

### What model and asset builders should do

- Give every v4 `body` that needs a gameplay identity its own nonempty,
  **unique** `part` label (case-insensitive). For example, `left_door` and
  `right_door`, not two entries both named `left_door`. The validator now rejects
  duplicate labels so future hit routing cannot choose the wrong part.
- Keep root model-space collision and bone-local moving collision in separate
  `.phys` sections. In H and I.1, animated door **collision** is still provided
  by the v3 `collision { bone "door_l" ... }` path; v4 `body`/`joint` declarations
  alone are not a working bullet-reactive door.
- Save assets using the Physics Asset Editor or the updated serializer. It now
  retains enough decimal digits to recover exact 32-bit floating-point numbers
  for mass, joint settings, small offsets and collision vertices.
- Do not use empty numeric strings (`mass ""`, `version ""`, etc.). These
  previously could be interpreted as zero and are now rejected.

### Animated door map-testing checklist

1. Test **door closed, half-open and fully open**: the green collision overlay
   and bullet/player traces should follow `door_l` rather than the rest pose.
2. Test changing to a **disabled bodygroup variant**, then back to a visible
   variant. A hidden door must not leave an invisible root collision surface.
3. Test toggling collision off and back on through normal game code; all child
   shapes must respect the requested state, even when the root shape is empty.
4. Test moving a door across an area-node boundary. The spatial link should
   follow its changing world-space bounds; this was a correctness fix in I.1.
5. Test replacing a model while the old model has an intentionally empty root
   collision variant. The new model should inherit the gameplay-requested
   collision on/off state rather than the old root's incidental shape flags.
6. Verify material/decal/sound still comes from `car.mdl`, not `.phys`.

**Still needs a real PhysX-enabled build:** runtime checks involving kinematic
actors, model swaps, spatial relinking, save/restore, mixed render-bodygroups,
and collision-on/off sequencing cannot be certified from ZIP-only syntax tests.

## What changed in this update

Milestone F moved runtime `_cp.mdl` collision into `.phys`. Milestone G completes the important animated part of that migration: `.phys` collision can now be bound to Studio bones and follow the model's current animation.

A single animated model can therefore remain one map/entity model:

```text
models/vehicles/car.mdl
models/vehicles/car.phys
```

The car does **not** need a separate door entity merely to make an animated door collide.

Conceptually:

```text
car.mdl current animation
        |
        +-- car_root/chassis -> root .phys collision
        +-- door_l ----------> bone-bound .phys collision
        +-- door_r ----------> bone-bound .phys collision
        +-- hood ------------> bone-bound .phys collision
        +-- trunk -----------> bone-bound .phys collision
```

The visible `.mdl` remains responsible for rendering, animation, materials, decals and impact sounds. `.phys` owns simplified collision/query geometry.

## For model builders

### Animated car-door example

A suitable visible model skeleton is:

```text
car_root
|-- door_l
|-- door_r
|-- hood
|-- trunk
|-- wheel_fl
|-- wheel_fr
|-- wheel_rl
`-- wheel_rr
```

The visible left-door vertices should be rigidly assigned/weighted to `door_l` if the door is a rigid Studio part.

In `.phys` v3, chassis collision has no `bone`, while the door collision names the moving bone:

```text
physics_asset
{
    version 3

    collision "chassis"
    {
        type triangle_mesh
        vertex ...
        triangle ...
    }

    collision "left_door"
    {
        type convex
        bone "door_l"
        vertex ...
        triangle ...
    }
}
```

**Coordinate rule:** root collision vertices are model-local. `bone "door_l"` collision vertices are `door_l` bone-local.

At runtime, when the Studio animation swings `door_l`, PrimeXT updates both:

- engine/player/NPC/bullet trace collision to the current door pose;
- a kinematic PhysX child collider used for real physical contacts with other PhysX bodies.

Do not duplicate the closed door inside the chassis collision if `left_door` already owns it. Otherwise the player can collide with an invisible closed-door copy while the animated door is open.

### Recommended moving-part geometry

Use one simple rigid collision piece per moving bone where possible:

```text
door_l      -> one low-poly convex/mesh section
hood        -> one low-poly convex/mesh section
trunk       -> one low-poly convex/mesh section
turret_yaw  -> one rigid section
turret_pitch-> one rigid section
```

This system is **rigid-per-bone**, not general deformable/skinned collision. A collision triangle should not span two bones if it is expected to move as one rigid piece.

For animated parts, prefer low-poly convex geometry where practical. Imported triangle meshes are supported for kinematic moving pieces, but simpler shapes reduce cooking/contact work.

### Migrating an existing animated `_cp.mdl`

1. Open the normal render model in `pxmv`.
2. Open **Tools -> Physics Asset Editor...**.
3. Click **Import _cp**.
4. The editor finds the sibling proxy automatically (`car.mdl` -> `car_cp.mdl`).
5. Rigid proxy triangles whose three vertices use the same **non-root** bone are imported as v3 bone-bound collision when that bone name exists in `car.mdl`.
6. Proxy root-bone collision becomes normal chassis/root collision.
7. Mixed-bone triangles are baked into root/reference space because one rigid PhysX child cannot follow multiple bones.
8. Scrub/play the car animation and inspect the green collision wireframe. It should stay on the moving door/hood/trunk.
9. Save `car.phys`.
10. Test in game, then remove `_cp.mdl` from the runtime content copy.

For reliable animated migration, the proxy and visible model must use matching bone names and compatible bind/local bone coordinates. The usual safe workflow is to build the collision proxy from the same skeleton as the render model.

### Creating a new animated collision asset

The current pxmv collision workflow still uses a temporary collision Studio model as the easiest geometry authoring source:

1. duplicate/use the visible model skeleton;
2. create only low-poly collision surfaces;
3. rigidly assign each moving proxy piece to the corresponding bone (`door_l`, `hood`, etc.);
4. keep chassis/root proxy triangles on the root bone;
5. export as temporary `car_cp.mdl`;
6. load `car.mdl` in pxmv and use **Import _cp**;
7. inspect animation + green collision wireframe;
8. save `car.phys`;
9. ship `car.mdl` + `car.phys`, not `_cp.mdl`.

### Bodygroups plus moving bones

A collision section can be both bone-bound and bodygroup-selective:

```text
collision "left_door_present"
{
    type convex
    bone "door_l"
    bodygroup "left_door"
    variant 1
    vertex ...
    triangle ...
}
```

An empty/removed variant still uses:

```text
collision "left_door_removed"
{
    type triangle_mesh
    bodygroup "left_door"
    variant 0
    disabled 1
}
```

Test both directions: present -> removed and removed -> present, at multiple animation frames.

### Static/root collision

A collision section with no `bone` follows the whole entity exactly as in Milestone F. Static and kinematic root props can use low-poly concave triangle collision. Dynamic root props derive a convex hull from active root collision vertices.

### Scale

Use model/entity scale `1` for animated collision whenever possible. Uniform scale should be tested carefully. Avoid non-uniform scaling of rotated bone-bound collision pieces; the engine trace representation and PhysX mesh scaling can diverge for that case.

## Bullet hit area versus physical collide area

For an animated door both areas come from the same current-pose `.phys` definition:

```text
bullet/player/NPC query
       -> current-pose animated .phys trace mesh

PhysX rigid-body contact
       -> bone-bound kinematic child actor
```

So a door open at 70 degrees is hit/collided at 70 degrees, not at its closed reference position.

After `.phys` confirms a bullet hit, PrimeXT now traces the **visible model at its current Studio pose** to obtain material/decal/sound information. An open metal door therefore gets its effect information from the visible open door rather than sequence-0/frame-0.

## For character/model builders

Living characters continue using ordinary Studio hitboxes for normal bullet damage. Do not replace character gameplay hitboxes with prop-style collision sections merely because v3 can name a bone.

Ragdolls/physical animation continue to use `body` and `joint` sections:

```text
body "head"
{
    bone "Bip01 Head"
    shape sphere
    radius 6
    mass 5
    hitgroup 1
}
```

Standard hitgroups remain:

```text
0 generic
1 head
2 chest
3 stomach
4 left arm
5 right arm
6 left leg
7 right leg
```

A `.phys` v3 file may contain prop collision and articulated bodies/joints together when a model genuinely needs both.

## For map and level builders

No new map entity/key is required for bone-bound collision. Place the normal model/entity as before:

```text
model = models/vehicles/car.mdl
```

Do not place or precache `_cp.mdl`.

If the entity already participates in PrimeXT PhysX collision, the physics layer automatically loads `car.phys`, creates its normal root actor and creates kinematic child actors for enabled bone-bound sections.

If `car.phys` is absent, original PrimeXT fallback behavior remains.

After editing `.phys`, reload/restart the map because definitions and cooked meshes are cached for the map lifetime.

### Animated-prop test checklist

For a car door, test all of these states:

- door completely closed;
- door partially open (important — catches reference-pose bugs);
- door completely open;
- bullet from outside and inside;
- player hull walking into both sides of the door;
- NPC hull/path contact if applicable;
- a dynamic PhysX prop struck/pushed by the moving door;
- the door moving into a dynamic prop;
- chassis/door seam for unwanted self-collision or duplicate collision;
- every collision-relevant bodygroup variant;
- map save/load if the entity is save-restorable;
- map reload after asset edits.

Use a mid-animation frame as a required test. Testing only fully closed/open states can hide an incorrectly root-bound proxy.

## For mod builders / programmers

### Runtime lookup

The runtime convention remains:

```text
models/foo.mdl -> models/foo.phys
```

`_cp.mdl` is authoring/import-only and must not be added to runtime precache/resource logic.

### Format versions

```text
v1 -> articulated/ragdoll only
v2 -> root/model-space prop collision + articulated data
v3 -> v2 + rigid bone-bound animated collision
```

The parser continues reading v1 and v2. For new interactive joint fields, use `.phys` version 4; previously authored v1–v3 files remain loadable.

### Collision representation

The engine-facing policy remains:

```text
Living character bullet damage -> Studio hitboxes
Prop with .phys collision       -> PhysicsAsset
Active ragdoll                  -> PhysicsAsset articulated bodies
No applicable .phys data        -> Legacy PrimeXT
```

There is no separate `CollisionProxy` representation anymore.

### v3 runtime implementation

Root collision remains on the entity's normal PhysX actor. Each animated Studio bone uses one kinematic child actor holding its enabled collision sections with the same entity `userData` and physics-asset owner ID. Same-owner physics-asset shapes are filtered from self-collision.

Animation updates drive each child using the current Studio bone transform/kinematic target. A current-pose entity trace mesh combines active root and bone-bound sections for GoldSrc/Xash movement and bullet queries.

Bodygroup changes update both root geometry and enabled bone-child shapes. Explicit gameplay collision-disable state is preserved.

### Materials/effects

`.phys` remains query authority; `.mdl` remains visual/material authority. For v3 animated props, effect lookup uses current Studio bone transforms so the material trace matches the animated render pose.

### Xash3D-FWGS

No Xash source modification is required. Do not reintroduce the private server-model loader used by the earlier `_cp.mdl` experiment.

### Compatibility contract

Do not make `.phys` globally mandatory:

```text
no .phys collision -> original PrimeXT collision
```

That fallback is required for old mods/maps.

## Recommended migration order

1. Update runtime/tools to Milestone G.
2. Pick one animated model with a simple rigid moving part, preferably a door.
3. Import its `_cp.mdl` into `.phys` v3.
4. Rename/delete `_cp.mdl` in the runtime test copy.
5. Verify the green wireframe follows the animation in pxmv.
6. Test closed, half-open and open collision in game.
7. Verify bullet material/decal/sound at half-open pose.
8. Test physical contact with a dynamic prop.
9. Convert additional hood/trunk/turret/machinery assets.
10. Leave old models without `.phys` untouched until improved collision is needed.

## Current limitations builders should know

- Bone-bound collision is rigid-per-bone; arbitrary weighted/skinned deformable collision is not implemented.
- Collision mesh data is embedded as text vertices/triangles in `.phys`; keep it low-poly.
- Dynamic root props currently derive one convex hull from active root collision data; compound-convex authoring/decomposition remains a future improvement.
- The **Import _cp** converter expects Studio v10 and sequence 0 in sequence group 0.
- Proxy and render skeletons need compatible bone names/local bind coordinates for automatic animated migration.
- Non-uniform scale on rotated bone-bound pieces is not a recommended authoring case.
- Physics asset definitions/cooked meshes are cached for the map lifetime; reload after editing.
- Full PhysX compile/link/runtime verification still requires the recursive PhysX/vcpkg dependencies absent from the supplied source ZIP.

## Builder checklist for Milestone G

Before shipping an animated collision asset, confirm:

- render `.mdl` loads/animates normally;
- sibling `.phys` parses without warnings;
- `.phys` is v3 when any collision section has `bone`;
- `_cp.mdl` is absent from the runtime test folder;
- root/chassis collision does not duplicate animated attachments;
- green collision wireframe follows each moving bone through the whole animation;
- half-open/mid-animation bullet collision is correct;
- player/NPC collision is correct at mid-animation;
- PhysX dynamic props collide with the moving piece;
- same-entity chassis/door shapes do not fight each other;
- collision bodygroups enable/disable correctly;
- decals/sounds/materials come from the current visible `.mdl` pose;
- old assets with no `.phys` still behave as before.

## Milestone H: interactive articulated prop *authoring structure* (`.phys` v4)

The version 4 metadata below reserves a safe, validated asset format for future
interactive doors, hoods, gates and turrets. **This release does not implement
runtime bullet-to-hinge transitions or breakable joints.** These new declarations
are parsed, validated and saved, but the existing v3 runtime still governs
collision and ragdoll simulation. Do not promise gameplay reactions yet.

For a single Studio `car.mdl`, rig `car_root` and `door_l` as distinct bones,
with the visible door vertices rigidly weighted to `door_l`. Keep the original
v3 `collision` mesh section bound to `door_l` for accurate animated bullet and
movement collision until the unified body-owned query path is implemented.

The proposed interactive body setup is:

```text
physics_asset
{
    version 4

    body "chassis"
    {
        bone "car_root"
        shape box
        half_extents 44 18 12
        mass 1000
    }

    body "left_door"
    {
        bone "door_l"
        shape box
        half_extents 16 2 14
        mass 18
        part "left_door"
        hitgroup 20
        prop_drive driven
        drive_strength 120
        drive_damping 15
    }

    joint "left_hinge"
    {
        parent "chassis"
        child "left_door"
        kind hinge
        hinge_min -5
        hinge_max 75
        break_impulse 900
    }
}
```

**Important**: This sample demonstrates new v4 metadata, not a complete
collision-ready vehicle asset. Existing v3 collision sections may be added for
current runtime querying. Do not duplicate physical contact shapes after the
planned body-owned query/response resolver is delivered.

New v4 fields: `part` identifies a hit part; `prop_drive` accepts `animated`,
`driven` or `free` (absence means legacy ragdoll behavior); driven bodies must
have positive `drive_strength`; optional `drive_damping` must be non-negative;
`kind hinge` permits `hinge_min`/`hinge_max` in degrees and optional non-negative
`break_impulse` (zero means no break threshold). These are stored definitions;
the runtime will need a separate implementation of impulse routing, physics
ownership transfer, hinge creation, bone override, networking and detachment.

Keep `.phys` v1/v2/v3 when you are not using v4 fields. Xash source remains
unchanged. Before delivering a later milestone that activates these definitions,
update this builder guide with actual map entities, commands and observed tests.

---

## Milestone I.9.3 — Remote prediction geometry and .phys trace-cache invalidation

**For model and mod builders:** This update changes no `.phys` version or collision
syntax. The client can now load a local `models/.../foo.phys` alongside `foo.mdl`
for multiplayer movement prediction; root sections use cached triangle tracing
and bone-bound sections use the most recently available renderer bone transforms,
including received physics-part pose overrides. Render-material lookup still comes
from the visible Studio model, not `.phys`.

**Package both files on EVERY client:** Keep `foo.mdl` and the corresponding
`foo.phys` together with identical content to the dedicated server. Arrange
normal mod resource distribution/installation before multiplayer testing: this
patch does not automatically download `.phys` files. A missing or mismatched
client `.phys` can still cause prediction corrections. Restart/reload the level
after replacing `.phys`; live hot-reload is not guaranteed.

**Bodygroups and animated doors:** Use the Studio render model's named bodygroup
variants and rigidly bound bone sections; avoid relying on reference-pose
collision for a moving door. A cached renderer pose may be unavailable before the
first render of an entity, or while an entity is out of view. In that case the
client deliberately avoids guessing a stale door position; the server remains
authoritative. Convex-cooked *dynamic* bodies may differ geometrically from their
authored triangle list; keep dynamic collision meshes simple and convex.

**Cache migration:** `.clip` disk cache format is now version **3**, with a
64-bit fingerprint of the entire `.phys` file in collision-derived cache entries.
Old `.clip` v2 data cannot be silently reused. The cached mesh identity includes
render model, bodygroup, skin, geometry type and asset fingerprint. The engine
rebuilds the cache on the next valid trace; manually deleting `cache/*.clip`
remains a troubleshooting option but should not be required merely after
editing `.phys` then restarting the map.

**Acceptance tests:** On a dedicated server with two clients, compare movement
through an open car door with the exact same `.phys` asset on server and remote
client. Repeat with the door half-open, a disabled bodygroup variant, a modified
`.phys` without touching `.mdl`, and a client missing `.phys`. Look for network
corrections and log messages; not all conditions are guaranteed identical due
to client pose freshness and convex cooking. Test a ragdoll/partial-ragdoll
transition separately, because those have additional query ownership issues.

**Verification boundary:** The C++ client/common files pass syntax checks and
portable cache/source tests pass. Actual client/server PhysX physics and remote
prediction have not been run in a live engine in this environment.


---
## Milestone I.9.4 — Physical-limb bullet ownership and rotated bounds (B5/B6)

**No `.phys` schema changes.** Assets remain v1–v4, and no model/map migration is needed.

### Builder rules for partial and full ragdolls

- Author a `.phys` body for every limb that should remain bullet-hittable after physical animation begins. While a character has an active partial/full ragdoll, weapon bullet traces reject its obsolete animated Studio hitboxes and the server's explicit `.phys` raycast selects the contact body and hitgroup. An unrepresented body region will not have an automatic animated-hitbox fallback for those weapon traces.
- **Movement is different from bullets:** partially simulated characters retain their GoldSrc movement hull; the bullet-only trace filter does not change generic movement collisions.
- Give each physics body a correct `hitgroup` and use the editor to check body sizes/positions against the reference pose. Test body weights from 0 through 1 and check displaced limbs beyond the original hull.
- Full ragdoll world-space collision/query bounds now follow the actual PhysX body bounds even if the entity is rotated. Render/client `mins/maxs` stay model-local; authoritative server `absmin/absmax` are computed from physics world-space bounds.

### In-game acceptance checklist (requires PhysX-enabled build)

1. Animate a standing character's arm, activate partial ragdoll, move its arm away from the original animation; shoot its former location (miss) and physical arm (hit correct `hitgroup`).
2. Verify the same partially ragdolled character still blocks the player through its usual movement hull.
3. Place a wall behind the character: bullets missing the arm should strike that wall; bodies behind a closer wall must never take damage.
4. Use two overlapping partial ragdolls: hit ordering must follow nearest physical geometry, not either animated hitbox.
5. Rotate a stretched-out full ragdoll by 90 degrees, shoot a protruding limb, check correct world-space `absmin/absmax`, PVS and culling.
6. Test bullets immediately after activation and after sleeping/thawing; check the current body bounds are used after physics stepping.
7. Verify material/sound/decal behavior remains inherited from the visible Studio model and no extra bullet impulses are generated.

**Engine:** This fix uses Xash's existing `ShouldCollide` and entity `SetAbsBox` callbacks. The upstream Xash source does not need modification. The complete PhysX runtime remains untested in this provided environment because the SDK/vcpkg toolchain is absent.

---

## Milestone I.9.5 — B7/B8/B9 physics consistency (scale, ragdoll saves, exit blend)

**Compatibility:** `.phys` remains v1–v4; no model/level asset conversion, new entity keys, or Xash patch is required. Previous bodygroup, hitgroup, and interactive-part settings remain as documented above.

### B7: scale and animated collision pivot

A bone-bound `.phys` section now scales its *hinge/bone pivot position* about the entity's origin in **entity-local axes**, just as the collision vertices are scaled. For example, an unscaled bone 20 units along model X with a uniform model scale of 2 has a world-pivot offset of 40 model units after scaling. This addresses the previous mismatch between the animated collision contact geometry and the scaled server trace mesh.

Model builders should keep bone-bound collision shapes rigid to one bone, place pivots at the actual joints in reference pose, and test a rotated entity at a non-unit positive scale to ensure geometry and pivot coincide. **Interactive dynamic hinged props and fully simulated ragdolls still require unit scale** in the existing runtime paths; this fix does *not* promise that non-unit-scale dynamic hinge/ragdoll joints work.

### B8: full/partial ragdoll save and restore

An active `.phys` ragdoll now writes a **whole-articulation snapshot** to the normal `CBaseAnimating` save stream: model identity, ordered body names, poses and velocities, per-body physics weights, sleeping flags, frozen state and pre-ragdoll movement/hull information. Full-ragdoll world-pose relation is preserved for visualization, and body world positions are offset by the entity savegame landmark translation on restore. A maximum of **64 physics bodies** per animated entity can be saved. If the asset is absent/changed, too large, or the snapshot is invalid, the restore path explicitly returns to the pre-ragdoll normal entity/hull instead of leaving a SOLID_NOT / MOVETYPE_NONE physics-less ghost.

For mod authors: the saved model name and **ordered physics-body names** must match the current `.mdl`/`.phys`; keep these stable across savegame-compatible mod releases. Old savegames without these new I.9.5 fields are not guaranteed to restore a pre-I.9.5 ragdoll. The separate I.8 interactive car-door save path remains unchanged. Detached doors still belong to their original entity; this does not add autonomous door entities.

### B9: animation/ragdoll exit blend

Clients now skip the Studio bone cache while `m_bRagdollExitBlend` is active, not only during active ragdoll frames. This allows the 0.18-second exit blend to advance even if the return animation is paused or has only a single frame. Model builders can test the exit on static reference poses and on looping animations.

### Required in-game acceptance tests (PhysX build)

1. Animate a rigid, bone-bound door at scales 1 and 2 while the parent car is rotated. Compare PhysX debug wireframes, player contacts, and precise bullet hits against the rendered door. Verify moving dynamic parts at scale 1 separately.
2. Spawn an active full ragdoll, force a non-reference pose, shoot a limb, save, reload and compare every body pose, velocities, collision ownership, bullet hitgroup, sleeping and freeze/thaw behavior.
3. Save a partial ragdoll with mixed weights (0, 0.35, 1.0), reload and confirm movement hull ownership, dynamic/kinematic status and physical-animation controls.
4. Change the saved `.phys` body names or exceed 64 bodies, then load: verify the fallback entity is visible and has ordinary movement/collision rather than an invisible corpse. Repeat without the PhysX backend.
5. Exit ragdoll into a paused one-frame idle animation. Verify blending finishes, then bone caching resumes and does not freeze the first blended frame.
6. Run save/load and rendering tests with both a local client and a dedicated remote client; watch for old snapshots and ragdoll collision being duplicated.

**Validation boundary:** This milestone has portable numeric/header regression tests, existing regression-suite replay, and client/non-PhysX server syntax checks. The PhysX SDK and vcpkg toolchain are not available in the provided project ZIP, so the actual PhysX translation units, savegame engine IO, joint solver and live networking still need full-build/runtime acceptance testing.


## Current J.10.6.10.4 ragdoll authoring and rendering

Use v5 .phys assets and regenerate obsolete files from the matching MDL in updated pxmv. F4 supplies actual Barney Human Standard/Free Rotation review rigs, preserves passive Studio bones, repairs render scale and palette weights, and maintains physical-part/root ownership. Deploy matching client and game_dir/glsl/skinning.h together. Positive uniform articulated scale is supported; nonuniform/mirrored articulated construction is unsupported. Review inferred flexion axes and selective contact pairs before Apply/Save. Native PhysX drops, Windows/Qt linking, GPU rendering, game networking/save/query/ballistics and FPS still require the J.10.6.10.4 acceptance checklist. Historical milestone sections above retain their original version-specific limits.


## Current J.10.6.10.5 F5 presets and creatures

Use Heavy/Loose variants for the checked human graph, Quadruped for four explicitly checked limb chains and Creature for an editable core with bounded limb/tail/wing seeds. Unknown aliases/branches need explicit mapping and review. The generated v5 asset contains concrete bodies/joints/pairs, so the game does not need the authoring template. Inspect joint guides, fitting/mass/contact warnings and passive Studio bones before Save. Only Barney has a supplied matching model here; synthetic quad/creature fixtures are test data. Native and additional actual-model acceptance remains pending.
