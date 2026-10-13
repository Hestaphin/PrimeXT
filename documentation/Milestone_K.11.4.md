# PrimeXT milestone K.11.4

K.11.4 retains the K.11.3 editor/camera repairs and K.11.3.1 whitespace cleanup.
It makes pxmv's capture of unboxed animation bones selectable for both authored
assets and generated candidate previews.

## Full and Partial previews

Choose **Simulation mode** on Physics Asset Editor > **Asset / preview**, or
Template Generator > **Review + apply**. Both selectors share one session setting.

- **Full ragdoll: freeze unboxed bones** is the default. Starting simulation
  captures every skeleton bone and pauses normal playback. PhysX drives bones
  with bodies; the remaining bones retain their captured local pose relative
  to their physical parents. This preserves K.11.3's drop-preview behavior.
- **Partial ragdoll: animate unboxed bones** keeps animation playback active,
  subject to the viewer's existing Pause/playback controls. Bones without bodies
  retain live animation, IK and procedural motion while following their physical
  ancestors. Defined physical body poses still come from PhysX.

These modes select ownership of unboxed bone animation. They do not assign
per-body physics weights, add collision shapes to fingers or override existing
asset constraints. Both modes simulate the same defined bodies and joints.

Switching from Partial to Full during a drop captures the currently drawn
skeleton. Switching back releases it. The existing scene, velocities and
generated candidate are preserved; no drop restart or regeneration is required.
Reselecting Full does not recapture its frozen pose. Stop/Reset releases the
capture in either mode. The selected mode remains available for the next drop
and applies to the generator's Simulate preview as well as the main Simulate.

The setting is not serialized to .phys or .ragtemplate, does not dirty the
asset or bypass generation review, and does not change the game's separate
ragdoll-animation path. Only pxmv preview ownership changes in K.11.4.

## Cumulative original-source patch

Apply `PrimeXT_K.11.4_from_original.patch` in a checkout of the supplied
original PrimeXT-master.zip. Paths are relative to the PrimeXT repository root.
It includes all retained PrimeXT K.11.3.1 changes and this feature.

```sh
git apply --check --binary --whitespace=error PrimeXT_K.11.4_from_original.patch
git apply --binary --whitespace=error PrimeXT_K.11.4_from_original.patch
git diff --check
```

Rebuild pxmv to use the selector. The Xash3D engine remains the K.11.3 engine;
no additional engine patch is required for this tool feature. Release labels
are K.11.4 and weapon-editor resource/manifest metadata is 1.11.4.0.

## Verification

Run from PrimeXT-master:

```sh
python3 game_shared/tests/ragdoll_preview_modes_k11_4.py --ubsan
python3 game_shared/tests/ragdoll_f5_dialog_integration.py --ubsan
python3 game_shared/tests/ragdoll_f4_preview_integration.py --ubsan
python3 game_shared/tests/ragdoll_f3_editor_integration.py --ubsan
```

The mode/start/stop test executes actual editor, generator and Studio capture
methods with an explicit recording scene-build service. Pose tests use real
hierarchy math and synthetic bones at several scales to check both frozen
and animated unboxed hands/branches while physical bodies remain authoritative.
GUI adapters check both page modes, control layout, mode synchronization and
unchanged template/save transactions. Scene-build failure must leave animation
unfrozen. The full template-dialog translation unit is syntax-checked in C++17.

The cumulative source and applied original checkout are checked for whitespace
and exact file bytes/modes. Native Windows/Qt painting, full pxmv linking,
the real PhysX solver, GPU/gameplay and the exact screenshot models are still
outside these adapter tests; GL/GUI development dependencies are unavailable.
