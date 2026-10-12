# J.10.6.7 — startup selection and render-state repairs

Baseline: the attached J.10.6.6 combined patch applied to the original PrimeXT
and Xash3D FWGS source ZIPs. This release preserves the earlier physics,
ballistics, inventory, pose reconstruction and Windows compatibility changes.

## Reported behavior and evidence

| Report | Evidence and repair | Remaining acceptance |
| --- | --- | --- |
| New game crashes without developer flags | The screenshot identifies `R_StudioSetupPlayerModel`, called from `CL_ProcessEntityUpdate`. The new CPU latch path calls selection without a draw entity; the engine dereferences `RI.currententity`. Selection now validates the index first and uses the requested player's entity when the draw context is absent or belongs to another entity. Missing player-info and changing entity fallbacks are handled. Menu-preview and active dead-player selection remain covered. | Rebuild the engine's `ref_gl.dll` as well as the client and retest without `-dev 2`. |
| Crossfire to env_cubemap crashes during model precaching | The stack reaches `Mod_ProcessRenderData` and the client model-data callback. That callback overwrites the active entity/model/header and leaves temporary bone-setup pointers installed; its null-context path also crashes in reproduction. It now initializes a missing context and restores the previous entity, model, Studio header and bone setup after cache creation/destruction. A null creation buffer is rejected. | The screenshot's client frames have only export labels. These repairs cover the implicated lifecycle, but the exact instruction in that crash is not established without a minidump and matching symbols. |
| sv_ragdoll_debug 1 draws magenta triangles across the map | PhysX world and legacy actors retain their default visualization flags. Ragdoll-only mode now clears those flags and enables the selected ragdoll bodies. General physics debugging restores all actor visualization; disabling visualization stops it through the scene scale. Scene enumeration runs only while visualization is enabled. | Native PhysX visualization needs a live run. Level 2 keeps the existing scene joint-frame/limit parameters. |
| Sky, water, lighting and reflections differ from the reference | Instance setup resets selected cubemaps on each draw. Bone-cache early exits skip the GL transform and cubemap bookkeeping for local players, jiggle, external, partial and ragdoll poses. These are repaired; initial cubemap selection also works at the origin, and coincident probes produce a finite blend factor. | These are demonstrated renderer-state bugs, not proof that every difference in the screenshots has one cause. Full sky/water/reflection appearance is not certified. |

`-dev 2` changes the engine's player-model selection branch, which explains how
it can mask the first crash. The successful logs contain no crash stack; they
do not establish why logging appears to avoid the second crash.

The patched screenshots/logs use engine build 4209 / v0.21; the reference image
uses build 4277 / v0.22. That is an additional comparison variable. Sky-side
images are reported as loaded. The missing next numbered water-normal frame
is an enumeration stopping condition, not evidence that all water normals are
absent. Debug shader uniform warnings alone do not establish the black-sky cause.

## Capture the remaining renderer state

This release registers the read-only console command `r_render_report`. It logs
the J.10.6.7 client marker, render cvars, active map, sky visibility and extents,
six sky texture handles, sky-camera origin/scale, cubemap validity/loading and
last-frame sky/mirror counters. It is safe to call from the menu.

For an affected map, capture a report with `gl_allow_3dsky 1`, then another with
`gl_allow_3dsky 0`, waiting for a rendered frame between each command. Restore
the setting afterward. This distinguishes a missing sky pass or camera from
missing texture/cubemap resources without changing shaders or map assets.

## Verification

Run from the PrimeXT source tree with Python 3 and GNU/Clang-compatible C/C++
compilers:

```sh
python3 game_shared/tests/verify_j10_6_7.py --ubsan
```

The 20 new cases execute the production player selector, CPU instance setup,
model-data callback, bone-cache bookkeeping, shared Studio bone evaluation,
render-report function and physics DebugDraw method. PrimeXT model/entity,
Studio animation/IK/procedural and math types/code are real. Engine services,
GPU cache allocation and PhysX actors/scene calls are explicitly adapted.
The selector's engine/entity adapters do not certify an engine ABI build.

Fourteen failures are reproduced against the exact J.10.6.6 baseline and pass
after the repairs. Control cases include developer mode, menu preview,
active dead-player selection and scaled static sky-model transforms. The
sky-model control uses a Studio fixture at the example map's origin/scale,
not the unavailable skybox.mdl asset. It also passes before the patch and
does not reproduce the screenshot's missing skyline.

Available acceptance passes: J.10.6.7 groups 6/6, inherited J.10.6.3 groups
39/39 (including J.10.6.6 compatibility), the portable/source suite 41/41,
J.10.6.5 pose cases, native validator samples and 16 editor unit cases. Counts
overlap through the inherited wrappers. Checked real client/server translation
units pass with PhysX disabled; the new method cases use UBSan.

Full Windows/engine/PhysX linking, native model/GPU allocation, actual map
loading, network timing and live OpenGL visual parity were not run here.
The supplied model assets, matching crash symbols and minidump are needed for
further localization if the remaining runtime symptoms persist.

## Windows retest

1. Apply the increment to exactly J.10.6.6, or apply the combined patch to the
   original two source ZIPs. Use one route.
2. Rebuild and deploy the matching client DLL, server DLL and engine/ref_gl
   targets. Deploy the rebuilt `ref_gl.dll`; the startup repair is in it.
3. Test New game with `-console` and without `-dev 2`. Then test LAN crossfire,
   `map env_cubemap`, a map restart and returning to the menu.
4. Test `sv_ragdoll_debug 1` and `0`. General physics debugging is selected by
   the existing `-physxdebug` launch flag; compare launches with and without
   that flag. Ragdoll-only mode should no longer outline the BSP collision mesh.
5. Compare the scene using the same game content and render cvars. If visuals
   remain wrong, include `r_render_report` output in a fresh engine log. If a
   crash persists, supply its `.dmp` and matching symbols/DLL build metadata.
