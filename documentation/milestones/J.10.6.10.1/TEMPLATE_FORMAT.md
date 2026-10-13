# `.ragtemplate` version 1

Files are bounded to 1 MiB and contain one `ragdoll_template` block. Strings may
be bare tokens or quoted; quoted strings escape only `\"` and `\\`. `//` and
`#` start comments. Parsing is transactional: errors leave the output unchanged.
Unknown/duplicate fields fail; `alias` is the only repeatable field.

```text
ragdoll_template {
    template_version 1
    id "custom_human"
    name "My human"
    description "Editable example"
    anatomy "human"
    mass_budget 70
    linear_damping 0.05
    angular_damping 0.05
    role "pelvis" {
        parent ""
        alias "Bip01 Pelvis"
        override ""
        required 1
        minimum_detail 0
        shape box
        hitgroup 3
        mass_weight 16
        twist_motion limited
        swing1_motion limited
        swing2_motion limited
        swing1 45
        swing2 45
        twist_low -45
        twist_high 45
    }
}
```

| Field | Validation / meaning |
| --- | --- |
| `template_version` | Required; exactly 1 |
| `id`, role ID, `parent`, `anatomy` | ASCII letters/digits/underscore/hyphen; up to 63 bytes; parent may be empty |
| `name`, `description` | Name 1–100 bytes; description 0–200; no control characters |
| `mass_budget` | Finite 0.01–100000; normalized over roles included in generation |
| damping | Finite 0–100; copied to new bodies |
| `role` | 1–128 roles; unique IDs without case; exactly one required root included at every detail |
| `parent` | Existing role or empty for root; cycles rejected |
| `alias` | Exact bone name, case insensitive; up to 63 bytes each, aggregate editable limit 200 bytes; no semicolon or duplicates |
| `override` | Exact bone name, case insensitive; up to 63 bytes; overrides aliases |
| `required` | 0 or 1; a missing required role at the requested detail fails |
| `minimum_detail` | 0 Simple, 1 Standard, 2 Detailed |
| `shape` | `box`, `sphere`, `capsule` |
| `hitgroup` | Integer 0–255 |
| `mass_weight` | Finite 0.001–10000; relative weight among included roles |
| angular motion | `locked`, `limited`, `free`; twist, swing1 and swing2 separately |
| swing limits | Finite 0.001–179.999 degrees |
| twist limits | Finite −180–180 degrees, low strictly less than high |

Identity/name and mapping/profile values are stored independently. IDs are
unique only within a template; built-ins are compiled constants, while imports
are editable copies. Serialization emits every supported field in role order,
using classic-locale, round-trip float precision. Missing optional fields use
the defaults defined in `ragdoll_template.h`; limits remain valid even for free
or locked angular modes so changing modes is safe.

## Mapping and composition

Names are never matched by substrings. Multiple distinct alias matches fail;
an explicit override resolves ambiguity. Two roles cannot claim the same bone.
The nearest included parent role must be an actual Studio ancestor. Missing
optional roles and roles excluded by detail are reported, and generation joins
to the nearest included role ancestor. Skeleton size is 1–256; duplicate names,
bad parent indices and cycles fail.

Confidence 1.0 means explicit override, 0.8 means exact alias with checked
ancestry, and 0 means missing/excluded. These are rule labels, not statistical
probabilities or a physics-quality score.

Generation is deterministic for fixed model bounds, template, detail and asset.
Replace/Merge composition preserves authored collision and interactive data.
Ambiguous authored name/bone matches and invalid combined body graphs fail
without changing the editor asset. A replaced rig uses new role weights; reused
authored/protected bodies keep their original values, so the final combined
mass need not equal the template budget.

Version 1 has no hinge authoring, free translation, joint-frame, friction,
resistance, solver/projection/sleep or collision-pair keys. Such inputs are
rejected. Ordinary `.phys` hinge editing and collision authoring remain
available outside this generator.
