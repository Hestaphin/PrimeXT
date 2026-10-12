# J.10.6.10.5 — F5 creature generation and authoring

F5 starts from the exact J.10.6.10.4 release: all 2,781 source hashes/executable modes matched. It adds Human Heavy/Armored, Human Loose, Quadruped/Animal and Creature/Monster presets, bounded descendant-chain expansion, editable chain controls and selected-joint frame/angle guides. The original Human Standard, Free Rotation and Custom presets retain their identities and order. The editable-copy slot now follows the built-in list dynamically.

Implementation and portable verification are delivered. **Actual quadruped/monster/second-humanoid and native physical/game acceptance remain pending.** Only the matching Barney MDL is supplied. The new nonhuman fixtures are synthetic analytic skeletons, not trusted game assets. CMake is unavailable; native Windows/Qt/real PhysX linking, drops, driver rendering, game/network/save execution and FPS were not run.

Seven reusable presets are in documentation/examples/ragdoll_templates_f5. Heavy and Loose Barney v5 candidates were generated from the SHA-checked actual MDL through production extraction. Each has 15 bodies/14 joints/four inferred-axis reviews; Heavy's mass budget is 110 and Loose's is 70. Profile values are starting points requiring drop/contact review. Original uploaded MDL and v4 reference remain unchanged; v5-only runtime rules remain in force.

In pxmv select a preset, Duplicate to edit mappings/profile, set chain edges/branch consent where appropriate, Generate Preview and inspect all mappings, shape/mass/pair warnings and joint guides. Confirm inferred-axis/expanded-chain review before Apply. Physics profile/pairs edits use the existing transactional editor. Cancel and Undo retain the existing protections. Unknown required roles and ambiguous linear-chain branching require explicit author decisions.

```sh
python game_shared/tests/verify_j10_6_10_5.py --ubsan --inherited --barney-model /path/barney.mdl --barney-reference /path/barney.phys --export-dir /output/f5_examples --json /output/f5.json
python game_shared/tests/verify_j10_6_10_5_native.py --build-dir /output/native --physx ON --json /output/native.json
```

See AUTHORING.md, FORMAT.md, CHANGELOG.md, VERIFICATION.md and ACCEPTANCE.md. Runtime .phys stays v5 and contains resolved bodies/joints/pairs, with no template/discovery dependency. No incremental Xash engine changes are required. Deploy matching client/GLSL when upgrading through a cumulative route that includes F4.
