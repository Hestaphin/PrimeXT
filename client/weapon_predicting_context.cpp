/*
weapon_predicting_context.cpp - part of client-side weapons predicting implementation
Copyright (C) 2025 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "weapon_predicting_context.h"
#include "client_weapon_layer_impl.h"
#include "hud.h"
#include "utils.h"
#include "weapons/glock.h"
#include "weapons/crossbow.h"
#include "weapons/python.h"
#include "weapons/mp5.h"
#include "weapons/shotgun.h"
#include "weapons/crowbar.h"
#include "weapons/tripmine.h"
#include "weapons/snark.h"
#include "weapons/hornetgun.h"
#include "weapons/handgrenade.h"
#include "weapons/satchel.h"
#include "weapons/rpg.h"
#include "weapons/egon.h"
#include "weapons/gauss.h"
#include "weapons/modular.h"
#include "modular_weapons.h"
#include "parsemsg.h"
#include <cstring>
#include <array>

extern void ResetClientWeaponPredictionForModularReload();

// J.4: server sends its canonical definition fingerprint and each registered
// cartridge ammo-slot mapping. Unverified/mismatched clients do not predict
// custom attacks; the server still owns damage and bullet flight.
namespace {
bool g_modularDefinitionsLoaded = false;
bool g_modularVerified = false;
// Mapping pointers reference Registry::Cartridge strings. Clear these BEFORE
// a registry reload destroys their backing storage.
std::array<bool, MAX_AMMO_SLOTS> g_modularMappedSlots{};
void ClearModularAmmoMappings()
{
    for (int slot = 1; slot < MAX_AMMO_SLOTS; ++slot)
        if (g_modularMappedSlots[slot])
            CBaseWeaponContext::AmmoInfoArray[slot].pszName = nullptr;
    g_modularMappedSlots.fill(false);
}

void EnsureModularDefinitionsLoaded() {
    if (g_modularDefinitionsLoaded) return;
    g_modularDefinitionsLoaded = true;
    modular::SharedRegistry().Load([](const std::string& path, std::string& text) -> bool {
        int length = 0;
        char* data = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(
            const_cast<char*>(path.c_str()), 5, &length));
        if (!data) return false;
        const bool valid = length >= 0 && length <= 65536;
        if (valid) text.assign(data, static_cast<size_t>(length));
        gEngfuncs.COM_FreeFile(data);
        return valid;
    });
}
}
bool ModularClientDefinitionsVerified() { return g_modularVerified; }
// Called only after all prediction contexts containing definition pointers
// have been destroyed. The next request reloads the virtual filesystem.
void ResetModularClientDefinitions()
{
    ClearModularAmmoMappings();
    // Weapon-selection metadata can also retain definition-owned c_str()
    // pointers. Drop those entries while the old registry storage is alive.
    for (const auto& weapon : modular::SharedRegistry().Weapons()) {
        if (weapon.runtimeId >= 0 && weapon.runtimeId < MAX_LOCAL_WEAPONS)
            CBaseWeaponContext::ItemInfoArray[weapon.runtimeId] = ItemInfo{};
    }
    g_modularDefinitionsLoaded = false;
    g_modularVerified = false;
}

int __MsgFunc_ModCfg(const char* name, int size, void* buffer) {
    BEGIN_READ(name, buffer, size);
    const int slot = READ_BYTE();
    const uint32_t digest = static_cast<uint32_t>(READ_LONG());
    const std::string cartridge = READ_STRING();
    END_READ();
    if (slot == 0) {
        // Every configuration header is a new verification epoch (also after
        // reconnect without a client-DLL reload). Discard old weapon contexts
        // before Registry::Load() replaces their definition pointers.
        ResetClientWeaponPredictionForModularReload();
        ResetModularClientDefinitions();
    }
    EnsureModularDefinitionsLoaded();
    auto& registry = modular::SharedRegistry();
    if (slot == 0) {
        g_modularVerified = registry.Valid() && registry.Fingerprint() == digest;
        if (!g_modularVerified)
            gEngfuncs.Con_Printf("[modular] Server weapon/cartridge definitions differ; prediction disabled.\n");
    } else if (g_modularVerified && slot > 0 && slot < MAX_AMMO_SLOTS) {
        const auto* def = registry.FindCartridge(cartridge);
        if (!def || registry.Fingerprint() != digest) {
            g_modularVerified = false;
        } else {
            CBaseWeaponContext::AmmoInfoArray[slot].pszName = def->id.c_str();
            g_modularMappedSlots[slot] = true;
        }
    }
    return 1;
}

CWeaponPredictingContext::CWeaponPredictingContext()
{
}

void CWeaponPredictingContext::PostThink(local_state_t *from, local_state_t *to, usercmd_t *cmd, bool runfuncs, double time, uint32_t randomSeed)
{
	m_playerState.time = time;
	m_playerState.randomSeed = randomSeed;
	m_playerState.runfuncs = runfuncs;
	ReadPlayerState(from, to, cmd);
	ReadWeaponsState(from);

	const bool playerAlive = m_playerState.deadflag != (DEAD_DISCARDBODY + 1);
	CBaseWeaponContext *currWeapon = GetWeaponContext(from->client.m_iId);
	if (currWeapon)
	{
		if (runfuncs) {
			HandlePlayerSpawnDeath(to, currWeapon);
		}

		if (playerAlive && m_playerState.viewmodel)
		{
			if (m_playerState.nextAttack <= 0.0f) {
				currWeapon->ItemPostFrame();
			}
		}

		if (playerAlive && cmd->weaponselect)
		{
			// handle weapon switching on weapons code side
			HandleWeaponSwitch(from, to, cmd, currWeapon);
		}
		else
		{
			// does not changing weapons now, keep weapon ID same
			to->client.m_iId = from->client.m_iId;
		}

		// check for desync between local & server-side weapon animation
		if (runfuncs && (m_playerState.activeWeaponanim != m_playerState.weaponanim))
		{
			gEngfuncs.pfnWeaponAnim( m_playerState.weaponanim, 0 );
			m_playerState.activeWeaponanim = m_playerState.weaponanim;
		}
	}

	UpdatePlayerTimers(cmd);
	WritePlayerState(to);
	WriteWeaponsState(to, cmd);
}

void CWeaponPredictingContext::ReadPlayerState(const local_state_t *from, const local_state_t *to, usercmd_t *cmd)
{
	m_playerState.cached.buttons = from->playerstate.oldbuttons;
	int32_t buttonsChanged = (m_playerState.cached.buttons ^ cmd->buttons);
	m_playerState.buttonsPressed = buttonsChanged & cmd->buttons;	
	m_playerState.buttonsReleased = buttonsChanged & (~cmd->buttons);

	m_playerState.viewAngles = cmd->viewangles;
	m_playerState.buttons = cmd->buttons;
	m_playerState.flags = from->client.flags;

	m_playerState.deadflag = from->client.deadflag;
	m_playerState.waterlevel = from->client.waterlevel;
	m_playerState.maxSpeed = from->client.maxspeed;

	m_playerState.fov = from->client.fov;
	m_playerState.weaponanim = from->client.weaponanim;
	m_playerState.viewmodel = from->client.viewmodel;
	m_playerState.nextAttack = from->client.m_flNextAttack;

	// it's counterintuitive, but for some things we should take value from "to" state
	// because player movement prediction spits out calculations results in there
	// see CL_FinishPMove engine function to see list of such things.
	m_playerState.velocity = to->client.velocity;
	m_playerState.viewOffset = to->client.view_ofs;
	m_playerState.origin = to->playerstate.origin;
	m_playerState.punchAngle = to->client.punchangle;
}

void CWeaponPredictingContext::WritePlayerState(local_state_t *to)
{
	// here we need to write back values that potentially could be modified from weapons code
	to->client.viewmodel				= m_playerState.viewmodel;
	to->client.fov						= m_playerState.fov;
	to->client.weaponanim				= m_playerState.weaponanim;
	to->client.m_flNextAttack			= m_playerState.nextAttack;
	to->client.maxspeed					= m_playerState.maxSpeed;
	to->client.velocity					= m_playerState.velocity;
	to->client.punchangle				= m_playerState.punchAngle;
}

void CWeaponPredictingContext::UpdatePlayerTimers(const usercmd_t *cmd)
{
	m_playerState.nextAttack -= cmd->msec / 1000.0;
	if (m_playerState.nextAttack < -0.001)
		m_playerState.nextAttack = -0.001;
}

void CWeaponPredictingContext::UpdateWeaponTimers(CBaseWeaponContext *weapon, const usercmd_t *cmd)
{
	weapon->m_flNextPrimaryAttack		-= cmd->msec / 1000.0;
	weapon->m_flNextSecondaryAttack		-= cmd->msec / 1000.0;
	weapon->m_flTimeWeaponIdle			-= cmd->msec / 1000.0;

	if (weapon->m_flNextPrimaryAttack < -1.0)
		weapon->m_flNextPrimaryAttack = -1.0;

	if (weapon->m_flNextSecondaryAttack < -0.001)
		weapon->m_flNextSecondaryAttack = -0.001;

	if (weapon->m_flTimeWeaponIdle < -0.001)
		weapon->m_flTimeWeaponIdle = -0.001;

	if (weapon->m_iId == WEAPON_EGON)
	{
		CEgonWeaponContext *ctx = weapon->As<CEgonWeaponContext>();
		ctx->m_flAttackCooldown -= cmd->msec / 1000.0;
		if (ctx->m_flAttackCooldown < -0.001)
			ctx->m_flAttackCooldown = -0.001;
	}
	else if (weapon->m_iId == WEAPON_GAUSS)
	{
		CGaussWeaponContext *ctx = weapon->As<CGaussWeaponContext>();
		ctx->m_flNextAmmoBurn -= cmd->msec / 1000.0;
		if (ctx->m_flNextAmmoBurn < -0.001)
			ctx->m_flNextAmmoBurn = -0.001;

		ctx->m_flAmmoStartCharge -= cmd->msec / 1000.0;
		if (ctx->m_flAmmoStartCharge < -0.001)
			ctx->m_flAmmoStartCharge = -0.001;
	}
}

void CWeaponPredictingContext::ReadWeaponsState(const local_state_t *from)
{
	for (size_t i = 0; i < MAX_LOCAL_WEAPONS; i++)
	{
		CBaseWeaponContext *weapon = GetWeaponContext(i);
		const weapon_data_t &data = from->weapondata[i];
		if (weapon)
		{
			weapon->m_fInReload				= data.m_fInReload;
			weapon->m_fInSpecialReload		= data.m_fInSpecialReload;
			weapon->m_iClip					= data.m_iClip;
			weapon->m_flNextPrimaryAttack	= data.m_flNextPrimaryAttack;
			weapon->m_flNextSecondaryAttack	= data.m_flNextSecondaryAttack;
			weapon->m_flTimeWeaponIdle		= data.m_flTimeWeaponIdle;

            // Only ACTIVE weapon ammo types are encoded in clientdata_t.
            // Never overwrite reserve/ammo identity for every carried gun.
            if (weapon->m_iId == from->client.m_iId) {
                const int primary = static_cast<int>(from->client.vuser4.x);
                const int secondary = static_cast<int>(from->client.vuser3.z);
                if (primary > 0 && primary < MAX_AMMO_SLOTS) {
                    weapon->m_iPrimaryAmmoType = primary;
                    m_playerState.ammo[primary] = static_cast<int>(from->client.vuser4.y);
                }
                if (secondary > 0 && secondary < MAX_AMMO_SLOTS) {
                    weapon->m_iSecondaryAmmoType = secondary;
                    m_playerState.ammo[secondary] = static_cast<int>(from->client.vuser4.z);
                }
            }

			ReadWeaponSpecificData(weapon, from);
		}
	}
}

void CWeaponPredictingContext::WriteWeaponsState(local_state_t *to, const usercmd_t *cmd)
{
	for (size_t i = 0; i < MAX_LOCAL_WEAPONS; i++)
	{
		weapon_data_t &data = to->weapondata[i];
		CBaseWeaponContext *weapon = GetWeaponContext(i);
		if (!weapon)
		{
			std::memset(&data, 0x0, sizeof(weapon_data_t));
			continue;
		}
		else
		{
			UpdateWeaponTimers(weapon, cmd);
			data.m_fInReload				= weapon->m_fInReload;
			data.m_fInSpecialReload			= weapon->m_fInSpecialReload;
			data.m_iClip					= weapon->m_iClip; 
			data.m_flNextPrimaryAttack		= weapon->m_flNextPrimaryAttack;
			data.m_flNextSecondaryAttack	= weapon->m_flNextSecondaryAttack;
			data.m_flTimeWeaponIdle			= weapon->m_flTimeWeaponIdle;
			WriteWeaponSpecificData(weapon, to);
		}
	}
}

// don't forget to check that according information are being sent in UpdateClientData/GetWeaponData within server/client.cpp 
void CWeaponPredictingContext::ReadWeaponSpecificData(CBaseWeaponContext *weapon, const local_state_t *from)
{
	const weapon_data_t &data = from->weapondata[weapon->m_iId];
	if (weapon->m_iId == WEAPON_RPG)
	{
		CRpgWeaponContext *ctx = weapon->As<CRpgWeaponContext>();
		ctx->m_fSpotActive = static_cast<int>(from->client.vuser2.y);
		ctx->m_cActiveRockets = static_cast<int>(from->client.vuser2.z);
	}
	else if (weapon->m_iId == WEAPON_SATCHEL)
	{
		CSatchelWeaponContext *ctx = weapon->As<CSatchelWeaponContext>();
		ctx->m_chargeReady = data.iuser1;
	}
	else if (weapon->m_iId == WEAPON_HANDGRENADE)
	{
		CHandGrenadeWeaponContext *ctx = weapon->As<CHandGrenadeWeaponContext>();
		ctx->m_flStartThrow = data.fuser1;
		ctx->m_flReleaseThrow = data.fuser2;
	}
	else if (weapon->m_iId == WEAPON_EGON)
	{
		CEgonWeaponContext *ctx = weapon->As<CEgonWeaponContext>();
		ctx->m_flAttackCooldown = data.fuser1;
	}
	else if (weapon->m_iId == WEAPON_GAUSS)
	{
		CGaussWeaponContext *ctx = weapon->As<CGaussWeaponContext>();
		ctx->m_flAmmoStartCharge = data.fuser1;
		ctx->m_flNextAmmoBurn = data.fuser2;
		ctx->m_fInAttack = data.iuser1;
	}
    else if (modular::SharedRegistry().FindWeaponByRuntimeId(weapon->m_iId))
    {
        auto *ctx = static_cast<CModularWeaponContext*>(weapon);
        ctx->m_modularFlags = data.iuser1;
        ctx->m_burstRemaining = data.iuser2;
        ctx->m_loadedCartridgeIndex = data.iuser3;
        ctx->m_selectedCartridgeIndex = data.iuser4;
    }
}

void CWeaponPredictingContext::WriteWeaponSpecificData(CBaseWeaponContext *weapon, local_state_t *to)
{
	weapon_data_t &data = to->weapondata[weapon->m_iId];
	if (weapon->m_iId == WEAPON_RPG)
	{
		CRpgWeaponContext *ctx = weapon->As<CRpgWeaponContext>();
		to->client.vuser2.y = ctx->m_fSpotActive;
		to->client.vuser2.z = ctx->m_cActiveRockets;
	}
	else if (weapon->m_iId == WEAPON_SATCHEL)
	{
		CSatchelWeaponContext *ctx = weapon->As<CSatchelWeaponContext>();
		data.iuser1 = ctx->m_chargeReady;
	}
	else if (weapon->m_iId == WEAPON_HANDGRENADE)
	{
		CHandGrenadeWeaponContext *ctx = weapon->As<CHandGrenadeWeaponContext>();
		data.fuser1 = ctx->m_flStartThrow;
		data.fuser2 = ctx->m_flReleaseThrow;
	}
	else if (weapon->m_iId == WEAPON_EGON)
	{
		CEgonWeaponContext *ctx = weapon->As<CEgonWeaponContext>();
		data.fuser1 = ctx->m_flAttackCooldown;
	}
	else if (weapon->m_iId == WEAPON_GAUSS)
	{
		CGaussWeaponContext *ctx = weapon->As<CGaussWeaponContext>();
		data.fuser1 = ctx->m_flAmmoStartCharge;
		data.fuser2 = ctx->m_flNextAmmoBurn;
		data.iuser1 = ctx->m_fInAttack;
	}
    else if (modular::SharedRegistry().FindWeaponByRuntimeId(weapon->m_iId))
    {
        auto *ctx = static_cast<CModularWeaponContext*>(weapon);
        data.iuser1 = ctx->m_modularFlags;
        data.iuser2 = ctx->m_burstRemaining;
        data.iuser3 = ctx->m_loadedCartridgeIndex;
        data.iuser4 = ctx->m_selectedCartridgeIndex;
    }
}

void CWeaponPredictingContext::HandlePlayerSpawnDeath(local_state_t *to, CBaseWeaponContext *weapon)
{
	if (to->client.health <= 0 && m_playerState.cached.health > 0) {
		weapon->Holster();
	}
	else if (to->client.health > 0 && m_playerState.cached.health <= 0) {
		weapon->Deploy();
	}
	m_playerState.cached.health = to->client.health;
}

void CWeaponPredictingContext::HandleWeaponSwitch(const local_state_t *from, local_state_t *to, const usercmd_t *cmd, CBaseWeaponContext *weapon)
{
	if (from->weapondata[cmd->weaponselect].m_iId == cmd->weaponselect)
	{
		CBaseWeaponContext *selectedWeapon = GetWeaponContext(cmd->weaponselect);
		if (selectedWeapon && selectedWeapon->m_iId != weapon->m_iId)
		{
			weapon->Holster();
			selectedWeapon->Deploy();
			to->client.m_iId = cmd->weaponselect;
		}
	}
}

CBaseWeaponContext* CWeaponPredictingContext::GetWeaponContext(uint32_t weaponID)
{
	if (m_weaponsState.count(weaponID)) {
		return m_weaponsState[weaponID].get();
	}
	else
	{
        // Generic data-authored weapons use the same context on client/server.
        // The ordered virtual-filesystem manifests must match on both sides.
        EnsureModularDefinitionsLoaded();
        if (const auto* definition = modular::SharedRegistry().FindWeaponByRuntimeId(weaponID)) {
            m_weaponsState[weaponID] = std::make_unique<CModularWeaponContext>(
                std::make_unique<CClientWeaponLayerImpl>(m_playerState), definition);
            ItemInfo itemInfo{};
            m_weaponsState[weaponID]->GetItemInfo(&itemInfo);
            CBaseWeaponContext::ItemInfoArray[weaponID] = itemInfo;
            return m_weaponsState[weaponID].get();
        }
		switch (weaponID)
		{
			case WEAPON_GLOCK:  
				m_weaponsState[weaponID] = std::make_unique<CGlockWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_CROSSBOW:
				m_weaponsState[weaponID] = std::make_unique<CCrossbowWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_PYTHON:
				m_weaponsState[weaponID] = std::make_unique<CPythonWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_MP5:
				m_weaponsState[weaponID] = std::make_unique<CMP5WeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_SHOTGUN:
				m_weaponsState[weaponID] = std::make_unique<CShotgunWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_CROWBAR:
				m_weaponsState[weaponID] = std::make_unique<CCrowbarWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_TRIPMINE:
				m_weaponsState[weaponID] = std::make_unique<CTripmineWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_SNARK:
				m_weaponsState[weaponID] = std::make_unique<CSqueakWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_HORNETGUN:
				m_weaponsState[weaponID] = std::make_unique<CHornetgunWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_HANDGRENADE:
				m_weaponsState[weaponID] = std::make_unique<CHandGrenadeWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_SATCHEL:
				m_weaponsState[weaponID] = std::make_unique<CSatchelWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_RPG:
				m_weaponsState[weaponID] = std::make_unique<CRpgWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_EGON:
				m_weaponsState[weaponID] = std::make_unique<CEgonWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_GAUSS:
				m_weaponsState[weaponID] = std::make_unique<CGaussWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			default: 
				return nullptr;
		}

		ItemInfo itemInfo;
		m_weaponsState[weaponID]->GetItemInfo(&itemInfo);
		CBaseWeaponContext::ItemInfoArray[weaponID] = itemInfo;
		return m_weaponsState[weaponID].get();
	}
}
