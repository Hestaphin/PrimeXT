# F5 authoring format and runtime contract

Template version remains 1; the two new optional role keys default to disabled for existing templates. Runtime .phys remains v5. Older runtime assets are still rejected and must be regenerated; no migration is added.

```text
role "tail"
{
    parent "pelvis"
    alias "tail"
    required 0
    minimum_detail 1
    shape capsule
    region "tail"
    mass_weight 1
    chain_depth 4
    chain_branches 0
}
```

chain_depth is a bounded number of descendant edges, 0..16. Zero keeps explicit roles only. A root role cannot expand the whole skeleton. chain_branches is 0 or 1 and requires positive depth. The seed's detail level gates the chain. Missing optional seeds are omitted. A linear chain with multiple available children fails with an actionable mapping message; opt into branches only after inspecting the model.

Resolve the explicit roles first and validate the skeleton. Reserve bones referenced by explicit aliases/overrides, including roles excluded at the current detail, so discovery cannot silently restore an intentionally omitted bone. Traverse available descendants in deterministic case-folded bone-name order. An explicit child stops discovery at that branch; its own declared profile/chain owns further descendants.

Each discovered role receives an explicit bone override, generated role ID and parent role. It inherits the seed's mass region/weight, shape, hitgroup, angular modes/limits and tuning. It gets a fresh segment frame with zero child-local anchor/angles rather than copying one seed-specific explicit frame. The explicit seed retains its authored frame. Regenerate after any edit; expanded links can be further edited in the main body/joint panels after Apply.

Expansion cannot exceed 128 roles. Fail transactionally on invalid/ambiguous mappings, cycles, invalid depth or capacity. At the selected depth, deeper Studio bones remain passive and a truncation note is emitted. Generated mappings are reported at review confidence 0.6; no template/discovery work is performed by the game loader.

Mass regions normalize across active roles, including expanded roles, to the selected budget before protected/merged authored bodies are reused. The existing mass-ratio/overlap/contact-clearance warnings remain mandatory review material. Selective contacts are resolved using the same existing graph/overlap policy; explicit semantic pair rules remain concrete body pairs after generation.

Names are aliases/explicit overrides checked against actual hierarchy, not universal anatomy detection. Duplicate aliases found in a model require an explicit override. The preset names are starting conventions; unknown monsters must be authored explicitly. Positive uniform articulated scale remains the supported runtime contract; nonuniform/mirrored articulated construction remains unsupported.
