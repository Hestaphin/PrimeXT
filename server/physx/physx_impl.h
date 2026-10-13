/*
physx_impl.h - part of PhysX physics engine implementation
Copyright (C) 2012 Uncle Mike
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#pragma once
#include "physic.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "io_streams.h"
#include "error_stream.h"
#include "assert_handler.h"
#include "clipfile.h"
#include "filesystem_utils.h"
#include "physics_asset.h"
#include "physics_asset_contact_filter.h"
#include "physics_part_layout.h"
#include "physics_hinge_frame.h"
#include "meshdesc.h"
#include "performance_cache.h"

#include <PxPhysicsAPI.h>
#include <PxSimulationEventCallback.h>
#include <PxScene.h>
#include <PxActor.h>
#include <PxTriangleMeshDesc.h>
#include <PxConvexMeshDesc.h>
#include <PxMaterial.h>
#include <PxCooking.h>
#include <map>
#include <string>
#include <PxTriangle.h>
#include <vector>
#include <memory>
#include <cstdint>
	
class DebugRenderer;
class EventHandler;
class ContactModifyCallback;
class HoldableItemController;

class CPhysicPhysX : public IPhysicLayer
{
public:
	CPhysicPhysX();
	void	InitPhysic( void );
	void	FreePhysic( void );
	void	Update( float flTimeDelta );
	void	EndFrame( void );
	void	RemoveBody( edict_t *pEdict );
	void	*CreateBodyFromEntity( CBaseEntity *pEntity );
	void	*CreateBoxFromEntity( CBaseEntity *pObject );
	void	*CreateKinematicBodyFromEntity( CBaseEntity *pEntity );
	void	*CreateStaticBodyFromEntity( CBaseEntity *pObject );
	void	*CreateVehicle( CBaseEntity *pObject, string_t scriptName = 0 );
	void	*CreateTriggerFromEntity( CBaseEntity *pEntity );
	void	*RestoreBody( CBaseEntity *pEntity );
	void	SaveBody( CBaseEntity *pObject );
    bool CaptureRagdollState(CBaseEntity *, PhysicsRagdollSaveState &,
        PhysicsRagdollBodySaveRecord *, int) override;
    bool RestoreRagdollState(CBaseEntity *, const PhysicsRagdollSaveState &,
        const PhysicsRagdollBodySaveRecord *, int) override;
	bool	Initialized( void ) { return (m_pPhysics != NULL); }
	void	SetOrigin( CBaseEntity *pEntity, const Vector &origin );
	void	SetAngles( CBaseEntity *pEntity, const Vector &angles );
	void	SetVelocity( CBaseEntity *pEntity, const Vector &velocity );
	void	SetAvelocity( CBaseEntity *pEntity, const Vector &velocity );
	void	MoveObject( CBaseEntity *pEntity, const Vector &finalPos );
	void	RotateObject( CBaseEntity *pEntity, const Vector &finalAngle );
	void	SetLinearMomentum( CBaseEntity *pEntity, const Vector &momentum );
	void	AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor );
	void	AddForce( CBaseEntity *pEntity, const Vector &force, ForceMode mode = ForceMode::Force );
	void	AddTorque( CBaseEntity *pEntity, const Vector &torque, ForceMode mode = ForceMode::Force );
	void	SetHoldableTarget( CBaseEntity *pEntity, const Vector &targetOrigin, const Vector4D &targetQuat ) override;
	void	ClearHoldableTarget( CBaseEntity *pEntity ) override;
	void	GetTransform( CBaseEntity *pEntity, matrix4x4 &out );
	void	EnableCollision( CBaseEntity *pEntity, int fEnable );
	void	MakeKinematic( CBaseEntity *pEntity, int fEnable );
	void	UpdateVehicle( CBaseEntity *pObject );
	int		FLoadTree( char *szMapName );
	int		CheckBINFile( char *szMapName );
	int		BuildCollisionTree( char *szMapName );
	bool	UpdateEntityTransform( CBaseEntity *pEntity );
	void	UpdateEntityAABB( CBaseEntity *pEntity );
	void	UpdateCollisionBodygroup( CBaseEntity *pEntity );
	bool	ResolveRenderImpact( CBaseEntity *pEntity, const Vector &start, const Vector &end, Vector &impactPoint, Vector &impactNormal, std::uint32_t &materialHash ) override;
	bool	UpdateActorPos( CBaseEntity *pEntity );
	void	SetupWorld( void );	
	void	FreeWorld( void );
	void	DebugDraw( void );
	void	DrawPSpeeds( void );

	void	TeleportCharacter( CBaseEntity *pEntity );
	void	TeleportActor( CBaseEntity *pEntity );
	void	MoveCharacter( CBaseEntity *pEntity );
	void	MoveKinematic( CBaseEntity *pEntity );
	void	SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, struct trace_s *tr );
	void	SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, struct gametrace_s *tr );
	bool	IsBodySleeping( CBaseEntity *pEntity );
	void	*GetCookingInterface( void ) { return m_pCooking; }
	void	*GetPhysicInterface( void ) { return m_pPhysics; }
	bool	CreatePhysicsAsset( CBaseEntity *pEntity ) override;
	bool	HasPhysicsAsset( CBaseEntity *pEntity ) override;
	void	UpdatePhysicsAssetPose( CBaseEntity *pEntity ) override;
    bool SetPhysicsPartMode( CBaseEntity *entity, const char *partName, PhysicsPartMode mode ) override;
    bool GetPhysicsPartWorldPose( CBaseEntity *entity, const char *partName, matrix4x4 &pose ) override;
    bool DamagePhysicsPart( CBaseEntity *entity, const char *partName, float damage ) override;
    int CaptureInteractiveParts(CBaseEntity *entity, PhysicsPartSaveRecord *out, int capacity) override;
    void RestoreInteractiveParts(CBaseEntity *entity, const PhysicsPartSaveRecord *records, int count) override;
	bool GetRagdollWorldBounds( CBaseEntity *pEntity, Vector &mins, Vector &maxs ) override;
	bool	IsRagdoll( CBaseEntity *pEntity ) override;
	void	SetRagdoll( CBaseEntity *pEntity, bool enabled ) override;
	bool SetRagdollBodyPhysicsWeight( CBaseEntity *pEntity, const char *bodyName, float physicsWeight, bool includeChildren ) override;
	bool IsRagdollFrozen( CBaseEntity *pEntity ) override;
	bool BuildRagdollDecalAttachment( CBaseEntity *pEntity, const Vector &worldPosition, const Vector &worldNormal, int preferredHitGroup, int &boneIndex, Vector &localPosition, Vector &localNormal ) override;
	CollisionRepresentation ResolveCollisionRepresentation( CBaseEntity *pEntity, CollisionQueryPurpose purpose ) override;
	bool TraceBulletPhysicsParts( const Vector &start, const Vector &end,
		edict_t *ignore, TraceResult *ioTrace ) override;
	bool TraceBulletRagdolls( const Vector &start, const Vector &end, edict_t *ignore, TraceResult *ioTrace, int *bodyIndex ) override;
	bool TraceBallisticPhysicsExit( CBaseEntity *target, int ragdollBodyIndex,
		const Vector &entry, const Vector &forward, float maxDepth, Vector &exit, Vector &normal ) override;
	bool ApplyPhysicsPartBulletImpact( CBaseEntity *entity, const Vector &start, const Vector &end,
		const Vector &legacyContact, const Vector &impulse, float damage ) override;
	void AddPhysicsPartRadialImpulse( CBaseEntity *entity, const Vector &origin,
		float radius, float impulse, float damage ) override;
	bool AddPhysicsAssetImpulse( CBaseEntity *pEntity, int bodyIndex, const Vector &impulse, const Vector &position ) override;
	void QueuePhysicsAssetImpulse( CBaseEntity *pEntity, int hitGroup, const Vector &impulse, const Vector &position ) override;
	void AddPhysicsAssetRadialImpulse( CBaseEntity *pEntity, const Vector &origin, float radius, float impulse ) override;

private:
    physicsasset::AssetContactFilter m_assetContactFilter;
	struct PhysicsAssetCollisionState;
	struct PhysicsPartRuntime;

	// misc routines
	bool DebugEnabled() const;
	bool TracingStateChanges(physx::PxActor *actor) const;
	void HandleEvents();
	int	ConvertEdgeToIndex( model_t *model, int edge );
	physx::PxConvexMesh	*ConvexMeshFromBmodel( entvars_t *pev, int modelindex );
	physx::PxConvexMesh	*ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	physx::PxConvexMesh	*ConvexMeshFromEntity( CBaseEntity *pObject );
	physx::PxConvexMesh *ConvexMeshFromPhysicsAsset( CBaseEntity *pObject, const PhysicsAssetDef &asset );
	physx::PxTriangleMesh *TriangleMeshFromBmodel( entvars_t *pev, int modelindex );
	physx::PxTriangleMesh *TriangleMeshFromStudio( model_t *model, int32_t body, int32_t skin );
	physx::PxTriangleMesh *TriangleMeshFromEntity( CBaseEntity *pObject );
	physx::PxTriangleMesh *TriangleMeshFromPhysicsAsset( CBaseEntity *pObject, const PhysicsAssetDef &asset );
	physx::PxTriangleMesh *TriangleMeshFromPhysicsAssetSection( const PhysicsAssetDef &asset, size_t collisionIndex );
	physx::PxConvexMesh *ConvexMeshFromPhysicsAssetSection( const PhysicsAssetDef &asset, size_t collisionIndex );
	bool HasPhysicsAssetCollision( CBaseEntity *pObject );
	void TrackPhysicsAssetCollision( CBaseEntity *pObject );
	void ReleaseAnimatedCollisionActors( PhysicsAssetCollisionState &state );
	void BuildAnimatedCollisionActors( CBaseEntity *pObject, PhysicsAssetCollisionState &state, const PhysicsAssetDef &asset );
    void BuildInteractiveHingeJoints( CBaseEntity *entity, PhysicsAssetCollisionState &state, const PhysicsAssetDef &asset );
	void UpdateAnimatedCollisionActors();
	void RebuildAnimatedCollisionTraceMesh( CBaseEntity *pObject, PhysicsAssetCollisionState &state, const PhysicsAssetDef &asset );
    void BuildAnimatedTraceKey(CBaseEntity *entity, const PhysicsAssetCollisionState &state,
        const PhysicsAssetDef &asset, performance::TraceMeshKey &key);
	void SetAnimatedCollisionEnabled( PhysicsAssetCollisionState &state, bool enabled );
    void ApplyPhysicsAssetRootCollision( CBaseEntity *entity, PhysicsAssetCollisionState &state, bool enableWhenAllowed );
	void SetArticulatedCollisionOwnership( CBaseEntity *pEntity, bool enabled );
    PhysicsPartRuntime *FindPhysicsPart( CBaseEntity *pEntity, const char *partName );
    void SampleInteractivePart( CBaseEntity *entity, PhysicsPartRuntime &part );
    void ApplyInteractivePartDrives( float dt );
    bool ApplyPhysicsPartDamageEvent( CBaseEntity *entity, PhysicsPartRuntime &part, float damage, float impulseMagnitude );
    bool DetachPhysicsPart( CBaseEntity *entity, PhysicsPartRuntime &part );
	bool SetPhysicsPartOwnership( CBaseEntity *pEntity, const char *partName, physicsasset::PartOwner owner );
	void UpdatePhysicsAssetCollisionBodygroups();
	physx::PxActor *ActorFromEntity( CBaseEntity *pObject );
	physx::PxBounds3 GetIntersectionBounds( const physx::PxBounds3 &a, const physx::PxBounds3 &b );
	CBaseEntity	*EntityFromActor( physx::PxActor *pObject );
	bool CheckCollision(physx::PxRigidActor *pActor);
	void ToggleCollision(physx::PxRigidActor *pActor, bool enabled);
	void UpdateCharacterBounds( CBaseEntity *pEntity, physx::PxShape *pShape );
	int	CheckFileTimes( const char *szFile1, const char *szFile2 );
	void StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q );
	void StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos );
	bool P_SpeedsMessage( char *out, size_t size );
	void CacheNameForModel( model_t *model, fs::Path &hullfile, uint32_t hash, const char *suffix );
	uint32_t GetHashForModelState( model_t *model, int32_t body, int32_t skin );
	clipfile::GeometryType ShapeTypeToGeomType(physx::PxGeometryType::Enum geomType);
	PhysicsAssetInstance *FindPhysicsAsset( CBaseEntity *pEntity );
	PhysicsAssetInstance *EnsurePhysicsAssetInstance( CBaseEntity *pEntity );
	void SuspendLegacyCollisionForRagdoll( CBaseEntity *pEntity, physx::PxRigidActor *actor );
	void RestoreLegacyCollisionAfterRagdoll( CBaseEntity *pEntity );
	void SuspendEntityForRagdoll( CBaseEntity *pEntity, PhysicsAssetInstance *asset );
	void RestoreEntityAfterRagdoll( CBaseEntity *pEntity );
	void UpdateRagdollPresentation( PhysicsAssetInstance *asset );
	void SendRagdollPose( PhysicsAssetInstance *asset );
	void ClearRagdollPose( CBaseEntity *pEntity );
    void SendInteractivePartPoses();
	std::shared_ptr<PhysicsAssetDef> GetPhysicsAssetDefinition( CBaseEntity *pEntity );
	void ClearWorldAssetCaches();

private:
	// The authoritative actor for a moving bone owns every collider section
	// on that bone. A future hinge takes over this same part; it must never
	// spawn a second simultaneous collision proxy for the same door.
	struct PhysicsPartShape
	{
		size_t collisionIndex = 0;
		physx::PxShape *shape = nullptr;
	};
	struct PhysicsPartRuntime
	{
		std::string name;
		int bodyIndex = -1; // authored .phys body, or -1 for legacy v3 proxy
		int boneIndex = -1;
		physicsasset::PropDriveMode initialMode = physicsasset::PropDriveMode::Animated;
		physicsasset::PartOwner owner = physicsasset::PartOwner::AnimatedProxy;
        physx::PxRigidDynamic *actor = nullptr;
        bool dynamicCompatible = false;
        bool ccd = false; // resolved profile retained across kinematic handoff
        bool hasHinge = false;
        bool detached = false;
        float accumulatedDamage = 0.0f;
        float releaseDamage = 0.0f;
        float breakDamage = 0.0f;
        float breakImpulse = 0.0f;
        PhysicsPartMode mode = PhysicsPartMode::Animated;
        physx::PxRevoluteJoint *hinge = nullptr;
        float driveStrength = 0.0f;
        float driveDamping = 0.0f;
        float driveMaxForce = 0.0f;
        float lastTargetAngle = 0.0f;
        float targetAngularVelocity = 0.0f;
        float targetSampleTime = 0.0f;
        bool hasTargetAngle = false;
        bool sampledPose = false;
        physx::PxTransform previousAnimatedPose = physx::PxTransform(physx::PxIdentity);
        physx::PxTransform latestAnimatedPose = physx::PxTransform(physx::PxIdentity);
        float previousSampleTime = 0.0f;
        float latestSampleTime = 0.0f;
        float partNetNextSend = 0.0f;
        bool partNetActive = false;
        bool partNetDetached = false;
        std::vector<PhysicsPartShape> shapes;
	};

	struct PhysicsAssetCollisionState
	{
		edict_t *edict = nullptr;
		model_t *renderModel = nullptr;
		int32_t renderBody = 0;
		bool selectionHasGeometry = true;
		bool collisionRequested = true;
		// Activated full ragdolls take ownership of their bone collisions.
		// Do not let animated prop proxies overlap the articulated actors.
		bool articulatedOverride = false;
		std::vector<PhysicsPartRuntime> parts;
        std::vector<physx::PxRevoluteJoint *> hinges;
		std::unique_ptr<CMeshDesc> animatedTraceMesh;
        performance::TraceMeshKey traceKey, traceCandidate;
	};

	struct LegacyShapeCollisionState
	{
		bool simulation = false;
		bool sceneQuery = false;
	};
	struct RagdollLegacyCollisionState
	{
		physx::PxRigidActor *actor = nullptr;
        int32_t renderBodyAtSuspend = 0;
        bool collisionRequestedAtSuspend = true;
		std::vector<LegacyShapeCollisionState> shapes;
	};
	struct RagdollEntityState
	{
		int solid = SOLID_NOT;
		int movetype = MOVETYPE_NONE;
		Vector mins;
		Vector maxs;
		Vector angles;
		matrix4x4 rootToEntity;
		bool hasRootRelation = false;
	};

	struct PendingRagdollImpulse
	{
		edict_t *edict = nullptr;
		Vector impulse;
		Vector position;
		int hitGroup = 0;
		float time = 0.0f;
	};
	struct RagdollNetworkState
	{
		float nextSendTime = 0.0f;
		float lastSendTime = 0.0f;
		bool initialized = false;
		bool sleeping = false;
		bool frozen = false;
	};

	std::map<int, PhysicsAssetCollisionState> m_physicsAssetCollisionStates;
	std::map<int, RagdollLegacyCollisionState> m_ragdollLegacyCollisionState;
	std::map<int, RagdollEntityState> m_ragdollEntityState;
	std::map<int, RagdollNetworkState> m_ragdollNetworkState;
	std::map<int, std::vector<PendingRagdollImpulse>> m_pendingRagdollImpulses;
	void TransferPendingRagdollImpulses( CBaseEntity *entity, PhysicsAssetInstance *asset );
	std::uint32_t m_ragdollSnapshotSequence = 0;
    std::uint32_t m_partSnapshotSequence = 0;
	std::map<std::string, physx::PxTriangleMesh *> m_triangleMeshCache;
	std::map<std::string, physx::PxConvexMesh *> m_physicsAssetConvexMeshCache;
	// Cache both successful and failed .phys lookups for the map lifetime. A null
	// entry means the sibling asset was missing/invalid and avoids filesystem
	// probes every time HasPhysicsAsset() is queried.
	std::map<std::string, std::shared_ptr<PhysicsAssetDef>> m_physicsAssetDefinitions;

	physx::PxPhysics *m_pPhysics;	// pointer to the PhysX engine
	physx::PxFoundation	*m_pFoundation;
	physx::PxDefaultCpuDispatcher *m_pDispatcher;
	physx::PxScene *m_pScene;	// pointer to world scene
	model_t	*m_pWorldModel;	// pointer to worldmodel

	char m_szMapName[256];
	bool m_fLoaded;	// collision tree is loaded and actual
	bool m_fDisableWarning;	// some warnings will be swallowed
	bool m_fWorldChanged;	// world is changed refresh the statics in case their scale was changed too
	bool m_traceStateChanges;
	double m_flAccumulator;
    struct PerformanceState
    {
        std::uint64_t meshBuilds = 0, meshHits = 0, shapeChanges = 0;
        unsigned steps = 0;
        double frameMilliseconds = 0, simulateMilliseconds = 0, fetchMilliseconds = 0;
        double meshMilliseconds = 0, poseMilliseconds = 0;
    } m_performance;

	physx::PxTriangleMesh *m_pSceneMesh;
	physx::PxActor *m_pSceneActor;	// scene with installed shape
	physx::PxBounds3 m_worldBounds;
	physx::PxMaterial *m_pDefaultMaterial;
	physx::PxMaterial *m_pConveyorMaterial;

	char p_speeds_msg[1024];	// debug message

	AssertHandler m_assertHandler;
	ErrorCallback m_errorCallback;
	std::unique_ptr<DebugRenderer> m_debugRenderer;
	std::unique_ptr<EventHandler> m_eventHandler;
	std::unique_ptr<ContactModifyCallback> m_contactModifyCallback;
	std::unique_ptr<HoldableItemController> m_holdableController;
	physx::PxCooking *m_pCooking;
	physx::PxDefaultAllocator m_Allocator;
	physx::PxPvd *m_pVisualDebugger;
	std::vector<std::unique_ptr<PhysicsAssetInstance>> m_physicsAssets;
};
