# J.10.6.10.3 — F3 shared physics settings and selective contact

F3 starts from the exact J.10.6.10.2 source: all 2,718 files and executable modes matched its release manifest. PrimeXT changes in this phase; Xash has no incremental changes.

The implementation and available automated evidence are delivered. **The physical exit remains pending.** CMake is unavailable in this environment; the real PhysX 4.1 server/client/pxmv and native drop target were not configured or linked. The matching Barney binary MDL, recordings, running game and Windows/Qt environment are absent. This release does not certify that Barney has stopped spinning or that Studio mesh spikes are repaired. F4 pose reconstruction and game verification remain separate.

## Delivered behavior

- `.phys` **v5 only**: an asset simulation profile, inherited body/joint overrides, explicit contact margins and Off/Selective/Custom pair policy. Versions 1–4 are rejected with regeneration instructions. There is no old-asset converter.
- Shared body/joint construction for runtime and pxmv: tolerance scale, fixed 100 Hz step, mass/inertia, damping, solver counts, speed/depenetration limits, sleep energy, CCD, contact margins, D6 limits/resistance and projection. Interactive hinges use the same frame/projection path while retaining their gameplay motor and damage/release/break metadata.
- D6 resistance is a velocity drive with zero stiffness, zero target angular velocity, separate twist/swing drives and a force limit. It is not a pose spring. Free Rotation still locks all joint translations.
- Generated Human Standard contacts exclude graph neighbors and shapes overlapping after inflation by their contact margins. Explicit semantic role rules override generated defaults and produce clearance warnings where appropriate. Runtime consumes the resolved immutable table; it does not infer anatomical pairs again.
- Stable per-body shape tokens and four-word filter metadata support up to 256 bodies, repeated names across instances and owner reuse. No mutable global registry is read by the callback. Actors/shapes are released before the tokens. Actor `userData` remains the engine edict pointer.
- **Physics settings** in the asset editor and **Physics profile** in custom templates expose profile/body/joint values and pair rules. Apply/Cancel, stale-snapshot rejection, inheritance, pending main-panel fields, role/body rename/delete and save/reload are covered by production-method tests using adapters.
- Authored CCD is restored when animation/kinematic bodies become dynamic, including articulated restore and interactive-part handoff. CCD is not enabled while an actor is kinematic.
- Existing F0 anchor/velocity/pose CSV measurements remain available. A standalone real-SDK capture target adds sampled penetration, landing motion, sleep/wake and localized impulse traces, with input hashes and Off/Authored orientation/timestep/scale comparisons.

## Files and use

Read [SCHEMA_V5.md](SCHEMA_V5.md), [AUTHORING.md](AUTHORING.md), [VERIFICATION.md](VERIFICATION.md), [ACCEPTANCE.md](ACCEPTANCE.md) and [DEFECT_REGISTER.md](DEFECT_REGISTER.md). `documentation/examples/ragdoll_templates_f3` contains three portable presets and two **recorded-bounds review candidates**. Those candidates lack actual bind rotations and filtered mesh samples; regenerate using the matching MDL before game use.

The bundle contains 11 patches: combined and PrimeXT-only increments from F2, F1, F0 and J.10.6.9.9, plus combined and per-project cumulative patches from the supplied originals. Choose one matching route. `tools/verify_patch_routes.py` checks complete source bytes and executable modes for all ten routes; do not apply cumulative and incremental alternatives together.

Build compatible PrimeXT server/client/pxmv binaries, regenerate assets in v5, review their rigs/pairs, and perform the native/game checklist. A successful portable test does not establish solver or gameplay quality.
