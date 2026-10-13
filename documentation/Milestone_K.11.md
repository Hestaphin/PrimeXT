# PrimeXT / Xash milestone K.11

K.11 merges the three parallel updates that followed J.10.6.9.9.

| Input branch | Changes retained |
| --- | --- |
| J.10.6.9.10 | Map lifetime repairs; fresh-start brush collision cache crash repair; mirror, sky, water, portal and monitor rendering repairs; aligned collision-cache storage and packed records |
| J.10.6.10.9 | Physics asset settings, ragdoll templates and presets, physical hierarchy and render pose reconstruction, joint diagnostics, authoring help and scrollable UI |
| J.11.6 | Version-4 cartridge/weapon schema, loaded-ammunition firing behavior, configurable damage, ballistic persistence, and native weapon editor improvements |

The supplied cumulative J.10.6.10.9 and J.11.6 patches contain older Xash
snapshots. Taking those snapshots would remove the already implemented
world ballistic query, safe entity creation/callback handling and player
model selection repairs. K.11 keeps the J.10.6.9.9 engine implementation and
adds J.10.6.9.10's BSP texture classification repair. The game can therefore
continue to query world penetration through `Xash.WorldBallisticQuery.1`.

The changed PrimeXT files from the three branches have no content overlaps.
K.11 retains their implementations, updates the renderer report and weapon
editor version labels, and fixes the static C runtime selection of the new
MSVC weapon editor test targets. Editor compiler warnings are cleaned
up without changing behavior. Inherited verification adapters now include
the current help, camera and override declarations, settings panel sizes and
review acknowledgement. Legacy controller checks load format-2 fixtures;
the J.11 suites exercise format-4 definitions. The Win32 resource and manifest both use
numeric version `1.11.0.0`; the visible milestone is `K.11`.

## Applying

Use the patch bundle's README to select exactly one route matching the
starting source. The cumulative combined patch is for the two supplied
original ZIP trees. Incremental routes are provided for J.10.6.9.9 and
each of the three branch versions. Apply a combined patch from the parent
containing `PrimeXT-master` and `xash3d-fwgs-master`:

```sh
git apply --check /path/to/the-selected.patch
git apply /path/to/the-selected.patch
```

Rebuild and deploy the PrimeXT client, server and tools. Rebuild Xash when
the selected route changes its sources. Keep engine and game libraries at
matching architectures; definitions should match on client and server.
`r_render_report` and `pxwe --version` report K.11.

## Validation scope

The bundle records the checks run against the merged sources, its exact
source manifest, incoming patch hashes and branch inclusion audit. Map
checks execute production functions with the supplied BSP fixtures;
physics and runtime checks use the existing documented OS/engine/PhysX
adapters. Linux production object compilation is separate from linking
complete game targets.

A Windows SDK, CMake, Qt/PhysX development dependencies and a running game
were unavailable for this merge. Full native Windows builds, PhysX solver
behavior, GUI painting and GPU/gameplay acceptance remain to be checked
with the actual game installation. The separate branch documentation and
tests remain available in this source tree.
