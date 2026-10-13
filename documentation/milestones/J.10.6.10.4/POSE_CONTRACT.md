# Physical Studio transform contract

Articulated physics supports finite **positive uniform entity scale**. Reflected pxmv render passes are visual copies of a positive-scale simulation. Arbitrary nonuniform scale, shear and mirrored articulated model construction remain unsupported.

| Value | Space and scale |
|---|---|
| Animated world bone | World position; entity scale in all three basis columns |
| Physics actor/body pose | World position; unit rotation |
| Authored body offset | Bone-local position/rotation; positional offset scaled once during runtime construction |
| Simulated bone | actor_world * inverse(body_local); world position and unit rotation |
| Snapshot root/relative positions | World-sized translations; do not multiply by entity scale again |
| Final render bone | Physics world position; unit rotation basis multiplied by render scale |
| Passive relative link | unit(animated_parent)^-1 * unit(animated_child), retaining world-sized displacement |
| Compact GPU palette | Unit proper quaternion, world position and signed orthogonal basis scale |

matrix3x4::Invert supports uniform scale. The inherited preview defect came from composing model-sized local translations with a unit-basis physical parent, and from leaving the passive master root at its old animation transform. The fix uses unit world bases for both relative operands and restores scale only on the result's basis.

Validate all parents and matrices before committing. Compute parent order without assuming bone indices are ordered. Seed driven bones, reconstruct passive ancestors above the designated root, then rebuild passive descendants and side branches. A driven parent owns its own world pose. A part override uses root -1 and leaves its animated ancestors/siblings alone. Preserve current animated/procedural local relations; stationary physical targets do not forcibly freeze passive animation.

The compact palette removes basis scale before quaternion extraction. A negative determinant is represented by flipping the third unit column and carrying a negative Z scale. Translation is unchanged. Its shader reconstructs the same orthogonal matrix as the complete palette. Every bone of a supported articulated model shares the same uniform scale; the first final world bone supplies the uniform. Nonuniform shear cannot be reconstructed by this encoding.

CPU skinning compensates byte rounding on the first valid influence, as GLSL does for valid imported palettes. Malformed CPU indices are ignored safely; that guard is not a promise that malformed GPU vertex buffers are accepted. Weighted authoring evaluates the same rule in reference pose with imported inverse-bind matrices, independently of animation/tool overrides. The existing final bone * inverse-bind pipeline remains in place.

Both drawing and collision prediction consume the same reconstructed pose. Adding a physical bone to an existing compatible snapshot captures its current displayed passive transform in current root space. Removing it returns to current passive animation-relative reconstruction. Entry/exit clocks, sequence ordering and cache ownership retain the inherited protocol; no network or savegame format change is made.
