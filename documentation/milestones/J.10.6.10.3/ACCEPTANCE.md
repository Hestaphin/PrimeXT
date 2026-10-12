# Native and gameplay acceptance

Keep dated logs, model/asset/binary hashes, settings, model scale, frame timing and recordings. Perform these checks on the dependency-complete build machine with a newly generated v5 rig from the matching MDL.

## Native builds

Run `verify_j10_6_10_3_native.py --build-dir /outside/source/build --json /output/native.json`, supplying the generator, architecture, toolchain/triplet and dependency prefixes needed for your machine. Build server, client and pxmv with PhysX ON, and repeat OFF in another directory. Real Qt/Win32 interaction, file overwrite/reopen and model switching must be exercised after linking.

Configure the separate real-PhysX capture target:

```sh
cmake -S game_shared/tests/native_f3 -B /outside/source/f3-drop -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/path/to/physx
cmake --build /outside/source/f3-drop --config RelWithDebInfo
python game_shared/tests/run_f3_native_drops.py --binary /path/to/primext-f3-drop --asset /path/to/fresh/barney.phys --model /path/to/matching/barney.mdl --out /output/barney-drops --scales 0.5 1 2
```

The standalone CMake project uses PrimeXT's PhysX finder; it may need the native vcpkg toolchain and matching library architecture. It targets PhysX 4.1. It was only declaration-based syntax-checked here.

The runner extracts and hashes actual Studio v10 bind bones, never an obsolete `.phys`. It compares Off and the saved authored policy in four orientations at 0.01 and stressed 0.02 s steps. Each 18 s capture checks native getter configuration, records anchor separation, speed, relative angular speed, root frame displacement, sampled contact penetration and sleep/wake events, and applies one localized impulse after 15 s. The late 10–15 s window separates landed motion from the drop. Exit 0 means finite capture completed; **review quality separately**. Contact penetration samples at most 64 reported points per pair; scene contact totals exclude CCD. The harness has no rendered mesh, map entities, damage, network, native FPS comparison or engine freeze behavior.

## Preview physical review

- Generate using the actual Barney MDL, inspect collider sizes/accessory exclusions and shoulder support, then review inferred flexion signs. Retain every relevant Studio bone.
- Check Off first, then Selective. Inspect enabled pairs and clearance; test explicit allow/deny and Off toggling without losing rows. Correct overlaps before tuning resistance/solver counts.
- Drop legs/head/shoulder first at normal and stressed timing, with repeated runs and the same geometry/settings. Anchors must remain connected, travel bounded and bodies finite; no catastrophic interpenetration, flip, persistent spin or ongoing landing jitter. Calibrate numeric limits to the measured reference span and step before calling any run passed.
- Confirm sleeping follows settling and a localized impulse wakes the appropriate body. Compare normal/half/double uniform scale. Pause and check visible mesh separately from stable colliders.
- Confirm runtime/preview getter settings and pair tables match. Reopen settings and check inherited/explicit values, stale Apply protection, template Export/Import and regenerated preview behavior on native widgets.

## Gameplay and lifecycle review

- On a flat test map use `sv_auto_death_ragdoll 1`, controlled non-gibbing shots and `sv_ragdoll_bullet_impulse_scale 0` first. Record at least ten varied deaths with debug colliders; restore impulses and test pellets/explosions/limb hits.
- Verify manual ragdoll bullet hits/hitgroups, living Studio hitboxes, material/decal ownership and modular ballistics. Ragdoll shapes must remain scene-query disabled.
- Test regenerated v5 static, dynamic, animated/bodygroup and hinged assets: precise movement/bullet collision, conveyor and trigger behavior, prop mode/motor/release/detach/break and bodygroup switching. Test `_cp` import and no-asset fallback too.
- Exercise partial ragdoll, physical animation, repeated animation↔physics transitions, sleep/freeze/resume and save/load. Check CCD on every dynamic handoff and explicit CCD Off overrides. Rebuild policy identity from the definition; do not reuse saved token addresses.
- Delete/recreate entities during map transitions and repeat many instances with identical bone names and recycled owners. Stop/fetch simulation before destroying/rebuilding shapes; check no stale token access or leaked actors/joints.
- Compare server/client presentation and interpolation. Repeat with a second humanoid and a nonstandard monster after providing actual matching models. Check unsimulated descendants/ancestors and mesh spikes as F4 work.
- Measure diagnostics Off/On CPU and FPS against the exact F2 baseline under the same map, body count, contacts and recording settings. Portable checks provide no FPS result.

All items above remain pending in this environment. Do not mark F2/F3 physical exit or F4 rendering/game acceptance complete on the basis of the portable tests or finite CSV alone.
