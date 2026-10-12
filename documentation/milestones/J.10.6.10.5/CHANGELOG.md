# F5 changes

- Add four presets to the three existing presets: Human Heavy/Armored, Human Loose, Quadruped/Animal and Creature/Monster. Human variants reuse the checked anatomy while changing resolved mass/resistance/limits; all joint translations stay locked.
- Quadruped maps pelvis/chest/head and four validated limb chains, with optional neck/feet/jaw/tail, regional mass budgets, hitgroups and reviewed lower-limb flexion. Animal neck fitting uses its actual child span rather than the human unit-up special case.
- Creature maps an editable core plus optional limb/tail/wing seeds. Expand only declared descendants, in deterministic bone-name order, within a depth and 128-role budget. Preserve explicitly mapped/detail-excluded bones; reject unexpected branching unless consent is enabled.
- Add optional chain_depth/chain_branches authoring fields and controls. Expanded links inherit profile/region/limits and infer fresh segment frames. Report expanded mappings/depth truncation and require chain review before Apply. Keep runtime v5 resolved and unchanged.
- Replace hard-coded editable-copy index with the built-in count. Preserve protected presets, transactional profile/pair edits, stale-preview checks, import/export, Cancel and Undo.
- Add selected-joint frame axes, separate twist/swing/hinge angle arcs, and a parent/child anchor-gap line. The toggle leaves body/collision wireframes visible and restores caller GL state. These are authoring guides, not a complete solver cone-error metric.
- Generate actual-model Barney Heavy/Loose v5 candidates and labeled synthetic quad/creature test fixtures. Update inherited example checks to validate historical published files while F5 checks all seven presets. Extend editor adapters for the new production guide helpers.
