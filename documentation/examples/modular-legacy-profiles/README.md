# J.6 optional legacy-style test profiles

These four `.weapon` profiles reuse shipped GoldSrc model paths and several
source-derived clip sizes/weapon timing values. They are **data-authored test
profiles**, NOT exact conversions or replacements for the built-in classes.
Projectile mass, drag, muzzle velocity and damage are illustrative gameplay
values, because the original hitscan classes do not define those quantities.

Copy `scripts/` into an empty mod overlay, merge manifest entries rather than
overwriting another mod's manifests, and verify runtime IDs 40–43 and reserved
ammo slots do not conflict with your mod. Register map `weapon_modular`
entities with `weapon_id = j6_glock_profile`, etc.; use `ammo_modular`
with `cartridge_id = j6_9mm_fmj` (etc.) for ammunition.

`j6_glock_profile`, `j6_mp5_profile`, `j6_python_profile` demonstrate ordinary
modular firing behavior. Their existing native counterparts remain unchanged
and may use distinct events, spread, idle behavior, recoil and prediction.
`j6_shotgun_profile` exercises multi-pellet ballistics but does NOT implement
the original shotgun's per-shell reload, pump state, or double-shot secondary.
The existing shotgun, MP5 grenade launcher, Python zoom and other specialized
legacy mechanisms continue running in their original native weapon classes.

The generic runtime can be used by players and (currently) the supported
human-grunt NPC adapter. It is not yet wired to every NPC class or turret.

This pack intentionally does not redefine legacy classnames or overwrite
existing weapons. True behavior-by-behavior legacy replacement remains an
in-game comparison task, not an achieved J.6 result.
