# J.10.6.10.1 — R01 phase F1

F1 replaces one-click generation with reusable templates and a reviewable
candidate workflow. It builds on J.10.6.10 F0 diagnostics. It does not certify
Barney's anatomical rig quality or repair the reported twisting, spikes, head
spin or detached-looking torso; those remain F2–F4 acceptance work.

## Author a candidate in pxmv

1. Build the updated PrimeXT sources, load the model, then open Physics Asset
   Editor → **Auto Generate**. Unapplied body/joint fields remain untouched.
2. Choose **Human Standard**, **Free Rotation**, or **Custom** and a body detail.
   Built-ins are immutable. **Duplicate** creates an editable copy; **Import**
   loads an editable `.ragtemplate`. Give custom copies a useful ID/name.
3. Review each role's parent, exact aliases, required flag, shape, mass weight,
   hitgroup, angular modes and limits. Aliases are separated by semicolons.
   **Pick override** selects an exact loaded-model bone. **Apply role edits**
   validates the draft; adding/removing roles also validates the hierarchy.
4. Choose **Replace ragdoll** or **Merge**. Replace keeps collision sections,
   interactive bodies, hinge endpoints and their ancestor closure. Merge keeps
   all authored bodies/joints and adds only absent ones. Neither mode retunes
   reused bodies or child-owned joints.
5. Click **Generate Preview**. The overlay shows a separate candidate. The
   report lists resolved/missing/excluded roles, confidence, body differences,
   joints and their angular modes. Ambiguous aliases or incompatible ancestry
   require an override or draft correction. Preserved bone bindings must also
   exist in the model.
6. **Simulate Preview** uses the candidate when pxmv has PhysX enabled. F0
   diagnostics can record it; metadata marks `generated_preview 1`. A build
   without PhysX still permits candidate review and applying authoring data.
7. **Apply to Asset** asks for confirmation of the concrete candidate and
   replaces editor data; unapplied main-panel text is replaced. Changed
   template/detail/mode settings require another reviewed preview. An asset
   edited since preview cannot be overwritten by stale generation.
8. Continue ordinary body/joint editing, simulate, then **Save** the `.phys`.
   **Undo Gen** restores the asset before the last applied generation, provided
   later applied edits would not be discarded. Save/reload/model changes have
   the normal editor lifecycle; loading another asset resets generation undo.

**Cancel** and the generator's title-bar close stop candidate simulation and
restore authoring view without changing the asset, dirty flag or pending main
panel fields. Save As for a template is independent of saving the `.phys`.
It suggests the model folder, confirms existing-file replacement, and writes
via a temporary file. Reusable templates can be stored in any selected folder.

## Presets and limits of this phase

- **Human Standard** uses logical human roles with Half-Life `Bip01` and
  ValveBiped exact aliases. Its initial 70-unit budget is normalized across
  included roles; damping starts at 0.05 and generic angular limits at ±45°.
  These are initial F1 settings, not HL2-equivalent anatomical tuning.
- **Free Rotation** uses the same role mapping and frees all three angular
  axes of newly generated D6 joints. Relative joint translations stay locked
  in runtime and preview. Reused authored joints retain their existing modes.
- **Custom** starts with one required root. Duplicate it to build another role
  graph; automatic matching for unfamiliar anatomy is not promised.

Initial fitting uses bone-local vertex bounds, with a reported bone-vector
fallback for bones without vertices. Coordinate-frame editing, asymmetrical
anatomical constraints, overlap repair, ratio diagnostics, resistance,
solver/projection/sleep tuning and selective self-collision are later phases.
Unsupported template keys fail explicitly rather than being ignored.

Templates are authoring inputs only. Saved `.phys` assets contain concrete
body names/bones, shapes, masses, joint endpoints and angular modes. No custom
template file is needed on the game server or client.

## Included files and checks

The [template schema](TEMPLATE_FORMAT.md), [interim `.phys` notes](PHYS_EXTENSION.md),
[Barney audit](BARNEY_AUDIT.md), [defect register](DEFECT_REGISTER.md),
[validation report](VALIDATION.md), [completion notes](COMPLETION.md) and [native acceptance](ACCEPTANCE.md) define
the completed work and outstanding evidence.

`documentation/examples/ragdoll_templates_f1/` contains all three preset files
and resolved Standard/Free Rotation Barney candidates generated from the
supplied model fixture. They are review samples, not certified replacement
game assets. The original uploaded files are retained unchanged.

From the PrimeXT source root:

```sh
python3 game_shared/tests/verify_j10_6_10_1.py --ubsan
python3 game_shared/tests/verify_j10_6_10_1.py --ubsan --inherited
python3 game_shared/tests/verify_j10_6_10_1.py --ubsan --inherited --barney-model /path/to/barney.mdl --json f1.json
```

Native Windows/Qt/PhysX builds and a real pxmv review/edit/save/reload run remain
pending. Available checks exercise production core and extracted editor/toolkit
methods through explicitly labeled service adapters.

The completion check also feeds the matching binary model to production vertex
bounds and ordinary body/joint editing, Save, Reload and Undo methods. Its
23 checks pass without modifying the supplied inputs. Numeric panel readers
now own the borrowed toolkit label before parsing. These checks use GUI service
adapters; they do not observe native controls, rendering or the PhysX solver.
