**J.10.6.1 — executable modes and exact world penetration**

This increment implements items 1 and 2 from the fix-order table: restoring Xash script permissions and fixing world BSP penetration. It is based on the supplied cumulative J.10.6 patch (SHA-256 71fde1e66fc25f87a25ae788f5bd15dbd8be77e8b1038be9e9bc55242552a547).

All 31 unrelated Xash script mode changes are reversed. The incremental patch changes those paths from 100644 back to 100755; the cumulative patch preserves their original executable modes.

The engine now exposes the optional Xash.WorldBallisticQuery.1 service through the existing native-object callback. The frozen physics API table and its version remain unchanged. Rebuild the patched Xash engine and PrimeXT server together. With an older engine or an unavailable/incompatible service, bullets still fire and hit, but world penetration stops conservatively. The existing non-world exit path remains available.

The query traverses the native point-collision BSP using plane intersections. It locates the first connected solid interval along the original trace ray and its following clear interval. The engine reads its own 16-bit or 32-bit clipnodes, validates bounds and caps traversal depth/work. Fixed point-contents samples are no longer the authority for world penetration.

Entry and exit surfaces are resolved at the actual boundary nodes. Their physical material definitions are resolved at the corresponding world positions. Missing/default material definitions, conflicting material names, missing face geometry and overlapping boundary faces are rejected. World trace hitgroup/materialHash is not used to assert material equality.

The projectile's post-exit offset is limited to half the following clear gap, up to 0.5 units. A second thin wall entirely inside the old offset is therefore still hit by subsequent flight. Transit duration includes the small approach distance between the epsilon-biased trace contact and the actual material entry, the material traversal, and the actual clipped exit offset. The existing transit/save layout is unchanged.

For the reviewed glass 0–2.08 / air 2.08–2.18 / concrete 2.18–4 case, the isolated actual-method test now finds glass exit 2.08, resumes at 2.13, and rejects the following concrete penetration at the tested energy. It no longer exits at 4 while treating the combined span as glass.

**Apply one patch path.** Put the two source roots under a common parent named PrimeXT-master and xash3d-fwgs-master, matching the supplied ZIPs.

For source already patched to J.10.6, apply PrimeXT_Xash_milestoneJ.10.6_to_J.10.6.1.patch with Git:

```bash
git apply --check /path/to/PrimeXT_Xash_milestoneJ.10.6_to_J.10.6.1.patch
git apply /path/to/PrimeXT_Xash_milestoneJ.10.6_to_J.10.6.1.patch
```

For fresh original ZIP sources, apply PrimeXT_Xash_milestoneJ.10.6.1_combined.patch directly. Do not stack earlier cumulative patches or apply both patch paths. Use extraction and patch tools that preserve POSIX executable bits; ordinary text-only patch application does not reliably apply Git mode changes.

**Verify the portable and isolated cases.** With Python 3, a GNU-style C/C++17 toolchain and both patched source roots:

```bash
python3 PrimeXT-master/game_shared/tests/verify_j10_6_1.py
```

Use --xash-root for another Xash directory. The driver checks identical query-contract headers, all 31 executable bits and direct waf launch on POSIX, production BSP traversal, game-side result/material validation, the actual engine-query and PreparePenetration/BeginPenetration methods with explicit service adapters, the existing J.10.6 portable/source suite, and a real server translation-unit syntax check on Linux with PhysX disabled.

The C engine adapter supplies deterministic model/edict/surface services and data. The game adapter uses the real project Vector and ballistic helpers and extracts the unchanged production methods. These checks are behavioral evidence for the repaired routes; they are not a linked engine, real BSP map, PhysX solver, or full SDK test. Three older source-wiring checks are updated because the world reverse-trace implementation has been replaced.

**Limits.** Bounds are 96 units of reverse depth, 8192 units of trace prefix, 256 traversal levels and 8192 visited nodes/leaf spans. Numerically unresolved geometry and tiny clear offsets stop penetration. Exact solid intervals do not recover internal material layers discarded during BSP construction; matching exposed materials does not prove a homogeneous interior. Hidden/touching layered-material accounting remains unsupported.

The other six actionable J.10.6 findings and the two conditional pose/death issues remain for later fixes. This increment does not change shape ambiguity selection, client pose initialization/blending, typed pickup/box behavior or modular respawn.
