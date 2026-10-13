# F2 fields and conventions

This phase extends F1's current version-4 authoring/runtime asset. It does not add migration tooling or redesign obsolete formats. The authorized breaking replacement schema and stability/filter settings are still F3 work. Build the matching F2 binaries: earlier parsers reject the new frame keys. A `.phys` is self-contained and does not load `.ragtemplate` at runtime.

## Runtime D6 frames

```
frame_anchor 0 0 0
frame_angles 0 90 0
```

Both are in the **child Studio reference bone's local space**. Anchors use MDL/engine distance units. Angles are Euler pitch/yaw/roll degrees, `Rz(yaw) * Ry(pitch) * Rx(roll)`, matching body `angles`. Local X is D6 twist/flexion; Y/Z are swing. The child reference bone transforms this frame into bind world space. Parent/child body-local frames are then inverse reference-body pose times this same world frame, including body offset/orientation. Uniform runtime scale multiplies frame positions once; `.phys` authoring resize also scales anchors once and never changes angles. Nonuniform, negative or mirrored runtime scale remains unsupported.

Frames must be finite. Nonzero D6 frames require version 4 and D6 kind. Prop hinges retain `hinge_anchor`, `hinge_axis` and separate hinge limits/damage/motor behavior. Converting to Hinge clears D6 frames. Angular motions remain `locked`, `limited`, `free`; all three joint translations stay locked in preview and runtime.

## Template additions (template_version 1)

| Scope | Field | Meaning |
| --- | --- | --- |
| Template | `radius_ratio` | Maximum capsule radius / segment length, 0.01–0.45; default 0.22. |
| Template | `end_trim` | Fraction trimmed at each end, 0–0.3; default 0.06. |
| Template | `mass_ratio_warning` | Warn above this neighbor ratio, 1–1000; default 10. |
| Template | `mass_region "id" weight` | Repeated positive regional budget weight, unique IDs. |
| Template | `exclude_section "gun"` | Repeated case-insensitive alphabetic bodypart token. Empty list includes all sections. Matches `gun`, not `gunner`; separators/digits bound tokens. |
| Role | `region "left_arm"` | Required valid regional ID when regions are declared; empty when none are declared. |
| Role | `frame_mode bone/segment/flexion/explicit` | Identity bone axes, inferred longitudinal twist, inferred flexion X, or supplied Euler frame. |
| Role | `frame_anchor x y z` | Child-reference-bone-local pivot; default origin. |
| Role | `frame_angles pitch yaw roll` | Used by Explicit; serialized authoring values otherwise retained. |
| Role | `flexion_sign 1/-1` | Reverses inferred flexion axis; not used by Explicit. |

Regional mass = total budget × active region weight / sum of active region weights. Body mass = regional mass × included role weight / sum of included role weights in that region. Missing regions are excluded before normalization. Without regions, role weights divide the entire budget as in F1. This budget applies to generated bodies; preserved/merged authored bodies retain their own masses. Reports evaluate the final composed asset.

Human initial regional targets are torso 44%, head/neck 8%, each arm/shoulder/hand 8%, each leg/foot 16%. These are experimental authoring defaults, not certified human biomechanics or solver tuning. Ordinary Half-Life hitgroups remain head 1, chest 2, stomach/pelvis 3, left/right arms 4/5 and left/right legs 6/7.

Flexion uses the bind bend plane when meaningful. Straight chains use the semantic torso/lateral plane; the sign is ambiguous until inspected. Review is required even where inference looks plausible. Explicit frames permit unfamiliar axes and mirrored local conventions. No literal Barney model-name or numeric bone-ID branch exists in production generation.

Overlap checks use sphere/capsule distances, segment-to-oriented-box distance and oriented-box separating axes. Reported positive depths are geometry estimates in model distance units, not PhysX contact/penetration measurements. Reports currently flag estimates above 0.1 units; this threshold is a review aid. F2 does not change the inherited same-owner contact suppression. Fit and review colliders before F3 enables selective contacts.
