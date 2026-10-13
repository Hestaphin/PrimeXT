# J.10.6.9.3 format-3 authoring sample

This sample demonstrates damage coefficients, nested category metadata,
default/random/mode/ammunition asset conditions and ammunition-specific
sound/HUD overrides. Its coefficients preserve the legacy example's reference
muzzle damage approximately (20 for FMJ, 18 for JHP).

J.10.6.9.3 adds the shared format. Conditional gameplay assets and current-energy
damage are step 5, so this sample currently exercises validation and data
round trips. The game still uses flat fields and its prior damage implementation.

The model, sound and sprite paths reference Half-Life assets. No copyrighted
binary assets are bundled; use an installation that supplies those resources.
Repeated paths in variants intentionally make this a format example.

The standalone validator target validates directly without changing a mod:

```sh
primext-weapon-validate documentation/examples/modular-format3
```

Portable CLI builds also support `pxwe --validate <sample-folder>`. The Windows
`pxwe` application opens the folder in its graphical editor.

For native editor save/reload testing, copy this directory to a disposable
test-mod root and open that copy with `pxwe`. Preserve the original sample.
To merge into an existing pack, add the individual files and IDs to its
manifests; do not overwrite populated manifests. The IDs match the older
example and must not be registered twice in one pack.

See `documentation/modular_format3_J10_6_9_3.md` for the full format and scope.
