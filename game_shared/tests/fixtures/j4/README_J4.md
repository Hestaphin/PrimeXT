# PrimeXT J.4 sample modular weapon and cartridge assets

Copy `scripts/` into your mod/game content folder, same files on server and clients.

- Map entity `weapon_modular`: `weapon_id = test_pistol` — player weapon pickup
- Map entity `ammo_modular`: `cartridge_id = test_fmj` or `test_jhp`, `amount = 25`
- Map entity `monster_human_grunt`: `modular_weapon = test_pistol`, optional `modular_cartridge = test_fmj`

The sample `.weapon` includes `npc_usable = 1` to opt into NPC use. NPCs and players share its ballistic projectile physical parameters, while NPC scheduling/animations remain monster AI driven. The sample ballistic values are gameplay examples.

Enable gravity and (optionally) remote tracer visuals:

```
sv_ballistics_enabled 1
sv_ballistics_tracers 1
```

The editor for creating these assets will be a separate application in J.5, NOT `pxmv`.
