# F2 validation evidence

The uploaded refreshed F1 cumulative patch was applied to the two original archives after their SHA-256 hashes matched F1 provenance. All 2,698 file contents and executable modes matched the uploaded F1 source manifest. A separate immutable F1 reconstruction passed 12/12 available check groups with UBSan and inherited checks; the optional real-model group was skipped because the binary MDL is absent.

## Available checks

| Check | Evidence / scope |
| --- | --- |
| F2 strict compilation and core tests | 71 checks: recorded Barney mapping/counts, regional mass conservation, no default neighbor ratios above 10, shoulder hierarchy, accessory omission, unmapped torso pooling, capsule segment alignment/outlier cap/trim, mirrored flexion signs, Free Rotation, explicit frames, scale, invalid data and analytic overlap. Actual production template/format code; UBSan and strict warnings. |
| Shared frame and actual D6 settings | 23 checks: child-local anchors scale once, orientation survives conversion, production runtime/editor setter statements agree, translations lock even with free angles, asymmetric limits use radians. Scoped mathematical/setter SDK facade, no solver. |
| Production Studio/authoring path | 22 checks: synthetic rigid/weighted Studio data, inverse-bind fitting, accessory token exclusions, animation independence, engine/portable rotation parity, inferred-axis gate, template and main-panel frame edits, invalid edits, Save/Reload/Undo/Cancel and cyclic parent rejection. GUI services adapted; no actual Barney or real GUI. |
| Existing F1 template/transaction checks | 90 core checks, 25 actual-dialog checks, three preset exports and two deterministic bounds-fixture candidates; F2 anatomy expectations updated explicitly. |
| Complete pxmv syntax/editor methods | 6/6 groups; 17/17 inherited editor-method cases. Five translation units include PhysicsAssetEditor, RagdollTemplateDialog, studio_render, studio_utils and GlWindow. GL declarations adapted; PhysX disabled. The full studio_utils check caught a missing cstring include, corrected before final validation. |
| Broader portable/source/method acceptance | 45/45 available groups in the inherited J.10.6.3 run, including the new F2 core; outer F0/F1 chains are also recorded. Three unchanged Xash complete-syntax checks remain dependency-blocked by the original missing library_suffix.h. |
| Patch routes | Eight routes compare every source byte and executable mode with SOURCE_MANIFEST.json. Detailed results in bundle evidence/patch_routes.json. |

Final focused and inherited summaries/logs are included in bundle `evidence/`. The dedicated focused refresh covers the final torso pooling, Free Rotation review behavior and template frame controls. Automated passing groups do not establish real SDK linking, real GUI operation, live model skinning, PhysX contact/settling, network/save/load or gameplay/FPS acceptance.

## Reproduce

From patched PrimeXT:

```
python3 game_shared/tests/verify_j10_6_10_2.py --ubsan --json f2.json
python3 game_shared/tests/verify_j10_6_10_2.py --ubsan --inherited --json f2_inherited.json
```

With the matching originally audited binary model, append `--barney-model /path/to/barney.mdl` to run the optional actual-model bounds/edit/save/reload group. That group is available for the native build machine and was not run here. It verifies original inputs by hash and works on temporary copies.

The native runner records BLOCKED: CMake is unavailable; no native configure/link command was attempted. The matching Barney binary, exact filtered samples, bind rotations and recordings are absent in this turn. ACCEPTANCE.md describes the still-pending physical/visual exit tests. F2 authoring implementation is delivered; its actual stable-drop exit remains pending.
