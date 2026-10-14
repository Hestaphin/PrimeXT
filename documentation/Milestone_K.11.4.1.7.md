# K.11.4.1.7 — trajectory, PhysX initialization, tools-only CMake, and Weapon Editor close

Incremental stabilization on **exact K.11.4.1.6**. The cumulative route is for
**clean original PrimeXT** only. The matched Xash3D FWGS engine patch remains the
one already paired with K.11.4.1.6; no engine-file changes in this release.

## Fixed

1. **Ballistic arc length at an apex**: a 20 ms vacuum step can leave the
   projectile at its starting position while the bullet moved both up and
   down. When initial/final velocities oppose, travelled distance is now
   integrated over speed with eight-panel Simpson sampling (rare fallback).
   Ordinary ballistic integration retains its previous displacement and
   distance computation. Range clipping can finish inside a reversal step.
2. **PhysX initialization failure**: guard PVD creation and transport, physics
   and dispatcher creation, extension initialization, scene creation and
   material creation. Failures release any partially constructed PhysX state
   and switch to `GameInitNullPhysics()` without dereferencing a null scene.
   Extension teardown is now conditional on successful init; explicitly release
   materials and reset pointers on physics teardown.
3. **Tools-only build**: build `squish` and `imagelib` when either compilers or
   `pxmv` are enabled, rather than only when compilers are enabled.
4. **Native Weapon Editor**: closing the window no longer requires invalid
   unsaved settings to validate. An uncommittable draft offers to close without
   saving. If Save fails at close time, a second prompt allows discard and
   close (or stay to correct). Ctrl+S/File > Save retains strict validation.
   This does **not** permit exporting invalid definitions, and discarding
   unsaved changes is only done after explicit confirmation.

## Validation limitations

Standalone ballistic code and source-level GUI wiring can run on Linux.
Windows GUI close interaction, full PhysX runtime/failure injection,
full Windows toolchain builds, and real game/GPU map rendering require local
native validation. Do not interpret build-target discovery or source guards
as an end-to-end native PhysX/Windows build.
