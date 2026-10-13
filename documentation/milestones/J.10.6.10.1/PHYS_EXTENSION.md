# Interim `.phys` extension in F1

F1 retains `.phys` version 4 and adds three optional D6 joint keys:

```text
twist_motion free
swing1_motion free
swing2_motion free
```

Each accepts `locked`, `limited` or `free`, without case. All default to
`limited`; existing default joints serialize as before. If any axis differs
from the default, serialization writes all three keys. Explicit non-default
modes require version 4 and `kind d6`. Hinge definitions keep default D6 mode
fields and their existing hinge limits/axis/break behavior.

Both pxmv and the runtime call `physics_asset_motion.h` to set angular modes.
Their existing X/Y/Z translation locks remain explicit. Swing/twist limit
numbers remain stored and validated even when an angular mode is free or
locked. Saved assets are resolved and have no `.ragtemplate` dependency.

This is the minimum runtime change needed to make F1 Free Rotation real.
Selective contact policy, resistance, joint coordinate frames and shared
construction are F2/F3 work. The authorized breaking schema is deferred, not
replaced by compatibility/migration work. No legacy migration tool is added.
Older binaries do not understand the new keys; rebuild before loading these
candidate assets.

Static/animated collision sections, interactive hinge fields, prop-drive data,
direct ragdoll queries, uniform-scale constraints, network/save/load and
ballistic code retain their previous schema/implementation in this phase.
This is a source-scope statement; native regression behavior still needs the
acceptance run.
