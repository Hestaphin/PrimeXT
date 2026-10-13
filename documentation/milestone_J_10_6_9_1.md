# J.10.6.9.1 — pxwe Windows manifest link fix

## Windows failures

All three supplied PrimeXT logs reach the pxwe link step and fail with:

```text
CVTRES : fatal error CVT1100: duplicate resource. type:MANIFEST, name:1, language:0x0409
LINK : fatal error LNK1123: failure during conversion to COFF
```

The duplicated resource comes from two active embedding paths in J.10.6.9:

1. `pxwe.rc` embeds `1 RT_MANIFEST "pxwe.manifest"`.
2. `add_executable` also lists `pxwe.manifest` as a target source.
   CMake therefore supplies it through `vs_link_exe --manifests` and
   appends its generated `manifest.res` to the link.

The supplied command lines show both resource files in the same link. All use
`/INCREMENTAL`. The uploaded "x64 release" log is actually `RelWithDebInfo`.
`/MANIFEST:NO` alone does not prevent CMake from embedding a separately
supplied user manifest. This is a J.10.6.9 build configuration defect.

The hotfix removes the XML from the executable source list, retains the RC
embedding and `/MANIFEST:NO`, and records an `OBJECT_DEPENDS` dependency
from the resource file to the XML for Ninja/Make generators. The manifest
itself, DPI settings, common-controls activation and application code remain
unchanged. Ubuntu selects the CLI target and never compiled these Windows
resources, explaining why the same defect did not affect that build.

The Windows logs already show successful compilation of `pxwe_win32.cpp`
and `pxwe.rc`. Their failure is localized to combining manifest resources
during linking. This does not certify GUI execution or other build jobs.

## Apply and rebuild

For a J.10.6.9 PrimeXT repository, use the PrimeXT-only incremental patch from
inside that repository (the directory containing tools, client and server):

```sh
git apply --check PrimeXT_milestoneJ.10.6.9_to_J.10.6.9.1.patch
git apply PrimeXT_milestoneJ.10.6.9_to_J.10.6.9.1.patch
```

Regenerate the existing CMake build using its existing options/cache, then
rebuild pxwe. For an already configured Ninja Windows build:

```bat
cmake -S . -B build
cmake --build build --target pxwe --verbose
```

For the standalone Visual Studio build:

```bat
cmake -S tools/weapon_editor -B build-pxwe -A x64
cmake --build build-pxwe --config Debug --target pxwe --verbose
cmake --build build-pxwe --config RelWithDebInfo --target pxwe --verbose
cmake --build build-pxwe --config Release --target pxwe --verbose
```

Use a separate `-A Win32` build directory to retest x86. If using Ninja,
keep its existing architecture-specific developer environment and build
directory; do not pass `-A` to Ninja. Regeneration must remove pxwe.manifest
from the wrapper's `--manifests` argument and its generated manifest.res
from the link. pxwe.rc.res remains the source of resource #1.

## Apple ARM64 failure

The Xash3D macos-14/ARM64 log contains only the `actions/checkout@v6` step.
Recursive submodule fetching stops on:

```text
fatal: unable to access 'https://github.com/ianlancetaylor/libbacktrace/':
Could not resolve host: github.com
```

Git exits 128 while fetching the pinned libbacktrace dependency. The following
"did not contain" message results from the failed fetch; this log does not show
that the pinned commit is invalid. No engine configuration, compilation or
linking was reached. Rerun the failed engine job; a new checkout can recover
from a temporary runner DNS failure. Persistent DNS errors require investigating
the runner/network. No Apple engine source or workflow is changed by this hotfix.

## Verification and limits

`python3 game_shared/tests/verify_j10_6_9.py --ubsan --skip-inherited` passes
**3/3 editor check groups**, including **23/23** real C++ backend cases, native
CLI compilation and installed-data validation without source/Python. The
already completed J.10.6.9 inherited regression results remain applicable;
no runtime/editor C++ algorithms changed.

Fresh patch application and reverse checks reproduce the release's tracked
file bytes and executable flags. This environment still has no CMake, Windows
SDK/compiler or macOS toolchain. The corrected Windows link and recovered
Apple job need rerunning on their native CI runners; they have not been
executed here. No prebuilt Windows executable is included.

CMake reference: [manifest-aware link wrapper implementation](https://github.com/Kitware/CMake/blob/master/Source/cmcmd.cxx)
and [OBJECT_DEPENDS](https://cmake.org/cmake/help/latest/prop_sf/OBJECT_DEPENDS.html).
