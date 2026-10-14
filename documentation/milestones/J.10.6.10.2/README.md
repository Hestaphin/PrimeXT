# J.10.6.10.2 — F2 human rig authoring

This implementation starts from the exact refreshed J.10.6.10.1 upload: all 2,698 source-file hashes and executable modes matched its manifest. F2 implements steps 8–11: anatomical body selection, torso-supported shoulders, bind-pose fitting, regional mass budgets, hitgroups, oriented frames and asymmetric elbow/knee limits.

The implementation and available automated checks are delivered. The F2 physical exit criterion is **pending**: no native PhysX drop, live Barney render or game run was available. The current attachments contain recorded Barney bounds/skeleton and F1 sample assets, but not the matching binary MDL, its bind rotations, filtered mesh samples or recordings. The supplied Barney F2 files are bounds-fixture review candidates. Regenerate them from the matching MDL before in-game use.

## Changes

- Human Simple/Standard/Detailed produces 11/15/18 bodies on the recorded Barney skeleton. Standard retains two shoulder links and feet; Detailed adds hands and a lower spine. Empty optional neck/spine intermediates are merged; Studio bones are retained. Optional missing shoulders fall back to chest support.
- Capsule X follows the actual child segment. The radius is capped by length, the transverse fit resists outliers, and both ends are trimmed. Omitted semantic torso vertices are pooled into the selected torso ancestor. Bone-name accessories are omitted by role mapping; configurable bodypart tokens exclude weapon/accessory mesh sections.
- Rigid and weighted vertices are collected in the reference pose. A paused animation and tool overrides cannot change fitting. Weighted samples use imported inverse-bind matrices and the strongest influence for ownership.
- Regional mass targets survive missing optional bodies. Neighbor mass ratios and primitive overlap estimates are reported without silently retuning protected/merged authored bodies.
- Head/spine/shoulder/hip/hand/foot profiles use role-specific limits. Elbows and knees use local X flexion with 0–140 and 0–130 degree travel; Y/Z angular motions are locked. Inferred flexion planes/signs require review before Apply. Imported/edited explicit frames avoid inference.
- `frame_anchor` and `frame_angles` are resolved in the runtime asset, edited in both template and main joint panels, serialized and reloaded. A shared child-frame adapter is used by preview and runtime. Joint translations remain locked for Free Rotation.
- Corrected the inherited runtime D6 setter's undefined `jointDef` reference to its actual loop variable `def`. Focused tests compile and execute the production setter statements, but do not certify a native SDK build.
- Collision geometry and interactive hinge/part closure retain F1 Replace/Merge protections; hinge fields remain distinct from D6 frames.

## Use

Build updated PrimeXT server/client/pxmv, then load the matching MDL in pxmv. Auto Generate → Human Standard → Standard detail → Generate Preview. Inspect the body graph, mass-ratio/overlap report and inferred axis entries. Duplicate the preset to change exclusions, fitting, mappings, mass weights/regions, frame mode, explicit anchor/angles or flexion sign. For a wrongly directed elbow/knee, reverse its flexion sign or choose Explicit and author its frame. Generate Preview again after edits.

Review positive flexion in the preview, then check the axis-review box and Apply. The main joint panel can further edit child-bone-local anchor XYZ and pitch/yaw/roll; the body panel edits mass, shape and hitgroup. Save writes `.phys`. Cancel and Undo preserve the reviewed transactional workflow. A simulation/drop and independent mesh check are still required before accepting a rig.

See FORMAT.md for units/profile syntax, VALIDATION.md for exact evidence, ACCEPTANCE.md for remaining checks and DEFECT_REGISTER.md for symptom status. F3 solver/resistance/projection/sleep/selective-contact work and F4 Studio reconstruction repairs are not part of F2.
