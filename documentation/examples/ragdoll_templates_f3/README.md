# F3 presets and review candidates

`human_standard.ragtemplate`, `free_rotation.ragtemplate` and `custom.ragtemplate` are serialized from production built-ins. The first two Barney `.phys` files are deterministic **v5 recorded-bounds review candidates**, generated from the supplied skeleton/bounds audit. They lack actual bind rotations and accessory-filtered rigid/weighted vertices. Regenerate from the matching MDL before simulation/game use. They are not repaired-model, native-drop or mesh-stability evidence.

The historical original `barney_reference.phys` remains v4 in the test reference fixtures; v5 rejects it. The separate v5 editor fixture uses deliberately authored generic parameters for regression testing, not an old-file migration path. No trustworthy second humanoid/monster asset is supplied without its matching model.

See `documentation/milestones/J.10.6.10.3/AUTHORING.md` and `SCHEMA_V5.md` for the profile, overrides, semantic pair rules and regeneration workflow. The example factory regenerates/byte-verifies every preset and candidate through actual format/template code.
