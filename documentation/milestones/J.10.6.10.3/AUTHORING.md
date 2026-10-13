# Authoring and regeneration

Build the updated server/client and pxmv together. Keep an archive of obsolete `.phys` for manual reference, then load the matching Studio MDL in pxmv and author a new v5 asset. A v1–v4 file is deliberately rejected; do not relabel it or use recorded-bounds examples as a generated replacement.

## Generate a rig

1. Open **Tools → Physics Asset Editor → Auto Generate**. Choose Human Standard, Free Rotation or Custom and an appropriate detail level. Duplicate a built-in to edit its profile, mappings and semantic pairs.
2. Confirm anatomical mapping, accessory exclusions, rigid/weighted bind fitting, torso-supported shoulder graph, mass regions, hitgroups and child-local joint frames. Review inferred elbow/knee signs and axes.
3. Open **Physics profile** on a custom template. Set damping/solver/limits/sleep/margins/resistance/projection and role overrides. Choose self-contact policy and pairs by role. Apply commits the template draft and invalidates any old generation preview.
4. **Generate Preview** resolves body names, overrides and the final collision table. Review the graph, shape overlap, contact-clearance and inferred-axis warnings. Optional roles may be skipped; check their report. **Simulate** and compare Off with Selective, then review the visible mesh independently.
5. Confirm inferred-axis review when required, then **Apply**. **Cancel** leaves the original untouched; **Undo Gen** restores the complete previous asset. **Save** writes the sibling v5 `.phys`.

Replace Ragdoll preserves authored collision and protected interactive closures. Their effective tuning and allowed pairs remain stable even when the generated profile changes. Merge keeps the authored profile and pair rows between reused bodies; newly added generated bodies/joints carry explicit resolved tuning. Full replacement requires the existing explicit author choice. Regenerate to change generated defaults; template edits alone do not alter a saved runtime asset.

## Edit an existing v5 asset

**Physics settings** opens a draft for the asset profile, a body override or a joint override. **Use profile settings** clears that target's overrides. Partial overrides retain unrelated inheritance when unchanged. Explicit zero resistance/damping and projection Off are meaningful. Hinge joints expose projection while their D6 resistance controls remain disabled.

Select two body names and **Allow pair**, **Deny pair** or **Default**. Default removes an explicit row; an unlisted runtime pair is denied. The list is the saved matrix representation. Off keeps configured rows while disabling them. Review clearance before accepting an allow. Pair rows follow body rename and are removed on body deletion; template rules follow role rename/delete.

**Apply settings** validates and commits; **Cancel** discards. Main-panel body/joint text remains available; apply those fields normally before saving. A changed asset/model or changed template makes an old settings snapshot stale and blocks Apply; close/reopen for a fresh snapshot. Save/Reload serialize every profile and override. Settings are edited while simulation is stopped and rebuilt on the next preview.

For props, import matching `_cp.mdl` geometry through **Import _cp**, then inspect root/bone/bodygroup geometry and add/edit v5 interactive bodies/hinges. This importer uses a model authoring source; it is not an old `.phys` converter. Keep one collider owner per logical piece. `.phys` supplies collision while `.mdl` remains visual material/decal authority. Living-character hits and active ragdoll body queries retain their existing ownership rules.

## Practical tuning

Fix graph/pivots/overlap first. Increase damping or resistance only after checking those causes. High solver counts increase cost. Very small margins can miss contacts; large margins can activate touching neighbors earlier. Projection can hide bad frames and inject corrections. Sleep should follow actual settling; the native harness does not force a time-based freeze. Check that a localized impulse wakes a sleeping body and that CCD remains restored after animation/physics and save/load transitions.

Use positive uniform scale and record it. Do not accept a model solely because its debug bodies look plausible: compare the rendered unsimulated bones, pause the simulation and inspect for independent mesh motion. Those F4 checks remain pending for Barney.
