# R01 defect register after F1

This supersedes the input-availability statements in the historical F0 register.
The supplied model and `.phys` now support a concrete fixture. No symptom clips,
native build or live reproduction are available in this workspace.

| Item | F1 evidence / status | Remaining work |
| --- | --- | --- |
| Generator overwrites data without candidate review | Replaced by a separate preview, explicit apply confirmation, cancel and guarded undo; actual methods tested through service adapters | Native controls, title-bar close and edit/save/reload acceptance |
| No reusable template workflow | Strict versioned parsing, export/import, three immutable presets and editable copies implemented | Native file picker and Windows commit run |
| Numeric panel parsing retains a borrowed toolkit pointer across repeated reads | Fixed: float/int readers copy the label once; actual body/joint edit/save/reload check passes with borrowed labels | Native Qt control acceptance |
| Alias ambiguity / wrong hierarchy | Exact aliases, explicit overrides, duplicate/cycle checks and Studio ancestry checks implemented | Broader model corpus; unfamiliar anatomy requires user mapping |
| R01-01 generic anatomical limits | Human Standard still starts at generic ±45°; custom per-role limits are editable | F2 frames and joint-specific anatomy |
| R01-02 absent resistance | No new friction/resistance/drive behavior | F2/F3 tuning and schema |
| R01-03 self-collision suppression | Current owner-wide suppression retained | F3 selective pair policy and overlap tests |
| R01-04 visual spikes/head spin/torso distortion | No symptom-level repair certified | F2 physical drop tests and F4 posing/renderer isolation |
| R01-05 volume mass floor | New candidates use normalized role weights; supplied mass discontinuities independently recomputed | F2 mass ratios, inertia and physically justified profile tuning |
| R01-06 shoulders through tiny neck | F1 logical upper-arm joints attach to chest; supplied Studio ancestry checked | Native fitting/drop acceptance; no stability claim |
| R01-07 unsimulated master ancestor parity hazard | Pose reconstruction source unchanged | F4 renderer/client/preview regression and repair |
| R01-08 AABB fitting direction/accessories | Initial AABB fit/fallback retained; production bounds checked against actual MDL bytes | F2 bone-direction fit, overlap and accessory review |
| R01-09 preview/runtime timestep/contact differences | Angular-mode setter shared; broader construction differences retained | F3 configuration/contact parity |
| R01-10 coupled cone metric limitation | Existing F0 per-axis lower-bound metric retained | Calibrated visualization/full-cone measurement if needed |
| R01-11 missing matching assets/native acceptance | Model/asset now available and audited; native/game/videos/FPS still unavailable | Run the acceptance checklist with this model and new build |

Free Rotation now has real resolved D6 angular modes while preserving explicit
translation locks. This completes that F1 preset's source behavior; it is not a
stability preset and does not repair anatomical quality.
