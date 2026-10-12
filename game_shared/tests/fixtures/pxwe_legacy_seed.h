#pragma once
// Keep the inherited controller cases on their original format-2 inputs.
// Current format-4 creation/editing is checked by native_j11_* and runtime_j11_*.
inline void SeedLegacyEditorProject(const pxwe::fs::path& root) {
    modular::Cartridge cartridge;
    cartridge.id = "fmj"; cartridge.chamber = "9x19"; cartridge.projectileType = "fmj";
    cartridge.schemaVersion = 2; cartridge.massGrams = 8.04f; cartridge.diameterMm = 9.02f;
    cartridge.dragCoefficient = .3f; cartridge.referenceVelocityMps = 360.f;
    modular::Weapon weapon;
    weapon.schemaVersion = 2; weapon.id = "pistol"; weapon.runtimeId = 32; weapon.displayName = "pistol";
    weapon.clipSize = 15; weapon.defaultAmmo = 30; weapon.maxCarry = 120; weapon.slot = 1; weapon.npcUsable = true;
    weapon.viewModel = "models/v_pistol.mdl"; weapon.playerModel = "models/p_pistol.mdl"; weapon.worldModel = "models/w_pistol.mdl";
    weapon.fireAnimation = 3; weapon.drawAnimation = 5; weapon.reloadAnimation = 6;
    weapon.primary.behavior = "ballistic"; weapon.primary.mode = "semi"; weapon.primary.animation = 3; weapon.primary.recoilPitch = -2;
    weapon.secondary.behavior = "ammo_cycle"; weapon.secondary.mode = "semi"; weapon.secondary.ammoCost = 0;
    weapon.ammoName = weapon.defaultCartridgeId = "fmj"; weapon.compatible.push_back({"fmj", 360});
    pxwe::fs::create_directories(root / "scripts/cartridges");
    pxwe::fs::create_directories(root / "scripts/weapons");
    std::ofstream(root / "scripts/cartridges/fmj.cartridge") << modular::SerializeCartridge(cartridge);
    std::ofstream(root / "scripts/cartridges/manifest.txt") << "fmj\n";
    std::ofstream(root / "scripts/weapons/pistol.weapon") << modular::SerializeWeapon(weapon);
    std::ofstream(root / "scripts/weapons/manifest.txt") << "pistol\n";
}
