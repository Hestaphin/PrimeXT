# Physics assets (.phys) — current v5

## K.11.3 editor workflow

Physics Asset Editor has **Bodies**, **Joints**, **Interactive parts**,
**Asset / preview** and **Help** pages. The generator has **Template settings**,
**Bone mapping**, **Constraints / chains**, **Review + apply** and **Help**.
Choose a section at the top. **Advanced fields** reveals angular limits,
constraint frames and tuning fields; hiding them preserves their values.
The default text scale is Normal 100%, with monitor DPI scaling and optional
125% or 150% text sizes.

To edit an existing recipe, open **Templates...**, choose **Template settings**
and **Import .ragtemplate**. Edit its settings and bone mapping; use
**Apply role edits** for the selected role. **Save template as...** saves the
recipe. To edit a built-in preset, select it and click **Edit copy** first.

Click **Generate preview**, inspect **Review + apply**, acknowledge any required
axis/chain or mass/overlap reviews, then **Apply + save .phys**. This transfers
the generated bodies and joints to the asset editor and writes the loaded
model's sidecar. **Apply to editor** transfers without saving. Changing the
recipe after generation requires another Generate. Ordinary **Save .phys**
also routes an outstanding candidate through review and apply.

Loading a model loads its sibling `.phys` for editing. To select another
existing v5 asset, use **Asset / preview > Open existing .phys...** with the
matching model loaded, edit its body/joint fields, Apply and Save. The file
must use bones present in the model. A `.ragtemplate` stores a reusable
generation recipe; a `.phys` stores the generated model asset.

K.11.4 adds a **Simulation mode** selector on **Asset / preview** and the
generator's **Review + apply** page. **Full ragdoll: freeze unboxed bones** is
the default: it captures the complete skeleton and holds unboxed hands/feet
relative to their physical parents. **Partial ragdoll: animate unboxed bones**
keeps animation running on those bones; bodies defined in .phys remain driven
by PhysX. Switch during simulation without restarting the scene. Stop or Reset
releases the capture. The choice is shared for this pxmv session and does not
modify .phys/.ragtemplate data or in-game behavior.

See [K.11.3 changes and validation](../../Milestone_K.11.3.md).
See [K.11.4 preview modes](../../Milestone_K.11.4.md).

PrimeXT J.10.6.10.3 uses **version 5 only**. Versions 1–4 fail with an instruction to regenerate from the matching MDL in pxmv. Rebuild compatible server/client/pxmv binaries and author a new asset; changing a version number on an old file is not regeneration. Xash has no incremental F3 changes.

A Studio model may have one sibling `.phys`: the model supplies rendering, animation, hitboxes and visual material/effects; the asset supplies optional root/bone-bound collision and/or articulated bodies. No applicable asset retains the original fallback behavior. `_cp.mdl` is an authoring/import source, not a runtime model.

Read the [v5 schema and units](../../milestones/J.10.6.10.3/SCHEMA_V5.md), [authoring/regeneration guide](../../milestones/J.10.6.10.3/AUTHORING.md), [test evidence](../../milestones/J.10.6.10.3/VERIFICATION.md) and [native/game checklist](../../milestones/J.10.6.10.3/ACCEPTANCE.md). Older milestone guides describe historical behavior and do not grant old-format compatibility in this release.

## Collision geometry

```text
physics_asset
{
    version 5
    collision "chair"
    {
        type triangle_mesh
        vertex -12 -12 0
        vertex 12 -12 0
        vertex 0 12 24
        triangle 0 1 2
    }
}
```

Collision-only assets are valid. Types are `triangle_mesh` or `convex`, with triangle-list indices. Omitted `bone` means model/entity-local reference geometry; `bone "door_l"` means rigid bone-local geometry following that visible Studio bone. This is rigid-per-bone collision, not multi-bone skin deformation. Static/kinematic root props retain selected triangles; dynamic root props cook selected vertices as convex geometry.

Named `bodygroup` sections require a nonnegative `variant`. Unnamed sections are always eligible. An intentionally empty variant has `disabled 1` and no vertices/indices. Bodygroup switching rebuilds the eligible root/part geometry. Avoid duplicating a moving door in both chassis and door collision.

Root/bone-bound geometry and articulated definitions may coexist. Keep a single collision owner per logical part. Same-owner root/animated collision proxies remain mutually excluded; F3 pair policies apply to articulated body instances. Different entities/world contacts, conveyor response and trigger/contact notification semantics remain available.

The asset answers collision queries; the rendered model answers surface material, sound and decal lookup at its current pose. Living characters retain ordinary Studio damage hitboxes. Active ragdolls use explicit body queries for exact localized bullet hits; their PhysX shapes remain scene-query disabled. Optional mass/hitgroup/part metadata does not turn every animated prop into a character.

## Articulated bodies and hinges

