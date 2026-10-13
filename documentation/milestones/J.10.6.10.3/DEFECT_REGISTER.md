# R01 defect register at F3

| Symptom / cause | Source change or evidence | Status |
|---|---|---|
| Detached anchors / free joint translation | shared D6 factory locks X/Y/Z; reference frame coincidence tested | construction fixed/tested; native anchor stability pending |
| Anatomically wrong shoulders/flexion | F2 torso support, fit, mass regions and reviewed child-local frames retained | source/authoring tested; actual Barney drops pending |
| Persistent head/body spin | explicit speed/sleep/damping profile and velocity-only angular resistance | implemented and setters tested; original symptom not reproduced or physically proven repaired |
| Contact jitter/explosive overlaps | margin-aware generated pair exclusions, explicit matrix and clearance warnings | logic/filter tested; solver contact quality pending |
| Preview/runtime tuning mismatch | shared tolerance/100 Hz, resolved body/joint/contact setup | portable getter/setter parity tested; actual SDK/native parity pending |
| CCD lost after animation handoff | SDK kinematic flags clear CCD; dynamic/restore/prop paths restore profile | actual ragdoll handoff method tested with SDK flag facade; native/game restore pending |
| Owner reuse/stale body identity | immutable instance group, full metadata, indexed owned tokens, actor-before-token teardown | callback/concurrent reads/actual cleanup tested; native lifecycle stress pending |
| Lost overrides/pairs in authoring | draft Apply/Cancel, partial inheritance, stale snapshot checks, name repair, exact persistence | production methods tested with GUI adapters; native widgets/Windows commit pending |
| Stable bodies but spiking/moving mesh | no F3 mesh transform repair | OPEN: F4; requires matching MDL and recordings |
| Unsampled SDK/game/network behavior | no SDK link, live game or actual model in environment | OPEN: native checklist; no certification |
| Frame-time/FPS regression | diagnostics remain optional; generation distances precomputed; callback indexed lookup | OPEN: measure baseline/candidate on the game machine |

The recorded Barney bounds are provenance for reproducible authoring examples, not reproduction of the uploaded visual symptoms. No blanket “Barney fixed” or physical acceptance claim is made.
