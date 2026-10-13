# PrimeXT / Xash milestone K.11.0

K.11.0 retains the K.11 merge of J.10.6.9.10 map repairs, J.10.6.10.9
physics/ragdoll refinements and J.11.6 weapon/ballistic refinements. It fixes
the three defects confirmed by the K.11 source audit.

The studio collision-cache loader now bounds the table, entries and used
lumps against their containing file/data block, checks reads, rejects
overlapping lumps and validates facet ranges, plane references and finite
geometry. Runtime allocation arithmetic is checked before allocation.
Partially read arrays remain local until validation succeeds, so rejected
caches can be removed and rebuilt without retaining partial mesh state.
Presence queries and cache appends share the checked table reader. Newly
written cache headers initialize unused lump records.

Full ragdoll activation reapplies each resolved body's CCD setting after
clearing kinematic ownership. Profile settings and explicit body overrides
are preserved on both initial activation and later reactivation.

Subview texture slots are committed after successful texture creation,
framebuffer creation and attachment validation. Failed allocations release
their provisional resources and preserve the previous framebuffer binding.
Failed attempts do not consume texture or framebuffer slots. The fallback
copy path also rejects missing textures. A framebuffer index of zero remains
valid; allocation failure is represented by a negative index.

User-visible renderer and weapon-editor version labels are K.11.0. The
Win32 resource and manifest use numeric version `1.11.0.1`.

Only one cumulative patch is supplied:
`PrimeXT_Xash_milestoneK.11.0_combined.patch`. Its base is exactly the supplied
original `PrimeXT-master.zip` and `xash3d-fwgs-master.zip`, whose file bytes
were checked against the original snapshot. It includes all retained K.11
changes and these fixes. Extract both original archives side by side and run
the following commands from their parent directory:

```sh
git apply --check /path/to/PrimeXT_Xash_milestoneK.11.0_combined.patch
git apply /path/to/PrimeXT_Xash_milestoneK.11.0_combined.patch
```

Rebuild the engine, PrimeXT client/server and required tools for the same
architecture. The engine fixes retained by K.11, including world ballistic
queries and map texture classification, remain in this cumulative patch.

Validation for this maintenance change:

- 56/56 focused compilation/regression groups passed under ASan/UBSan or
  the recording PhysX adapter, including 22 corrupt-cache cases, valid cache
  appends, full ragdoll reactivation, driver allocation failures, framebuffer
  rollback, valid portals/monitors and texture reuse.
- The nested construction/filter/CCD/lifetime group passed 98 assertions.
- 23/23 inherited cache/subview cases passed with the supplied
  `func_physbox.bsp` and `portal_test.bsp` fixtures.
- 10/10 complete Linux production translation-unit compilations passed at
  `-O0` and `-O2`: client subview/backend/render main and client/server mesh
  descriptions. Server compilation used PhysX disabled.
- Server and client J.11.3 loaded-ammunition firing integration checks passed.

Run the new focused checks from `PrimeXT-master` with:

```sh
python3 client/render/tests/verify_k11_0.py
```

These checks use documented filesystem, engine, GL and PhysX adapters.
Complete native Windows builds, Qt editor execution, the real PhysX solver
and engine/GPU gameplay were not available for this validation.