Supported primitives are box, sphere and capsule. Each body binds one unique Studio bone, and joints form one rooted acyclic tree. Bodies carry reference-bone-local offset/orientation, positive mass and hitgroup. D6 frames use child reference-bone-local anchor XYZ and pitch/yaw/roll; all translations remain locked, including Free Rotation.

Profiles expose damping, solver passes, speed/depenetration clamps, mass-normalized sleep energy, CCD and contact/rest margins. Per-body omissions inherit the asset profile. D6 angular resistance is a velocity-only twist/swing drive with zero stiffness and a torque cap; projection defaults Off and is editable per joint. Keep props' existing drive/motor controls separate from D6 resistance.

Interactive hinges retain child-local `hinge_anchor`/`hinge_axis`, angular limits, part identity, animated/driven/free modes, motor strength/damping/force limits and impact/damage release/break controls. See the v5 `documentation/examples/interactive_car_door.phys` syntax example and the historical builder guide for gameplay details; a matching model and native acceptance are still needed. Native gameplay has not been verified in this environment.

Animation-owned articulated actors follow the current animation pose with simulation ownership disabled. Ragdoll activation transfers pose/velocities and enables bodies; partial ragdoll/physical animation, sparse network posing, query/decal ownership and save/load remain existing systems. F3 restores authored CCD on dynamic handoff because kinematic flags clear it in PhysX. Native/game acceptance remains pending.

## Selective contacts

`self_collision off` keeps pair rows but disables them. Selective/Custom consume explicit symmetric body-name pair rows; unlisted pairs are denied. Runtime filtering uses immutable per-instance indexed tables and stable shape tokens, not a mutable entity registry. Rebuild/destroy outside simulation, releasing joints/actors/shapes before token storage.

Human generation chooses Selective: it excludes graph neighbors within two edges and shapes that overlap after contact-margin inflation, then applies semantic role rules. Explicit allows with poor clearance produce warnings. The saved table is the same policy used by runtime and preview; inspect/simulate it for the actual model.

## Physics Asset Editor

Open Tools → Physics Asset Editor for the loaded model's sibling asset. Auto Generate opens a template workflow: choose/duplicate/import a preset, map roles, review fitting/masses/frames, Generate Preview, inspect warnings/axes, simulate, then Apply/Save. Cancel preserves the original, and Undo Gen restores the prior asset. Replace/Merge preserve unrelated authored collision and interactive closure under the documented policy.

Physics settings edits profile/body/joint overrides and body pair rows transactionally. Use profile clears an override; unchanged partial inheritance survives edits. Apply validates and blocks stale snapshots. Pending main-panel fields are preserved. Rename/delete repairs pair references. Custom templates expose Physics profile and semantic pair rules; editing invalidates a prior preview and requires regeneration.

Import _cp reads matching Studio v10 proxy geometry. Rigid same-bone triangles become bone-bound sections; root/mixed triangles are baked to reference/model space; bodygroup/empty variants are preserved. Sequence 0 must be in sequence group 0, as in the existing importer. Inspect the wireframe through animation before saving v5. This imports a model source and does not migrate an obsolete `.phys`.

## Scale, diagnostics and verification

Use positive uniform articulated scale. Dimensional settings scale consistently with geometry; sleep/resistance/torque use scale². Nonuniform or mirrored articulated models are unsupported. Keep bone-bound rigid geometry low-poly and avoid excessive actor splits.

F0 measurements remain optional: server `sv_ragdoll_diagnostics 1`, client `cl_ragdoll_diagnostics 1`, or editor Diagnostics/Save F0 log. Value 2 gives verbose capture. They default Off. Anchor/speed/sleep/scene-contact/pose metrics identify physical drift separately from rendering errors. F3 adds a standalone real-SDK drop capture harness for contact/landing/sleep/wake comparisons.

J.10.6.10.4 adds shared physical Studio hierarchy reconstruction, preview scale/reflection and CPU/compact GPU palette repairs. The matching Barney MDL is now tested through production extraction and generates v5 review rigs in documentation/examples/ragdoll_templates_f4. Deploy updated client and GLSL together. Native SDK/Qt/Windows linking, driver shader rendering, physical drops, live game and FPS remain unrun; follow documentation/milestones/J.10.6.10.4/ACCEPTANCE.md. Portable tests use explicit SDK/GUI/engine adapters.


## J.10.6.10.5 creature and preset authoring

Auto Generate now offers seven presets, including Human Heavy/Loose, Quadruped and Creature. Duplicate/import to edit chain edges and explicit branch consent. Discovery follows only declared seeds, respects explicit/detail-excluded mappings, reports truncation and requires expanded-chain review. All results remain self-contained v5 assets. Select a main-panel joint to see parent/child anchors, frame axes and independent angular guides; the guides do not certify coupled cone limits or physical stability. New actual Barney variants and labeled synthetic nonhuman fixtures are in documentation/examples/ragdoll_templates_f5. Follow documentation/milestones/J.10.6.10.5/ACCEPTANCE.md for additional actual models and native drops/game verification.
