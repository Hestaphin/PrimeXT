# Supplied Barney audit — F1

Inputs were read without modification. This is a model/asset audit and portable
generation fixture, not a real pxmv, solver, skinning or live-game reproduction.

| Input | Bytes | SHA-256 |
| --- | ---: | --- |
| `barney.mdl` | 129104 | `d326ef89e83489db85e1cc1d3c554f1b7cbc9c43c4bc7a940d76b0d75e1a393f` |
| `barney(1).phys` | 11801 | `8040edc7a4b5de91279730f692a64df41167a4ee42cdcb18777b58f848777490` |

The model is IDST Studio v10, 24 bones, flags 0 (rigid vertex bone assignments).
The bone table starts at byte 244. Bounds include 472 vertex assignments across
all submodels in two bodyparts. `Bip01`, `Bip01 Spine1` and `Bip01 Neck` have no
assigned vertices. Numeric bone/bounds data is recorded in
`game_shared/tests/fixtures/j10_6_10_1/barney_source.json` and
`barney_skeleton.h`; the binary model is not embedded in the patch.

The completion check uses the actual SHA-checked MDL bytes with the production
vertex-bounds method, comparing all bone names/parents and bounds against this
fixture. It also loads a temporary copy of the original `.phys` through the
editor's production Reload method, then generates, edits body/joint fields,
saves and reloads concrete values. All 23 checks pass under UBSan. GUI services
and bind transforms are supplied by the harness; no native rendering or solver
run is claimed. Original input hashes remain unchanged.

The supplied `.phys` is version 4 with 23 bodies and 22 D6 joints. All body bone
names exist in this model. The authored mass sum is **70.130087559**. Head mass
is **5.75605631**, neck mass **0.200000003**, and Spine1 mass **0.200000003**;
head/neck is about **28.7803:1**. All 22 joints use generic 45° swing limits
and −45°/45° twist limits. These reproduce the handoff leads; they do not
establish the sole cause of the reported symptoms.

## Verified model hierarchy and F1 roles

| Bone index | Bone | Studio parent | Human role / minimum detail | Vertices |
| ---: | --- | --- | --- | ---: |
| 0 | Bip01 | none | — | 0 |
| 1 | Bip01 Pelvis | Bip01 | pelvis / Simple | 27 |
| 2 | Bip01 L Leg | Bip01 Pelvis | left_thigh / Simple | 7 |
| 3 | Bip01 L Leg1 | Bip01 L Leg | left_calf / Simple | 17 |
| 4 | Bip01 L Foot | Bip01 L Leg1 | left_foot / Standard | 15 |
| 5 | Bip01 R Leg | Bip01 Pelvis | right_thigh / Simple | 7 |
| 6 | Bip01 R Leg1 | Bip01 R Leg | right_calf / Simple | 17 |
| 7 | Bip01 R Foot | Bip01 R Leg1 | right_foot / Standard | 15 |
| 8 | Bip01 Spine | Bip01 Pelvis | spine / Detailed | 6 |
| 9 | Bip01 Spine1 | Bip01 Spine | spine_mid / Detailed | 0 |
| 10 | Bip01 Spine2 | Bip01 Spine1 | — | 3 |
| 11 | Bip01 Spine3 | Bip01 Spine2 | chest / Simple | 14 |
| 12 | Bip01 Neck | Bip01 Spine3 | — | 0 |
| 13 | Bip01 L Arm | Bip01 Neck | — | 20 |
| 14 | Bip01 L Arm1 | Bip01 L Arm | left_upper_arm / Simple | 18 |
| 15 | Bip01 L Arm2 | Bip01 L Arm1 | left_forearm / Simple | 14 |
| 16 | Bip01 L Hand | Bip01 L Arm2 | left_hand / Standard | 20 |
| 17 | Bip01 R Arm | Bip01 Neck | — | 20 |
| 18 | Bip01 R Arm1 | Bip01 R Arm | right_upper_arm / Simple | 17 |
| 19 | Bip01 R Arm2 | Bip01 R Arm1 | right_forearm / Simple | 24 |
| 20 | Bip01 R Hand | Bip01 R Arm2 | right_hand / Standard | 77 |
| 21 | Bip01 Head | Bip01 Neck | head / Simple | 92 |
| 22 | Bone05 | Bip01 Head | — | 22 |
| 23 | Bone03 | Bip01 Pelvis | — | 20 |

The logical roles use aliases, not bone-index special cases. The production
generator contains no Barney name branch. Studio arms pass through Neck →
L/R Arm → L/R Arm1 → L/R Arm2 → Hand. F1 maps upper arms to Arm1, forearms
to Arm2 and connects their physical joints directly to the mapped chest
(`Bip01 Spine3`), after confirming ancestry. The Studio hierarchy is unchanged.

For this fixture, Simple creates **11 bodies / 10 joints**, Standard
**15 / 14**, and Detailed **17 / 16**. Standard omits physical master, neck,
clavicle-like Arm, intermediate Spine/Spine1/Spine2, Bone03 and Bone05 roles.
Detailed includes Spine and Spine1; Spine1 uses a reported fallback shape.
Omission is a template selection decision, not a claim that accessory or
unsimulated-bone rendering is fixed.

## Published candidates

`documentation/examples/ragdoll_templates_f1/barney_f1_human_standard.phys`
and `barney_f1_free_rotation.phys` are generated from the supplied numeric
fixture with Standard detail and a 70-unit normalized role budget. They are
fully resolved, reload through the actual parser, and support concrete body
edits and serialization. Template files are not runtime dependencies.

Collider fitting still uses initial bone-local AABBs and generic joint axes.
No overlap, anatomical range, drop-orientation, settling, mesh spike, parity or
FPS acceptance has been performed. Review copies in the matching model before
using them as game assets. The original `.phys` remains a comparison fixture.

Only one matching model was supplied. No second anatomical rig is fabricated;
the ValveBiped/generic mapping tests use explicitly labeled synthetic skeletons.
