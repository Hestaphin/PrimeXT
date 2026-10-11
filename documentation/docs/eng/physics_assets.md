# Physics assets (.phys)

For interactive-part authoring in pxmv (drive modes, child-bone hinges, part damage and break thresholds), see `map_model_level_mod_builder_instructions.md` (Milestone I.9). A complete parsed `.phys` v4 schema example is at `documentation/examples/interactive_car_door.phys`; it requires a separate matching Studio MDL and has not been in-game PhysX-tested.


A Studio model can optionally have one sibling physics asset:

- `models/man.mdl` — rendering, animation, Studio hitboxes, materials and effect surfaces
- `models/man.phys` — optional prop collision geometry and/or articulated physics

Beginning with physics asset **version 2**, `_cp.mdl` is no longer a runtime asset. PrimeXT owns the extra collision data inside `.phys`, so Xash3D-FWGS does not need a private server-model loader or source modification for the collision-proxy system.

Physics asset **version 3** preserves the animated part of the old collision-proxy idea: a collision section may be rigidly bound to a Studio bone. Doors, hoods, trunks, turrets, levers and other rigid animated model pieces therefore carry their collision with their current animation pose.

Older assets remain compatible:

- v1: articulated/ragdoll data;
- v2: root/model-space prop collision plus articulated data;
- v3: v2 plus bone-bound animated prop collision.

Models with no applicable `.phys` collision retain original PrimeXT collision behavior.

## Root/model-space collision (v2+)

A v2 or v3 asset may contain collision geometry without ragdoll bodies:

```text
physics_asset
{
    version 2

    collision "chair"
    {
        type triangle_mesh
        vertex -12 -12 0
        vertex  12 -12 0
        vertex   0  12 24
        triangle 0 1 2
    }
}
```

A collision section with no `bone` key stores vertices in the render model's local/reference coordinate system and follows the whole entity. `type` accepts `triangle_mesh` and `convex`. Static and kinematic root props preserve selected `triangle_mesh` triangles. Dynamic root props cook the selected collision vertices into a PhysX convex hull.

A single `.phys` can contain root collision, bone-bound collision and ragdoll bodies/joints.

## Bone-bound animated collision (v3)

Version 3 adds an optional `bone` key:

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

        // These vertices are in door_l bone-local space.
        vertex ...
        triangle ...
    }
}
```

The rules are:

- no `bone` -> vertices are entity/model local and follow the whole model;
- `bone "name"` -> vertices are local to that Studio bone;
- the referenced bone must exist in the visible `.mdl`;
- bone-bound collision is rigid-per-bone, not arbitrary multi-bone skin deformation.

At runtime PrimeXT creates a kinematic PhysX child actor for each enabled bone-bound section and advances it from the current server Studio bone transform. The normal root prop actor remains responsible for root/chassis collision. Root and child shapes from the same entity are tagged with the same physics-asset owner so they do not collide with one another.

PrimeXT also rebuilds an entity-local trace mesh from the current Studio pose. Engine/player/NPC/bullet sweeps therefore see the same animated door/hood/turret position instead of a reference-pose proxy.

For a car door the intended flow is:

```text
car.mdl animation
      -> current door_l bone
      -> car.phys left_door collision
      -> current bullet/movement collision
      -> current kinematic PhysX collision
```

Keep root collision from duplicating moving attachments. For example, the chassis root collision should not also contain the closed door if a separate `bone "door_l"` section represents that door.

For moving parts, low-poly convex collision is recommended where practical. Kinematic triangle meshes are supported for migrated proxy geometry, but simpler rigid shapes reduce cooking/contact cost.

## Bodygroup collision variants

Root or bone-bound collision can also follow a named bodygroup from the visible `.mdl`:

```text
collision "magazine_present"
{
    type convex
    bone "magazine_bone"
    bodygroup "magazine"
    variant 1
    vertex ...
    triangle ...
}
```

Sections without `bodygroup` are always eligible. A named section is active only when the visible render model selects the matching `variant`.

An intentionally empty bodygroup variant is represented explicitly:

```text
collision "magazine_removed"
{
    type triangle_mesh
    bodygroup "magazine"
    variant 0
    disabled 1
}
```

`disabled 1` contributes no collision geometry. Runtime switching updates root collision and bone-bound child shapes without requiring another model.

## Collision/material ownership

For a prop with `.phys` collision:

```text
bullet / movement / physics query
        -> .phys collision geometry at current pose

