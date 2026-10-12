# Previous-phase audit

| Finding in the F3 baseline | F4 disposition and evidence |
|---|---|
| Preview replaces unit actor bones but leaves passive master root behind | Shared reverse-root/forward-branch reconstruction; baseline reproducer and actual Barney passive-bone tests |
| Preview computes scaled-animation inverse links and composes them under unit physical bases | Unit world-relative links plus restored basis scale; baseline at scale 2.5 has physical basis 1, descendant error 7.81188 and ancestor error 99.1879; repaired synthetic/actual-model tests |
| Compact client skinning extracts quaternions directly from scaled matrices | Unit quaternion plus signed scale uniform on mesh/weapon/decal paths; actual GLSL expression comparison and complete changed client translation units |
| CPU preview skinning sums quantized weights without GLSL shortfall compensation; assumes contiguous valid influences | Shared rounding behavior and safe valid-index iteration; four-influence, hole, invalid, empty and no-blend cases |
| Weighted bind fitting renormalizes every influence, differing from rendered byte weights | Compensate only a shortfall; preserve over-one sums. Synthetic weighted model exercises production fitting with imported inverse binds |
| Newly driven packet bone jumps directly to target while root is interpolated | Capture displayed passive bone on compatible receipt; production packet/prediction/draw test |
| Old source assertion expects inline ancestor loops | Assert scale-aware calls in the shared implementation; inherited regression chain rerun |
| F3 v5 schema/profile/filter/editor behavior | Retained and rerun through F3/F2/F1/F0 suites. No obsolete asset migration or speculative solver retuning |
| F3 bounds-only Barney examples | Keep as historical evidence; F4 adds actual MDL-generated v5 rigs and precise model/reference hashes |

The exact F3 source manifest matched before edits. Its actual Barney authoring baseline passed 23 checks using GUI adapters. The runtime body's inverse offset equation was already correct and is tested through the production CaptureSimulationPose method; no server pose math change was necessary. Test-only SDK declarations gained bounds validity and the getter's inflation argument.

These repairs isolate rendering/authoring defects. They do not prove the original head-spin symptom is resolved: a moving collider requires real solver/constraint/contact measurements. Native build, save/load/network engine execution and game acceptance remain open in ACCEPTANCE.md.
