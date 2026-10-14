# PrimeXT milestone K.11.3.1

K.11.3.1 retains all K.11.3 editor, simulation and weapon-camera changes.
It removes 46 inherited trailing-whitespace errors in
`server/physx/physx_impl.cpp`, including the shape loops, collision and conveyor
branches, skeleton loops and actor pose setters. The whitespace-insensitive
diff of this source file against K.11.3 is empty; no physics logic changes.
Release labels now read K.11.3.1 and the weapon editor resource/manifest version
is 1.11.3.1. Xash3D engine changes remain those supplied with K.11.3.

The previous K.11.3 validation checked the maintenance delta for whitespace
but missed errors inherited by the cumulative patch. K.11.3.1 checks the entire
diff from the original source, before and after generating the deliverable.

## Apply in a PrimeXT Git repository

`PrimeXT_K.11.3.1_from_original.patch` is a cumulative PrimeXT-only patch with
repository-root paths such as `server/physx/physx_impl.cpp`. Apply it to the
supplied original PrimeXT source checkout, rather than an already patched
K.11.x checkout. The patch includes K.11.3 and its predecessors.

```sh
git switch -c milestone-k11-3-1
git apply --check --binary --whitespace=error PrimeXT_K.11.3.1_from_original.patch
git apply --binary --whitespace=error PrimeXT_K.11.3.1_from_original.patch
git diff --check
rm PrimeXT_K.11.3.1_from_original.patch
git add -A
git diff --cached --check
git commit -m "PrimeXT milestone K.11.3.1"
git push -u origin milestone-k11-3-1
```

Use your actual destination branch for the final push. The strict apply and
both whitespace checks should succeed with no diagnostics. `--whitespace=nowarn`
only suppresses apply warnings; it does not clean whitespace in source.

Validation: the cumulative source diff passes `git diff --check`; strict
application to a fresh original archive succeeds; staged and unstaged
post-application diffs pass the same check; the applied source bytes and modes
match the final PrimeXT tree. K.11.3.1 needs no additional runtime acceptance
for its whitespace and release-label changes. The native GUI and actual PhysX
limitations recorded for K.11.3 still apply.
