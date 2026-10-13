# Runtime schema v5

Only `physics_asset { version 5 ... }` is supported. Exactly one version declaration is required; generated files write it first. Old files fail with an actionable message to regenerate from the MDL using pxmv. Changing an old file's version number is not regeneration. The template format remains `template_version 1`, extended with F3 keys; it is distinct from the runtime asset version.

```text
physics_asset
{
    version 5
    simulation
    {
        linear_damping 0.1
        angular_damping 0.5
        solver_position_iterations 16
        solver_velocity_iterations 4
        max_linear_velocity 2000
        max_angular_velocity 20
        max_depenetration_velocity 200
        sleep_threshold 0.5
        contact_offset 0.787401557
        rest_offset 0
        ccd 1
        angular_resistance 1.5
        drive_max_torque 1000
        projection off
        projection_linear_tolerance 0.25
        projection_angular_tolerance 10
    }
    self_collision custom
    collision_pair "root" "head" deny
    body "root" { bone "root" shape box half_extents 4 4 4 mass 12 }
    body "head" { bone "head" shape sphere radius 3 mass 5 angular_damping 0.8 }
    joint "neck"
    {
        parent "root" child "head" kind d6
        frame_anchor 0 0 0 frame_angles 0 0 0
        twist_motion limited swing1_motion limited swing2_motion limited
        twist_low -30 twist_high 30 swing1 25 swing2 25
        angular_resistance 2
    }
}
```

This is a syntax example. It requires a matching model and authored fitting; it is not a second model validation result.

## Profile, units and limits

Omitted simulation keys use these schema defaults. Human Standard chooses damping 0.1/0.5 and resistance 1.5; Free Rotation chooses 0.1/0.05 and resistance 0. Imported/custom tuning remains editable.

| Key | Schema default | Accepted resolved value / units | Scope |
|---|---:|---|---|
| `linear_damping`, `angular_damping` | 0.05 each | 0–100, inverse seconds | profile/body |
| `solver_position_iterations` | 16 | integer 1–255 | profile/body |
| `solver_velocity_iterations` | 4 | integer 0–255 | profile/body |
| `max_linear_velocity` | 2000 | 0.001–100000, engine units/s | profile/body |
| `max_angular_velocity` | 20 | 0.001–1000, rad/s | profile/body |
| `max_depenetration_velocity` | 200 | 0.001–100000, engine units/s | profile/body |
| `sleep_threshold` | 0.5 | 0–1000000, mass-normalized kinetic-energy units (units/s)² | profile/body |
| `contact_offset` | 0.02 × (1/0.0254) ≈ 0.7874 | 0.00001–1000, engine units | profile/body |
| `rest_offset` | 0 | 0–1000 and strictly below contact offset, engine units | profile/body |
| `ccd` | 1 | 0 or 1 | profile/body |
| `angular_resistance` | 0 | 0–1000000, torque/(rad/s) | profile/D6 joint |
| `drive_max_torque` | 1000 | 0–1000000000, engine torque; positive if resistance > 0 | profile/D6 joint |
| `projection` | off | off/on | profile/joint |
| `projection_linear_tolerance` | 0.25 | 0.00001–1000, engine units | profile/joint |
| `projection_angular_tolerance` | 10 | 0.01–179.999 degrees | profile/joint |

Resolution is explicit override → asset profile → schema default. Body/joint omissions inherit. Numeric `-1` is the internal inheritance sentinel and is omitted by serialization; use omission in authored text, including `ccd`. Explicit zero damping/resistance and `projection off` override positive/on defaults. Range checks apply after resolution, including rest/contact interactions. Settings parse transactionally; duplicate singleton keys, nonfinite values, unknown enums and invalid/reversed pairs fail. Serializer uses float `max_digits10` and the classic locale.

The shared physics tolerance length is 1/0.0254 and speed is 800, matching the runtime's inch-like engine units. Runtime and editor use a 0.01-second step; diagnostics retain frame/backlog information. Material remains the existing friction 0.5/0.5, restitution 0. These values do not establish arbitrary model stability.

