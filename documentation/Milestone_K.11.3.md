# PrimeXT / Xash milestone K.11.3

K.11.3 starts from the attached K.11.2 PrimeXT snapshot and retains the engine
fixes from K.11.0. It reorganizes the physics authoring tools, repairs candidate
save handoff, freezes the entire skeleton during pxmv simulation and corrects
the weapon editor's first-person projection. The .phys v5 and .ragtemplate
formats stay compatible with K.11.2.

## Physics editor pages and scrolling

The main editor uses Bodies, Joints, Interactive parts, Asset / preview and
Help pages. Template generation uses Template settings, Bone mapping,
Constraints / chains, Review + apply and Help. The page selector and primary
actions keep the workflow clear without displaying every field at once.
Advanced fields reveals joint angles, frames, damping, chain tuning and
break thresholds. Hidden controls retain their saved values and validation.
Numeric fields use the shortest round-trip float representation.

The normal text size is 100%; 125% and 150% remain available. Windows control
geometry now scales by both the selected text size and monitor DPI. Scrolling
batches child moves without copying stale pixels, clears exposed dialog areas
and redraws every child. A failed movement batch falls back to individual
moves. Qt retains its scroll-area adapter and the same page visibility rules.

## Edit templates and save assets

1. Load the matching render model and open Physics Asset Editor.
2. Click Templates. To edit a preset, choose it and click Edit copy. To edit
   a saved recipe, use Template settings > Import .ragtemplate.
3. Edit settings or select a role on Bone mapping. Apply role edits commits
   that role's fields. Advanced fields on Constraints / chains exposes angles,
   frame placement and chain controls. Save template as writes the recipe.
4. Generate preview and inspect Review + apply. Required review checkboxes
   remain visible in basic mode. Recipe changes invalidate the old candidate.
5. Apply to editor transfers the generated candidate without writing a file.
   Apply + save .phys transfers and writes it. If writing fails, the transferred
   asset remains available in the editor for correction and saving.

Save .phys with an outstanding generated candidate now routes through the
same review/apply flow. It cannot interpret an unapplied preview as an empty
authored asset and remove the sidecar. Cancel preserves the original asset;
Undo generation restores the previous asset after an applied generation.

To edit an existing .phys, load its matching MDL; its sibling asset loads
automatically. Asset / preview > Open existing .phys can select another v5
file and checks its body bones against the model before replacing the editor
asset. Edits save to the selected path. A .ragtemplate is a generation recipe;
a .phys is a complete generated model asset, not an editable template recipe.

## Simulation and weapon camera

pxmv captures all bone transforms in model space when simulation starts.
Physical bodies drive their bones; unboxed descendants retain their captured
local pose while following the physical hierarchy. Branches without a
physical ancestor also stop animating. Stop/Reset releases the capture.
Normal animation playback pauses during simulation; final pose reconstruction
also protects against autoplay/procedural changes. This is a tool preview
change, not a change to game ragdoll ownership.

The weapon editor uses pxmv's default weapon-origin camera for First Person:
+X forward, -Y right, +Z up, offset (1, 0, -1), near plane 4 and vertical FOV
68 degrees. It no longer centers or fits the whole viewmodel to its bounding
box. Near-plane clipping preserves crossing triangles and UVs, perspective
interpolation corrects textures and raster limits preserve viewport aspect.
Reset view keeps the current preset. Orbit and directional presets keep their
existing projection. The preview does not reproduce game bob or custom view
offsets, lighting or procedural animation.

## Apply this cumulative patch

The single patch is `PrimeXT_Xash_milestoneK.11.3_combined.patch`. Extract the
supplied original PrimeXT-master.zip and xash3d-fwgs-master.zip side by side,
then run from their parent directory:

```sh
git apply --check /path/to/PrimeXT_Xash_milestoneK.11.3_combined.patch
git apply /path/to/PrimeXT_Xash_milestoneK.11.3_combined.patch
```

This patch includes the retained K.11 merge, K.11.0 maintenance repairs and
K.11.2 Windows min/max diagnostics compatibility. Rebuild engine, PrimeXT
client/server and pxmv/pxwe for matching architecture. Applying it over an
already patched K.11.x tree is not the intended route.

## Verification and limits

Production-method tests with explicit platform adapters passed page geometry
and visibility in both modes, generation/import/review/apply/cancel/undo,
actual .phys saving/reopening and captured skeleton reconstruction. Strict
UBSan weapon preview tests passed authored eye projection, bounding-box
independence, behind-eye clipping, near-plane crossing, aspect ratio and
unchanged model bytes. Recording Win32 callbacks passed DPI geometry,
repeated scrolling, background erase and movement-batch fallback. Portable
context help and text-size tests passed. The full template-dialog translation
unit passed Linux C++17 syntax checking.

The inherited K.11.0 cache/subview/CCD suite passed 56/56 groups with
ASan/UBSan or a recording PhysX adapter; the K.11.2 Windows macro test passed.
The cumulative patch is checked against fresh original ZIP extractions and
the applied Git tree must exactly match the final source tree, including
file modes.

Native Windows/Qt GUI painting, a complete pxmv link, the real PhysX solver,
GPU/gameplay and the exact Barney/AK models shown in the screenshots were
not available for this run. Synthetic bones and meshes test the repaired
logic; native builds and those actual assets still need visual/drop checks.
The complete editor compile is blocked by missing GL development headers.
