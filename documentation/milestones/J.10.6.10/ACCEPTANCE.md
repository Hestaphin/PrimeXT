# F0 native / matching-model acceptance — pending

Record results here rather than treating automated getter/method tests as live
acceptance. All boxes below are pending in the delivered environment.

## Inputs and builds

- [ ] Supply the exact installed `barney.mdl`, its current reference `barney.phys`
  and the referenced recordings. Record SHA256 and model scale for both assets.
- [ ] Build PrimeXT server/client and pxmv with actual PhysX 4.1 and supported
  dependency toolchains. Build Windows x64 Debug/Release and x86 Debug/Release.
- [ ] Build the normal supported Linux configuration; test a PhysX-disabled
  tools build as well. Record full configure/link output and resulting binaries.
- [ ] Confirm the loaded binaries belong to J.10.6.10; applying a source patch
  does not update the existing executable or DLLs.
- [ ] Build Xash only if checking the complete cumulative distribution. Its
  source is unchanged from J.10.6.9.9 by this F0 incremental milestone.

Use normal project build instructions and vcpkg features. The automated replay
script requires a GCC-compatible compiler; it is not an MSVC target-build command.

## pxmv reproduction

- [ ] Back up the installed reference asset. Load that exact model and asset,
  recording sequence, bodygroups, model orientation, gravity and scale.
- [ ] Run with Diagnostics off to record visible baseline behavior/FPS.
- [ ] Enable Diagnostics, start Simulate, and record body wireframes plus mesh
  through the failure. Export the new `.phys.f0.N.csv` without saving the asset.
- [ ] Verify exported comments match the simulated settings/asset, including
  unsaved changes. Verify the original `.phys` hash and earlier CSV are unchanged.
- [ ] Compare anchor separation and angular speed with pose link deltas. Pause
  animation separately from physics; the existing Simulate path keeps stepping
  while animation is paused. Use a recorded single frame to compare stationary
  colliders and mesh; Reset ends simulation and restores animated ownership.
- [ ] Inspect the Studio ancestor above the physical root and any vertices
  influenced by both simulated and unsimulated bones. This is not automatically
  the cause of every spike.
- [ ] Exercise normal and stressed frame intervals. Log the four-step preview
  cap/backlog rather than assuming its timestep matches the game.
- [ ] Toggle diagnostics during simulation, reset/restart, reload a model, hide
  the editor, and export. No crash, stale model labeling or authoring changes.
- [ ] Inspect controls at native DPI; adapter layout tests are not screenshots.

## Game baseline capture

Record the existing cvar values before changing diagnostic test settings.

```text
sv_auto_death_ragdoll 1
sv_ragdoll_debug 2
sv_ragdoll_diagnostics 1
cl_ragdoll_diagnostics 1
sv_ragdoll_bullet_impulse_scale 0
```

- [ ] Spawn Barney on a flat map, use controlled non-gibbing shots and record
  activation, fall and settled state. Save server/client console output and clip.
- [ ] Repeat with `sv_ragdoll_freeze 0` to distinguish real settling from the
  existing optional freeze. Restore the original freeze setting after testing.
- [ ] For a brief failure capture, use diagnostic value 2, then return to 0/1.
  Per-step console output has a substantial observational cost.
- [ ] Inspect invalid/missing counts before drawing conclusions from low error.
  Kinematic/frozen bodies are reported separately from sleeping dynamics.
- [ ] Compare client ancestor/child deltas once snapshot targets stop moving;
  driven target lag during interpolation is expected and measured separately.
- [ ] Repeat baseline falls in head/shoulder/leg-first orientations. The later
  repaired-feature gate requires at least 10 varied deaths and additional rigs.
- [ ] Restore original bullet/blast tuning; verify a sleeping/frozen corpse
  responds to limb shots/explosions as before. Check SP/client-server agreement.
- [ ] Exercise save/load, entity deletion/reuse and map changes with diagnostics
  toggled. Each actor recreation/capture has fresh metadata; no stale pointers.

## Performance and functional regression baseline

- [ ] Use the same map, camera, corpse count and duration for diagnostics 0, 1
  and a short 2 capture. Record FPS distribution and CPU/GPU frame times using
  the normal profiler/counters. Record recorder sampling cost separately.
- [ ] Confirm default-off diagnostics do not allocate or scan body/joint/bone
  lists for measurements; the runtime retains a constant mode/reset check.
- [ ] Verify static and animated collision proxies, bodygroups, interactive hinge
  damage/release/break, modular weapon bullets and ragdoll direct query ownership.
- [ ] Preserve clips/logs and update the defect register with actual reproduction
  evidence before applying stability or renderer repairs.

No fixed error/FPS threshold is certified yet. Calibrate world-unit, bind-span,
angle, penetration and settling thresholds using the actual model/time step.
Zero per-axis overshoot does not certify the whole D6 cone or anatomical limits.
