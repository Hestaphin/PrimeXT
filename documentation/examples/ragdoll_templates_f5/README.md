# F5 presets and review data

Seven reusable templates: Human Standard, Free Rotation, Custom, Human Heavy/Armored, Human Loose, Quadruped/Animal and Creature/Monster. Duplicate/import in updated pxmv to edit mappings, chain depth/branch consent, profiles, frames and semantic pairs. Generate from the actual model and review before Apply/Save.

barney_f5_human_heavy.phys and barney_f5_human_loose.phys were generated through production extraction from the supplied 129,104-byte MDL (SHA256 d326ef89e83489db85e1cc1d3c554f1b7cbc9c43c4bc7a940d76b0d75e1a393f). They are v5 review candidates with 15 bodies/14 joints/four inferred flexion axes. Heavy's budget is 110, Loose's is 70. Native solver/game acceptance remains pending. Read the matching review reports and generation_provenance.json.

The historical uploaded v4 reference (SHA256 8040edc7a4b5de91279730f692a64df41167a4ee42cdcb18777b58f848777490) and MDL remain unchanged and are not redistributed. v5-only loading remains the runtime contract; regenerate obsolete assets.

test_fixtures contains labeled synthetic quad/creature assets and observations, generated from the analytic skeletons in game_shared/tests/fixtures/j10_6_10_5/skeletons.h. They have **no matching MDL and are not game-ready sidecars**. The second naming convention test is also synthetic. Additional actual model and native acceptance is required.