confirmed visual impact
        -> visible .mdl at current Studio pose
        -> first visible surface intersection
        -> material / decal / impact sound
```

The `.phys` mesh does not need fake Studio textures. It answers where collision exists. The render `.mdl` remains the material/effect authority.

For animated v3 collision the visible-effect trace is built from current Studio bones rather than sequence-0/frame-0, so an open door resolves its impact against the open rendered door.

If a model has no usable `.phys` collision geometry, original PrimeXT collision generation is used.

## Migrating legacy `_cp.mdl`

`pxmv` -> **Tools -> Physics Asset Editor...** includes **Import _cp**.

With `models/car.mdl` loaded, the button looks for `models/car_cp.mdl` and converts its Studio v10 geometry into `.phys` collision sections:

- rigid triangles whose three vertices use the same non-root proxy bone, and whose bone name exists in the visible model, become v3 bone-bound collision;
- root-bone geometry becomes normal root/model-space collision;
- mixed-bone triangles cannot represent one rigid collision piece, so they are baked into reference/model space;
- multi-variant bodyparts become named bodygroup variants;
- empty variants become `disabled 1`;
- existing ragdoll bodies/joints in the target `.phys` are preserved.

For animated migration, `_cp.mdl` and the visible `.mdl` should use the same bone names and compatible bind/local bone coordinate system. A door proxy rigidly weighted to `door_l` is the expected authoring pattern. Do not build a collision triangle spanning `door_l` and `car_root` if the triangle is supposed to move rigidly with the door.

The Physics Asset Editor draws bone-bound imported collision through the currently displayed Studio pose. Scrub/play the animation and verify the green wireframe follows the moving part before saving.

After saving `car.phys`, `_cp.mdl` is no longer needed at runtime. Keep it only as source/archive material if desired.

The importer requires sequence 0 to live in sequence group 0, matching the previous proxy-baking assumption. Invalid bodygroup/vertex data is rejected rather than silently producing malformed physics data.

## Version 1 articulated syntax

Version 1 remains valid for ragdoll-only assets:

```text
physics_asset
{
    version 1

    body "pelvis"
    {
        bone "Bip01 Pelvis"
        shape capsule
        radius 6
        half_height 7
        offset 0 0 0
        angles 0 0 0
        mass 12
        linear_damping 0.05
        angular_damping 0.1
        hitgroup 3
    }

    body "chest"
    {
        bone "Bip01 Spine2"
        shape box
        half_extents 8 5 10
        mass 18
        hitgroup 2
    }

    joint "spine"
    {
        parent "pelvis"
        child "chest"
        swing1 25
        swing2 20
        twist_low -15
        twist_high 15
    }
}
```

Supported articulated shapes are `box`, `sphere`, and `capsule`. One rigid body may reference each skeleton bone. Articulated bodies must form one tree with exactly one root. v2/v3 assets follow the same articulated rules when they contain `body`/`joint` sections.

Physics assets are discovered lazily. Definitions/cooked geometry are cached for the map lifetime; expensive articulated actors/joints are instantiated only when ragdoll/physical-animation behavior requests them.

## Ragdoll and physical animation behavior

While animation owns a character skeleton, instantiated articulated bodies can follow animation without becoming an invisible second character collider. Full ragdoll activation snaps each body to the exact current animation pose, transfers animation-derived velocities, enables simulation, and hands presentation/contact ownership to `.phys` bodies.

Milestones B/C network sparse physics-bone poses to the client. Milestone D routes bullets to exact ragdoll bodies and localized impulses. Milestone E adds sleep/freeze, transition blending, partial ragdoll, physical animation, bone-space ragdoll decals and debug visualization. Milestone F unifies prop collision under `.phys`; Milestone G restores animated collision-proxy behavior through v3 bone-bound collision.

The prop collision section does **not** replace living-character Studio hitboxes for ordinary bullet damage. The centralized resolver selects hitboxes for living characters, PhysicsAsset collision for props, and articulated PhysicsAsset bodies for active ragdolls.

## Physics Asset Editor

The editor targets the sibling asset of the loaded model (`models/man.mdl` -> `models/man.phys`). Runtime and editor share `game_shared/physics_asset_format.*`.

Current authoring features include:

- create/load/reload/save `.phys`;
- legacy **Import _cp** migration to embedded v2/v3 collision sections;
- automatic rigid-per-bone migration of animated proxy pieces to v3;
- current-pose collision wireframe preview;
- collision bone/bodygroup validation;
- clear imported collision without deleting ragdoll authoring;
- add/remove/edit ragdoll `box`, `sphere`, and `capsule` bodies;
- bone binding, offsets/orientation, mass, damping and hitgroups for articulated bodies;
- parent-child D6 joints and swing/twist limits;
- geometry-aware Auto Generate for a first-pass ragdoll;
- live PhysX ragdoll preview when pxmv is built with `ENABLE_PHYSX`.

Auto Generate preserves imported collision sections instead of replacing the whole asset.

For new animated prop collision, the currently supported editor workflow is to author a low-poly temporary `_cp.mdl` with rigid Studio bone assignments and use **Import _cp**. The temporary proxy is an authoring source only and does not ship at runtime.

## Scale and authoring guidance

Bone-bound collision is intended for normal rigid Studio attachments. Use scale `1` where possible. Uniform entity scaling follows the same geometry scaling path; non-uniform scaling of a rotated bone-bound piece can be less exact in PhysX than the engine trace representation, so avoid non-uniform scale for animated collision assets unless specifically tested.

Keep bone-bound collision low-poly. One collision section creates one kinematic child actor; do not split one door into dozens of tiny sections unless needed for bodygroups.

## Compatibility rules

1. No `.phys`: original PrimeXT behavior.
2. v1 `.phys`: articulated/ragdoll behavior; no prop collision override.
3. v2 `.phys` `collision`: root/model-space prop collision/query geometry.
4. v3 `.phys` `collision` with `bone`: current-pose bone-bound collision/query geometry.
5. v2/v3 may also contain `body`/`joint` articulated definitions.
6. `_cp.mdl` is ignored by runtime; **Import _cp** is migration/authoring tooling only.
7. Xash3D-FWGS source modifications are not required.

## Interactive articulated props — v4 metadata (Milestone H)

`.phys` v4 adds validated **authoring metadata only**: `body.part`,
`body.prop_drive`, `body.drive_strength`, `body.drive_damping`,
`joint.kind hinge`, `joint.hinge_min`, `joint.hinge_max`, and
`joint.break_impulse`. Runtime animated→dynamic handoff, impulse routing,
breakage, and network updates are **not yet enabled**. See the Map / Model /
Level / Mod Builder instructions for a car-door example.


## Milestone I.1 baseline audit (runtime architecture unchanged)

The on-disk format is still `.phys` **v4**. Model-builder-facing fixes:

- Text serialization now writes floats using `max_digits10` precision so
  saving and loading preserves exact float values.
- Blank quoted numeric values are rejected rather than parsed as zero.
- Invalid programmatic enum values for collision type, body shape, prop mode
  and joint kind are rejected during validation.
- Nonempty v4 `part` names are unique case-insensitively within an asset.
- The runtime now maintains requested prop collision separately from the
  root shape's currently-selected bodygroup/geometry, and independently
  updates the PhysX simulation/query flags. Animated collision bounds are
  relinked when they change.

This is not implementation of v4 driven/free hinge behavior. Such bodies
are still metadata only until the later Milestone I runtime stages.

Standalone parser regression tests are at
`game_shared/tests/physics_asset_i1_tests.cpp`.
