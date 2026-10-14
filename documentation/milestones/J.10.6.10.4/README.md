# J.10.6.10.4 — F4 Studio pose and skinning repairs

F4 starts from the exact attached J.10.6.10.3 source: all 2,752 file hashes and executable modes matched its manifest. It repairs physical Studio hierarchy reconstruction, preview scale/reflection, compact GPU palette encoding, CPU weight rounding and receipt-time continuity when a bone becomes physically driven. The previous-phase audit also fixes weighted reference fitting to use the same byte-weight rule as rendering.

The implementation and available automated checks are delivered. **The physical/game exit criterion remains pending.** CMake is unavailable in this environment; Windows/Qt/real PhysX linking, native solver drops, GPU shader compilation, game sessions and FPS were not run. A mathematical pose test cannot certify settling, head spin or live mesh quality.

The uploaded matching Barney MDL is now present and SHA-checked. Human Standard and Free Rotation v5 examples were generated through production bind/filtered-vertex extraction: 24 Studio bones, 15 physics bodies, 14 joints and 382 filtered vertices. Human Standard resolves 105 pair rows with 73 allows. Four inferred flexion axes and overlap warnings still require review. The historical uploaded v4 asset remains unchanged; obsolete .phys versions are rejected under F3's v5-only contract.

Build compatible server/client/pxmv and deploy the updated client **and game_dir/glsl/skinning.h together**. Restart the game/rebuild shader caches after replacement. Generate Preview from the matching MDL, review frames/pairs, simulate Off versus Selective, Apply/Save, then run ACCEPTANCE.md. The sample files are review candidates, not certified game assets.

From patched PrimeXT:

```sh
python game_shared/tests/verify_j10_6_10_4.py --ubsan --inherited --barney-model /path/barney.mdl --barney-reference /path/barney.phys --export-dir /output/examples --json /output/f4.json
python game_shared/tests/verify_j10_6_10_4_native.py --build-dir /output/native --physx ON --json /output/native.json
```

The first command uses the exact supplied MDL/v4 reference hashes and explicit engine/GUI/SDK adapters. The second uses real installed dependencies and reports a blocker if configuration cannot start. See VERIFICATION.md for evidence boundaries, POSE_CONTRACT.md for transform conventions, PREVIOUS_PHASE_AUDIT.md for earlier-code findings and DEFECT_REGISTER.md for unresolved symptoms. F5 creature algorithms and preset expansion remain outside this release.
