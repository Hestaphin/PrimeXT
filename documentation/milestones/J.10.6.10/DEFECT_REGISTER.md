# R01 defect register at J.10.6.10

F0 adds evidence collection. None of the user-reported Barney symptoms is
declared repaired. Source hazards and synthetic regression cases are separated
from reproductions on the exact installed model.

| ID | Finding | Evidence | Status / next investigation |
| --- | --- | --- | --- |
| R01-01 | Anatomically excessive twisting | Reported in the supplied handoff; current generator assigns all joints generic ±45° twist / 45° swings without anatomical axes. | Open. Resolve roles, oriented local frames and asymmetric anatomy in F1/F2; measure actual rig first. |
| R01-02 | Sharp stretched mesh / triangle spikes | User-observed symptom in handoff, not reproduced here. Physical body and mesh causes remain separate. | Open. Compare anchors and actor speeds with driven/child/ancestor pose deltas and frozen-frame views; inspect GPU skinning if bone matrices are sound. |
| R01-03 | Head keeps spinning / corpse fails to settle | User-observed symptom; low generic body angular damping and no per-joint angular resistance are present in source. | Open. Record relative rotation, angular speeds, contacts and sleep counts. No damping or sleeping fix applied in F0. |
| R01-04 | Torso looks detached after death | Reported symptom, exact model/asset unavailable. It could be anchor separation or pose reconstruction. | Open. Distinguish world-anchor error from ancestor/skin-weight discontinuity. |
| R01-05 | Mass discontinuities from volume normalization and 0.2 floor | Source confirmed in AutoGenerate. The handoff reports head/neck ≈28.8:1 in its reference asset; those asset figures were not independently recomputed here. | Open. F2 needs anatomical mass budgeting and ratio validation. |
| R01-06 | Shoulder chains can run through a small neck body | Source uses nearest simulated Studio ancestor, with no logical chest/shoulder roles. The supplied handoff reports this in Barney. | Open. Verify actual model hierarchy before implementing human physics hierarchy. |
| R01-07 | pxmv does not reverse-reconstruct unsimulated ancestors above the physical root | Source confirmed: preview follows descendants; shared client CPU reconstruction also rebuilds root ancestors. A synthetic moving-pelvis test exposes ancestor discrepancy with stable driven transforms. | Confirmed reconstruction parity hazard, not proof of Barney's sole spike cause. Measured by F0; repair reserved for the diagnosed pose phase. |
| R01-08 | Limb fitting follows dominant local vertex-AABB axis | Source confirmed; selection uses largest local half-extent and max transverse radius. Actual limb/accessory geometry was not supplied. | Open. Compare intended bone direction, accessory weights and overlaps before F2 fitting changes. |
| R01-09 | Preview/runtime step and contact setup differ | Preview is 60 Hz with a four-step update cap and CONTACT_DEFAULT; runtime is 100 Hz and enables additional CCD/contact reporting. Both use 8/2 body solver counts and owner-wide self-contact suppression. | Confirmed configuration differences. F0 records the existing behavior; shared construction/contact policy belongs to F3. |
| R01-10 | Full coupled swing-cone violations are not quantified by F0 | Per-axis getter overshoot is a lower bound, explicitly labeled. | Diagnostic limitation. Use joint visualization now; add calibrated full-cone metrics if needed during F2/F3. |
| R01-11 | Matching-model reproduction and native acceptance missing | Available inputs contain source ZIPs/patches and the handoff markdown, not the matching model, reference `.phys`, videos or game/toolchain. | Pending. Do not claim Windows, solver, live Barney or FPS acceptance. |

The baseline already implements locked joint translations, bind-pose-derived
frames, direct ragdoll bullet queries, positive-uniform-scale handling, hinge
support and physical-to-bone offset reconstruction. F0 does not remove these.
The unchanged-scope verifier records their core format/creation/pose code.

## Baseline verification issues distinguished from engine defects

- The first Python ZIP extraction discarded POSIX executable modes, causing
  the inherited waf-launch check to fail. Archive modes were restored and the
  baseline reconstructed before implementation; no Xash production fix is needed.
- An initial ad hoc test invocation omitted TMPDIR for one filesystem fixture
  and the sample argument for another. Correct invocations passed. These are
  harness/environment issues, not physics failures.
- Adding diagnostic headers exposed a test-only `private`/`public` macro collision
  with STL declarations. The editor fixture now includes STL before that macro.
- The generic older test enumerator now provides the getter-facade include only
  for the F0 adapter executable; production syntax checks never use it as an SDK.

No live performance numbers have been measured. Portable compilation/test timings
in delivery logs are environment evidence, not game FPS or solver cost.
