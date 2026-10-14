/*
physic.h - an abstract class for physics subsystem
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifndef PHYSIC_H
#define PHYSIC_H

#include <cstdint>

class Vector;
class Vector4D;
class CBaseEntity;
class matrix4x4;

// I.8 savegame snapshot; independent of PhysX SDK. Bounded per-entity storage.
// Entries are matched by stable .phys part name, never by a transient actor ID.
constexpr int kSavedPhysicsPartLimit = 8;
struct PhysicsPartSaveRecord
{
    char name[32] = {};
    int mode = 0;
    int detached = 0;
    float damage = 0;
    float pose[7] = {};
    float linear[3] = {};
    float angular[3] = {};
};
static_assert(sizeof(PhysicsPartSaveRecord)==96,
    "Physics part savegame record layout unexpectedly changed");

// I.9.5: whole-articulation save format, independent of PhysX headers.
// Serialized as raw characters by the existing GoldSrc save system.
// An over-capacity or mismatching asset restores to the pre-ragdoll entity
// state instead of leaving a non-solid ghost with no articulated actors.
constexpr int kSavedRagdollBodyLimit = 64;
struct PhysicsRagdollBodySaveRecord
{
    char name[32] = {};
    float pose[7] = {};  // world actor position and quaternion
    float linear[3] = {};
    float angular[3] = {};
    float weight = 1.0f;
    int sleeping = 0;
};
struct PhysicsRagdollSaveState
{
    int active = 0;
    int valid = 0;
    int full = 0;
    int frozen = 0;
    int count = 0;
    int oldSolid = 0;
    int oldMovetype = 0;
    int hasRootRelation = 0;
    float oldMins[3] = {};
    float oldMaxs[3] = {};
    float oldAngles[3] = {};
    float savedEntityOrigin[3] = {}; // correct world poses for savegame landmark offsets
    float rootToEntity[16] = {};
    char model[128] = {};
};

class IPhysicLayer
{
public:
    enum class PhysicsPartMode { Animated, Driven, Free };

	enum class CollisionQueryPurpose
	{
		Movement,
		BulletDamage,
		PrecisionQuery,
		PhysicsSimulation,
	};

	enum class CollisionRepresentation
	{
		Legacy,
		Hitboxes,
		PhysicsAsset,
	};

	enum class ForceMode
	{
		Force,			//!< parameter is a force (N) or torque (Nm) — depends on mass/inertia
		Impulse,		//!< parameter is an impulse (N·s) or angular impulse (Nm·s) — mass/inertia dependent
		VelocityChange,	//!< parameter is a velocity change (distance/time) or angular velocity change (rad/s) — mass/inertia independent
		Acceleration	//!< parameter is an acceleration (distance/time²) or angular acceleration (rad/s²) — mass/inertia independent
	};

	virtual void	InitPhysic( void ) = 0;
	virtual void	FreePhysic( void ) = 0;
	virtual void	Update( float flTime ) = 0;
	virtual void	EndFrame( void ) = 0;
	virtual bool	Initialized( void ) = 0;
	virtual void	RemoveBody( struct edict_s *pEdict ) = 0;
	virtual void	*CreateBodyFromEntity( CBaseEntity *pEntity ) = 0;
	virtual void	*CreateBoxFromEntity( CBaseEntity *pObject ) = 0;
	virtual void	*CreateKinematicBodyFromEntity( CBaseEntity *pEntity ) = 0;
	virtual void	*CreateStaticBodyFromEntity( CBaseEntity *pObject ) = 0;
	virtual void	*CreateVehicle( CBaseEntity *pObject, int scriptName = 0 ) = 0;
	virtual void	*CreateTriggerFromEntity( CBaseEntity *pEntity ) = 0;
	virtual void	*RestoreBody( CBaseEntity *pEntity ) = 0;
	virtual void	SaveBody( CBaseEntity *pObject ) = 0;
    virtual bool CaptureRagdollState( CBaseEntity *, PhysicsRagdollSaveState &,
        PhysicsRagdollBodySaveRecord *, int ) { return false; }
    virtual bool RestoreRagdollState( CBaseEntity *, const PhysicsRagdollSaveState &,
        const PhysicsRagdollBodySaveRecord *, int ) { return false; }

	virtual void	SetOrigin( CBaseEntity *pEntity, const Vector &origin ) = 0;
	virtual void	SetAngles( CBaseEntity *pEntity, const Vector &angles ) = 0;
	virtual void	SetVelocity( CBaseEntity *pEntity, const Vector &velocity ) = 0;
	virtual void	SetAvelocity( CBaseEntity *pEntity, const Vector &velocity ) = 0;
	virtual void	MoveObject( CBaseEntity *pEntity, const Vector &finalPos ) = 0;
	virtual void	RotateObject( CBaseEntity *pEntity, const Vector &finalAngle ) = 0;
	virtual void	SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity ) = 0;
	virtual void	AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor ) = 0;
	// PhysX integrates gameplay Force/Acceleration inputs over the current
	// server frame's accepted duration. Impulse/VelocityChange are one-shot.
	virtual void	AddForce( CBaseEntity *pEntity, const Vector &force, ForceMode mode = ForceMode::Force ) = 0;
	virtual void	AddTorque( CBaseEntity *pEntity, const Vector &torque, ForceMode mode = ForceMode::Force ) = 0;
	virtual void	SetHoldableTarget( CBaseEntity *pEntity, const Vector &targetOrigin, const Vector4D &targetQuat ) = 0;
	virtual void	ClearHoldableTarget( CBaseEntity *pEntity ) = 0;
	virtual void	GetTransform( CBaseEntity *pEntity, matrix4x4 &out ) = 0;
	virtual void	EnableCollision( CBaseEntity *pEntity, int fEnable ) = 0;
	virtual void	MakeKinematic( CBaseEntity *pEntity, int fEnable ) = 0;
	virtual void	UpdateVehicle( CBaseEntity *pObject ) = 0;
	virtual int		FLoadTree( char *szMapName ) = 0;
	virtual int		CheckBINFile( char *szMapName ) = 0;
	virtual int		BuildCollisionTree( char *szMapName ) = 0;
	virtual bool	UpdateEntityTransform( CBaseEntity *pEntity ) = 0;
	virtual void	UpdateEntityAABB( CBaseEntity *pEntity ) = 0;
	virtual void	UpdateCollisionBodygroup( CBaseEntity *pEntity ) = 0;
	virtual bool	ResolveRenderImpact( CBaseEntity *pEntity, const Vector &start, const Vector &end, Vector &impactPoint, Vector &impactNormal, std::uint32_t &materialHash ) = 0;
	virtual bool	UpdateActorPos( CBaseEntity *pEntity ) = 0;
	virtual void	SetupWorld( void ) = 0;	
	virtual void	FreeWorld( void ) = 0;
	virtual void	DebugDraw( void ) = 0;
	virtual void	DrawPSpeeds( void ) = 0;
	virtual void	TeleportCharacter( CBaseEntity *pEntity ) = 0;
	virtual void	TeleportActor( CBaseEntity *pEntity ) = 0;
	virtual void	MoveCharacter( CBaseEntity *pEntity ) = 0;
	virtual void	MoveKinematic( CBaseEntity *pEntity ) = 0;
	virtual void	SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, struct trace_s *tr ) = 0;
	virtual void	SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, struct gametrace_s *tr ) = 0;
	virtual bool	IsBodySleeping( CBaseEntity *pEntity ) = 0;
	virtual void	*GetCookingInterface( void ) = 0;
	virtual void	*GetPhysicInterface( void ) = 0;
	virtual bool	CreatePhysicsAsset( CBaseEntity *pEntity ) = 0;
	virtual bool	HasPhysicsAsset( CBaseEntity *pEntity ) = 0;
	virtual void	UpdatePhysicsAssetPose( CBaseEntity *pEntity ) = 0;
    // Server-owned per-part transition; no automatic damage trigger in I.4.
    virtual bool SetPhysicsPartMode( CBaseEntity *entity, const char *partName, PhysicsPartMode mode ) = 0;
    virtual bool GetPhysicsPartWorldPose( CBaseEntity *entity, const char *partName, matrix4x4 &pose ) = 0;
    // Gameplay/script damage to a named articulated part, separate from owner HP.
    virtual bool DamagePhysicsPart( CBaseEntity *entity, const char *partName, float damage ) = 0;
    virtual int CaptureInteractiveParts(CBaseEntity *entity, PhysicsPartSaveRecord *out, int capacity) = 0;
    virtual void RestoreInteractiveParts(CBaseEntity *entity, const PhysicsPartSaveRecord *records, int count) = 0;
    // B6: returns the authoritative world-space AABB for full ragdolls.
    // Partial ragdolls retain their normal movement hull instead.
    virtual bool GetRagdollWorldBounds( CBaseEntity *pEntity, Vector &mins, Vector &maxs ) = 0;
	virtual bool	IsRagdoll( CBaseEntity *pEntity ) = 0;
	virtual void	SetRagdoll( CBaseEntity *pEntity, bool enabled ) = 0;
	virtual bool SetRagdollBodyPhysicsWeight( CBaseEntity *pEntity, const char *bodyName, float physicsWeight, bool includeChildren ) = 0;
	virtual bool IsRagdollFrozen( CBaseEntity *pEntity ) = 0;
	virtual bool BuildRagdollDecalAttachment( CBaseEntity *pEntity, const Vector &worldPosition, const Vector &worldNormal, int preferredHitGroup, int &boneIndex, Vector &localPosition, Vector &localNormal ) = 0;
	virtual CollisionRepresentation ResolveCollisionRepresentation( CBaseEntity *pEntity, CollisionQueryPurpose purpose ) = 0;
	// Supplemental raycast for protruding/primitive-only articulated parts.
	// Never overrides a nearer existing world/legacy/ragdoll result.
	virtual bool TraceBulletPhysicsParts( const Vector &start, const Vector &end,
		struct edict_s *ignore, struct gametrace_s *ioTrace ) = 0;
	virtual bool TraceBulletRagdolls( const Vector &start, const Vector &end, struct edict_s *ignore, struct gametrace_s *ioTrace, int *bodyIndex ) = 0;
	// J.9 target-only exit query; exact physics body for ragdolls, exact shape for parts.
	virtual bool TraceBallisticPhysicsExit( CBaseEntity *target, int ragdollBodyIndex,
		const Vector &entry, const Vector &forward, float maxDepth, Vector &exit, Vector &normal ) = 0;
	// I.5: only accept a bullet part after the ordinary engine trace has hit its owner.
	// Return true when an active part collider was struck, even when its authored
	// policy is Animated (no physical reaction).
	virtual bool ApplyPhysicsPartBulletImpact( CBaseEntity *entity, const Vector &start, const Vector &end,
		const Vector &legacyContact, const Vector &impulse, float damage ) = 0;
	virtual void AddPhysicsPartRadialImpulse( CBaseEntity *entity, const Vector &origin,
		float radius, float impulse, float damage ) = 0;
	virtual bool AddPhysicsAssetImpulse( CBaseEntity *pEntity, int bodyIndex, const Vector &impulse, const Vector &position ) = 0;
	virtual void QueuePhysicsAssetImpulse( CBaseEntity *pEntity, int hitGroup, const Vector &impulse, const Vector &position ) = 0;
	virtual void AddPhysicsAssetRadialImpulse( CBaseEntity *pEntity, const Vector &origin, float radius, float impulse ) = 0;
};

extern void GameInitNullPhysics( void ); // shutdown simulation for some reasons
extern IPhysicLayer	*WorldPhysic;

#endif // PHYSIC_H