Positive uniform runtime scale multiplies geometry, offsets, joint anchors, linear speeds, depenetration speeds, contact/rest offsets and linear projection tolerance by scale. Sleep threshold, angular resistance and maximum torque multiply by scale². Mass, damping, solver counts, angular speed/limits and angular projection tolerance remain unchanged. The permanent geometry-resize tool applies the same dimensional transforms and preserves inheritance; finite overflow fails transactionally. Validate afterward because a large resize can exceed schema ranges. Nonuniform or mirrored articulated scale is unsupported.

## Joints and contact policy

All D6 X/Y/Z translations are locked. Angular keys are `locked`, `limited` or `free`; even unused serialized limit values must be finite and valid. Twist low/high lie within −180..180 and low < high; swing angles are strictly 0..180. `frame_anchor` is child reference-bone-local XYZ; `frame_angles` is engine pitch/yaw/roll in degrees. Both runtime local frames derive from that one bind reference.

Resistance maps to zero-stiffness, zero-target-velocity TWIST and SWING drives with a torque cap and force-unit constraint flag. SLERP is unused so a locked secondary axis does not disable the selected drive. Projection defaults off; it is a positional correction aid, not a substitute for good pivots, masses or limits. Hinge joints keep `hinge_anchor`, `hinge_axis`, `hinge_min/max`, impact/damage thresholds and the existing gameplay motor. Explicit positive D6 resistance on a hinge is rejected; the profile's D6 resistance is not installed on its motor. `body.drive_strength`, `drive_damping` and `drive_max_force` remain separate prop motor controls.

`self_collision` is `off`, `selective` or `custom`. Off disables contacts without discarding pair rows. Selective and Custom both consume the saved symmetric allow/deny table; unlisted pairs default to deny. There is no runtime geometry inference. Each `collision_pair "bodyA" "bodyB" allow|deny` references two existing, distinct names, case-insensitively; duplicates and reverse duplicates fail. Self pairs are always denied. Maximum bodies: 256; maximum distinct pair rows: 32640.

Template profile keys are flat in the template root. Role body/joint keys override them. `collision_neighbor_depth` defaults to 2 and `collision_overlap_margin` to 0.1. `pair_rule "roleA" "roleB" allow|deny` uses semantic role IDs. Generation resolves the final composed graph and bind shapes, pads each shape by its resolved contact offset plus half the extra margin, denies neighbors within depth and overlapping pairs, then applies explicit rules. Missing optional endpoints are reported and skipped. An explicit allow with padded overlap produces `CONTACT CLEARANCE REVIEW`; simulation and author review remain necessary.

## Existing v5 content and lifecycle

Collision sections retain `type triangle_mesh|convex`, bone-local/model-local vertices, bodygroup/variant selection and explicit disabled variants. Bodies retain primitives, offsets, mass, hitgroups, prop drive and part identity. One body binds each Studio bone; joints form one rooted acyclic tree. Collision-only assets are valid. New-format regeneration preserves unrelated authored collision and interactive closure unless full replacement is explicitly chosen.

Filter word 0 contains conveyor bit 0, asset-body bit 1 and policy bit 2; word 1 contains the engine owner; word 2 is body index + 1; word 3 is the policy cookie. Shapes point to stable tokens in an immutable per-instance group. Different entities and world contact keep default behavior; same-owner proxies stay excluded. Different live groups with a recycled owner remain separate. Actor `userData` keeps its edict semantics. Triggers/conveyor/contact notification flags are preserved. Ragdoll shapes keep scene queries disabled; existing manual body queries handle gameplay hits.

Settings change through authoring/rebuild, not an in-place mutation of a live table. Stop simulation before rebuilding/destroying; release joints and actors/shapes before their group. Runtime restore reconstructs instance identity and tuning from the active v5 definition, rather than persisting token pointers. Kinematic flags clear CCD in the SDK; dynamic handoff explicitly restores the resolved CCD value.

PhysX 4.1 primary API references: [Joints manual](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxguide/Manual/Joints.html), [Rigid-body flags](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/structPxRigidBodyFlag.html), [Shape margins](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/classPxShape.html), [Rigid dynamic settings](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/classPxRigidDynamic.html), [Filter callback](https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxapi/files/classPxSimulationFilterCallback.html).
