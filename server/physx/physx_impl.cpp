/*
physx_impl.cpp - part of PhysX physics engine implementation
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

#ifdef USE_PHYSICS_ENGINE
#include <PxConfig.h>
#include "physx_impl.h"
#include "physics_asset_construction.h"
#include "saverestore.h"
#include "client.h"
#include "bspfile.h"
#include "triangleapi.h"
#include "studio.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "animation.h"
#include "trace.h"
#include "game.h"
#include "user_messages.h"
#include "build.h"
#include "error_stream.h"
#include "event_handler.h"
#include "holdable_item_controller.h"
#include "assert_handler.h"
#include "debug_renderer.h"
#include "contact_modify_callback.h"
#include "collision_filter_data.h"
#include "physics_part_motion.h"
#include "physics_part_drive.h"
#include "physics_part_impact.h"
#include "physics_part_failure.h"
#include "physics_part_restore_policy.h"
#include "physics_asset_collision_ownership.h"
#include "physics_asset_scale.h"
#include "physics_asset_trace_coordinates.h"
#include "physics_asset_primitive_trace.h"
#include "physics_asset_trace_owner.h"
#include "ballistic_shape_selection.h"
#include "physics_asset_cache_lifecycle.h"
#include "ragdoll_net.h"
#include "ragdoll_bullet_policy.h"
#include "ragdoll_restore_i95_policy.h"
#include "physics_part_net.h"
#include "decomposed_shape.h"
#include "meshdesc_factory.h"
#include "crclib.h"
#include <PxMat33.h>
#include <PxMat44.h>
#include <vector>
#include <functional>
#include <algorithm>
#include <thread>
#include <utility>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <fmt/format.h>

#if defined (HAS_PHYSIC_VEHICLE)
#include "NxVehicle.h"
#include "vehicles/NxParser.h"
#endif

constexpr float k_PaddingFactor = 0.49f;
constexpr float k_DefaultDensity = MetricDensityToEngine(900.f); // like wood
constexpr float k_WaterDensity = MetricDensityToEngine(1000.f);
constexpr float k_WaterLinearDragFactor = 500.f;
constexpr float k_WaterAngularDragFactor = 1.0f;
constexpr uint32_t k_SolverIterationCount = 4;
constexpr double k_SimulationStepSize = physicsasset::kSimulationStepSeconds;

using namespace physx;

static std::int16_t QuantizeRagdollRelativePosition( float value, bool &ok )
{
	const float scaled = value * ragdollnet::kRelativePositionScale;
	if( !std::isfinite( scaled ) || scaled < -32767.0f || scaled > 32767.0f )
	{
		ok = false;
		return 0;
	}
	const int rounded = static_cast<int>( std::lround( scaled ) );
	return static_cast<std::int16_t>( std::max( -32767, std::min( 32767, rounded )) );
}

static std::int16_t QuantizeRagdollQuaternion( float value )
{
	const float clamped = std::max( -1.0f, std::min( 1.0f, value ));
	return static_cast<std::int16_t>( std::lround( clamped * ragdollnet::kQuaternionScale ));
}

static matrix4x4 MatrixFromPxTransform( const PxTransform &pose )
{
	const PxMat44 m( pose );
	float values[16] =
	{
		m.column0.x, m.column0.y, m.column0.z, m.column0.w,
		m.column1.x, m.column1.y, m.column1.z, m.column1.w,
		m.column2.x, m.column2.y, m.column2.z, m.column2.w,
		m.column3.x, m.column3.y, m.column3.z, m.column3.w
	};
	return matrix4x4( values );
}

CPhysicPhysX	g_physicPhysX;
IPhysicLayer	*WorldPhysic = &g_physicPhysX;


// Physics-asset v2/v3 collision sections are selected from the visible MDL's
// bodygroups. The render model stays the material/effect authority; .phys only
// owns collision/query geometry.
static mstudiobodyparts_t *FindBodyPartByName( studiohdr_t *header, const char *name )
{
	if( !header || !name || !name[0] )
		return NULL;

	mstudiobodyparts_t *bodyParts = (mstudiobodyparts_t *)((byte *)header + header->bodypartindex);
	for( int i = 0; i < header->numbodyparts; ++i )
	{
		if( !Q_stricmp( bodyParts[i].name, name ))
			return &bodyParts[i];
	}
	return NULL;
}

static int ResolveBodygroupVariant( model_t *renderModel, int32_t renderBody, const char *bodyGroup )
{
	if( !renderModel || renderModel->type != mod_studio || !bodyGroup || !bodyGroup[0] )
		return -1;
	studiohdr_t *header = (studiohdr_t *)renderModel->cache.data;
	mstudiobodyparts_t *part = FindBodyPartByName( header, bodyGroup );
	if( !part || part->nummodels <= 0 || part->base <= 0 )
		return -1;
	return ( renderBody / part->base ) % part->nummodels;
}

static bool CollisionSectionActive( const physicsasset::CollisionDef &collision, model_t *renderModel, int32_t renderBody )
{
	if( collision.bodyGroup.empty() )
		return true;
	return ResolveBodygroupVariant( renderModel, renderBody, collision.bodyGroup.c_str() ) == collision.variant;
}

static int FindStudioBoneIndex( model_t *renderModel, const char *boneName )
{
	if( !renderModel || renderModel->type != mod_studio || !boneName || !boneName[0] )
		return -1;
	studiohdr_t *header = (studiohdr_t *)renderModel->cache.data;
	if( !header || header->numbones <= 0 ) return -1;
	mstudiobone_t *bones = (mstudiobone_t *)((byte *)header + header->boneindex);
	for( int i = 0; i < header->numbones; ++i )
	{
		if( !Q_stricmp( bones[i].name, boneName )) return i;
	}
	return -1;
}

static PxTransform StudioBoneWorldTransform( CBaseEntity *entity, int boneIndex )
{
	Vector origin, angles;
	GET_BONE_POSITION( entity->edict(), boneIndex, origin, angles );
	// PhysX scales the local collider geometry, so the bone pivot must also
	// be scaled in ENTITY local space. The Studio API returns an unscaled pivot.
	const Vector scale = entity->GetScale();
	const matrix4x4 frame( entity->GetAbsOrigin(), entity->GetAbsAngles(), 1.0f );
	Vector local = frame.VectorITransform( origin );
	const auto scaled=physicsasset::ScaleBonePivot({local.x,local.y,local.z},
	    {scale.x,scale.y,scale.z});
	origin = frame.VectorTransform(Vector(scaled.x,scaled.y,scaled.z));
	float mat[16];
	matrix4x4( origin, angles, 1.0f ).CopyToArray( mat );
	return PxTransform( PxMat44( mat ));
}

CPhysicPhysX::CPhysicPhysX() : 
	m_pPhysics(nullptr),
	m_pFoundation(nullptr),
	m_pDispatcher(nullptr),
	m_pScene(nullptr),
	m_pWorldModel(nullptr),
	m_pSceneMesh(nullptr),
	m_pSceneActor(nullptr),
	m_pDefaultMaterial(nullptr),
	m_pConveyorMaterial(nullptr),
	m_pCooking(nullptr),
	m_pVisualDebugger(nullptr),
	m_fLoaded(false),
	m_fDisableWarning(false),
	m_fWorldChanged(false),
	m_traceStateChanges(false),
	m_flAccumulator(0.0)
{
	m_szMapName[0] = '\0';
	p_speeds_msg[0] = '\0';
	m_worldBounds.minimum = PxVec3(-32768, -32768, -32768);
	m_worldBounds.maximum = PxVec3(32768, 32768, 32768);
}

void CPhysicPhysX :: InitPhysic( void )
{
	if( m_pPhysics )
	{
		ALERT( at_error, "InitPhysic: physics already initalized\n" );
		return;
	}

	if( g_allow_physx != NULL && g_allow_physx->value == 0.0f )
	{
		ALERT( at_console, "InitPhysic: PhysX support is disabled by user.\n" );
		GameInitNullPhysics ();
		return;
	}

	m_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_errorCallback);
	if (!m_pFoundation)
	{
		ALERT(at_error, "InitPhysic: failed to create foundation\n");
		GameInitNullPhysics();
		return;
	}

	const PxTolerancesScale scale=physicsasset::AssetTolerancesScale();

	if (DebugEnabled()) 
	{
		m_pVisualDebugger = PxCreatePvd(*m_pFoundation);
		PxPvdTransport *transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 300);
		m_pVisualDebugger->connect(*transport, PxPvdInstrumentationFlag::eALL);
	}

	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pFoundation, scale, false, m_pVisualDebugger);
	m_pDispatcher = PxDefaultCpuDispatcherCreate(std::thread::hardware_concurrency());
	if (!m_pPhysics)
	{
		ALERT( at_error, "InitPhysic: failed to initalize physics engine\n" );
		GameInitNullPhysics();
		return;
	}

	m_pCooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_pFoundation, PxCookingParams(scale));
	if (!m_pCooking)
	{
		ALERT( at_warning, "InitPhysic: failed to initalize cooking library\n" );
	}

	PxSetAssertHandler(m_assertHandler);
	if (!PxInitExtensions(*m_pPhysics, m_pVisualDebugger)) {
		ALERT( at_warning, "InitPhysic: failed to initalize extensions\n" );
	}

	// create objects needed for scene creation
	m_debugRenderer = std::make_unique<DebugRenderer>();
	m_eventHandler = std::make_unique<EventHandler>();
	m_contactModifyCallback = std::make_unique<ContactModifyCallback>();
	m_holdableController = std::make_unique<HoldableItemController>();

	// create a scene
	PxSceneDesc sceneDesc(scale);
	sceneDesc.simulationEventCallback = m_eventHandler.get();
	sceneDesc.contactModifyCallback = m_contactModifyCallback.get();
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -800.0f);
	sceneDesc.flags = PxSceneFlag::eENABLE_CCD;
	sceneDesc.cpuDispatcher = m_pDispatcher;
	sceneDesc.sanityBounds = m_worldBounds;
	sceneDesc.filterShader = [](
		PxFilterObjectAttributes attributes0, PxFilterData filterData0,
		PxFilterObjectAttributes attributes1, PxFilterData filterData1,
		PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize) -> PxFilterFlags {
		    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
			{
				pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
				return PxFilterFlag::eDEFAULT;
			}

			pairFlags = PxPairFlag::eCONTACT_DEFAULT
					  | PxPairFlag::eDETECT_CCD_CONTACT
					  | PxPairFlag::eNOTIFY_TOUCH_CCD
					  |	PxPairFlag::eNOTIFY_TOUCH_FOUND
					  |	PxPairFlag::eNOTIFY_TOUCH_PERSISTS
					  | PxPairFlag::eNOTIFY_CONTACT_POINTS
					  | PxPairFlag::eCONTACT_EVENT_POSE;

			CollisionFilterData fd1(filterData0);
			CollisionFilterData fd2(filterData1);

			if (fd1.HasConveyorFlag() || fd2.HasConveyorFlag()) {
				pairFlags |= PxPairFlag::eMODIFY_CONTACTS;
			}

			return physicsasset::AssetPairFilter(filterData0,filterData1);
		};
	
    sceneDesc.filterCallback=&m_assetContactFilter;

	m_pScene = m_pPhysics->createScene(sceneDesc);

	if (DebugEnabled())
	{
		PxPvdSceneClient *pvdClient = m_pScene->getScenePvdClient();
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0f); // disable on release build because performance impact
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, 1.0f);
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, 1.0f);
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eBODY_AXES, 1.0f);

		if (pvdClient)
		{
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
		}
	}

	m_pDefaultMaterial = physicsasset::CreateAssetMaterial(*m_pPhysics);
	m_pConveyorMaterial = m_pPhysics->createMaterial(1.0f, 1.0f, 0.0f);
}

void CPhysicPhysX :: FreePhysic( void )
{
	m_physicsAssets.clear();
    // Joints reference both the root and child actors. They must be released
    // before the scene/physics objects (and before either actor is destroyed).
    for( auto &entry : m_physicsAssetCollisionStates )
        ReleaseAnimatedCollisionActors( entry.second );
    m_physicsAssetCollisionStates.clear();
	if (m_pScene) m_pScene->release();

	// All actors and scene-owned shape references have been released. Drop the
	// same map-owned caches cleared by FreeWorld (including missing .phys entries).
	ClearWorldAssetCaches();
	m_physicsAssetCollisionStates.clear();
	m_ragdollLegacyCollisionState.clear();
	m_ragdollEntityState.clear();
	m_ragdollNetworkState.clear();
	m_pendingRagdollImpulses.clear();
	m_ragdollSnapshotSequence = 0;

	if (m_pCooking) m_pCooking->release();
	if (m_pDispatcher) m_pDispatcher->release();
	if (m_pPhysics) m_pPhysics->release();
	if (m_pVisualDebugger) 
	{
		PxPvdTransport *transport = m_pVisualDebugger->getTransport();
		m_pVisualDebugger->release();
		if (transport) {
			transport->release();
		}
	}

	PxCloseExtensions();
	if (m_pFoundation) m_pFoundation->release();

	m_debugRenderer.reset();
	m_eventHandler.reset();
	m_contactModifyCallback.reset();
	m_holdableController.reset();

	m_pScene = nullptr;
	m_pCooking = nullptr;
	m_pPhysics = nullptr;
	m_pVisualDebugger = nullptr;
	m_pFoundation = nullptr;
}

void CPhysicPhysX :: Update( float flTimeDelta )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
		return;

    const bool measure = p_speeds && p_speeds->value >= 2.0f;
    m_performance.steps = 0;
    m_performance.frameMilliseconds = m_performance.simulateMilliseconds = 0;
    m_performance.fetchMilliseconds = m_performance.poseMilliseconds = 0;
    performance::ScopedTimer updateTimer(m_performance.frameMilliseconds, measure);
    if(!std::isfinite(flTimeDelta) || flTimeDelta < 0.0f) flTimeDelta = 0.0f;
    const bool diagnostics = std::isfinite(ragdoll_diagnostics.value) && ragdoll_diagnostics.value > 0;

	if( g_psv_gravity )
	{
		// clamp gravity
		if( g_psv_gravity->value < 0.0f )
			CVAR_SET_FLOAT( "sv_gravity", 0.0f );
		if( g_psv_gravity->value > 800.0f )
			CVAR_SET_FLOAT( "sv_gravity", 800.0f );

		PxVec3 gravity = m_pScene->getGravity();
		if( gravity.z != -( g_psv_gravity->value ))
		{
			ALERT( at_aiconsole, "gravity changed from %g to %g\n", gravity.z, -(g_psv_gravity->value));
			gravity.z = -(g_psv_gravity->value);
			m_pScene->setGravity( gravity );
		}
	}

	// Legacy game code sometimes assigns pev->body directly instead of calling
	// CBaseAnimating::SetBodygroup(). Keep collision proxies synchronized anyway.
	UpdatePhysicsAssetCollisionBodygroups();
	UpdateAnimatedCollisionActors();
	HandleEvents();

	for( auto &asset : m_physicsAssets )
	{
		asset->FollowAnimation();
		if( asset->IsRagdoll() && asset->NeedsAnimationControl() )
			asset->UpdateAnimationPose( gpGlobals ? gpGlobals->time : 0.0f );
	}

	m_flAccumulator += flTimeDelta;
	while (m_flAccumulator > k_SimulationStepSize)
	{
        ++m_performance.steps;
		m_flAccumulator -= k_SimulationStepSize;
		m_holdableController->ApplyForces();
		ApplyInteractivePartDrives( static_cast<float>(k_SimulationStepSize) );
		for( auto &asset : m_physicsAssets )
			asset->ApplyAnimationControl( static_cast<float>( k_SimulationStepSize ),
				ragdoll_drive_strength.value, ragdoll_drive_damping.value, ragdoll_drive_maxforce.value );
        {
            performance::ScopedTimer timer(m_performance.simulateMilliseconds, measure);
            m_pScene->simulate(k_SimulationStepSize);
        }
        {
            performance::ScopedTimer timer(m_performance.fetchMilliseconds, measure);
            m_pScene->fetchResults(true);
        }
		if(diagnostics)
		{
            performance::ScopedTimer timer(m_performance.poseMilliseconds, measure);
			for(auto &asset : m_physicsAssets) asset->CaptureSimulationPose();
			PxSimulationStatistics diagnosticStats;
			m_pScene->getSimulationStatistics(diagnosticStats);
			for( auto &asset : m_physicsAssets )
				asset->RecordDiagnostics(static_cast<float>(k_SimulationStepSize),&diagnosticStats,ragdoll_diagnostics.value,
					flTimeDelta,static_cast<float>(m_flAccumulator));
		}
	}
    // Only presentation/diagnostics consume these snapshots. Controllers read
    // actor state directly; normal play needs the final completed pose once.
    if(!diagnostics && m_performance.steps)
    {
        performance::ScopedTimer timer(m_performance.poseMilliseconds, measure);
        for(auto &asset : m_physicsAssets)
        {
            asset->CaptureSimulationPose();
            asset->RecordDiagnostics(static_cast<float>(k_SimulationStepSize), nullptr, 0);
        }
    }

	// Presentation is intentionally published once per server update, not once
	// per 100 Hz PhysX substep. Sleeping full ragdolls can be kinematic-frozen
	// after a grace period; direct .phys bullet queries remain valid and any
	// physical impulse thaws them before applying force.
	const float now = gpGlobals ? gpGlobals->time : 0.0f;
	for( auto &asset : m_physicsAssets )
	{
		if( !asset->IsRagdoll() ) continue;
		asset->UpdateSleepFreeze( now, ragdoll_freeze.value > 0.0f, ragdoll_freeze_delay.value );
		if( asset->HasSimulationPose() )
			UpdateRagdollPresentation( asset.get() );
	}
    // Sample completed solver poses after the final physics substep.
    SendInteractivePartPoses();
}

void CPhysicPhysX :: EndFrame( void )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
		return;

	// fill physics stats
	if( !p_speeds || p_speeds->value <= 0.0f )
		return;

	PxSimulationStatistics stats;
	m_pScene->getSimulationStatistics( stats );

	switch( (int)p_speeds->value )
	{
	case 1:
		Q_snprintf(p_speeds_msg, sizeof(p_speeds_msg), 
			"%3i active dynamic bodies\n%3i static bodies\n%3i dynamic bodies",
			stats.nbActiveDynamicBodies, 
			stats.nbStaticBodies, 
			stats.nbDynamicBodies
		);
		break;		
	case 2:
        Q_snprintf(p_speeds_msg, sizeof(p_speeds_msg),
            "PhysX steps %u | update %.2f ms\nsimulate %.2f | fetch %.2f | pose %.2f ms\n"
            "mesh builds %llu / hits %llu | build %.2f ms total\nshape flag changes %llu",
            m_performance.steps, m_performance.frameMilliseconds, m_performance.simulateMilliseconds,
            m_performance.fetchMilliseconds, m_performance.poseMilliseconds,
            static_cast<unsigned long long>(m_performance.meshBuilds),
            static_cast<unsigned long long>(m_performance.meshHits), m_performance.meshMilliseconds,
            static_cast<unsigned long long>(m_performance.shapeChanges));
        break;
	}
}

void CPhysicPhysX :: RemoveBody( edict_t *pEdict )
{
	if( !pEdict )
		return;

	CBaseEntity *entity = CBaseEntity::Instance( pEdict );
	PxActor *legacyActor = entity ? ActorFromEntity( entity ) : nullptr;

	// EventHandler keeps raw actor pointers between simulation and the following
	// HandleEvents() call. Purge them before releasing either articulated .phys
	// actors or the legacy one-actor-per-entity body. Waiting for a removed-shape
	// callback is too late because HandleEvents() runs before the next simulate().
	if( m_eventHandler )
	{
		std::vector<PxActor *> actors;
		for( const auto &asset : m_physicsAssets )
		{
			if( asset->Owner() == entity )
				asset->CollectActors( actors );
		}
		for( PxActor *actor : actors )
			m_eventHandler->purgeActorReferences( actor );
		auto collisionState = entity ? m_physicsAssetCollisionStates.find( entity->entindex() ) : m_physicsAssetCollisionStates.end();
		if( collisionState != m_physicsAssetCollisionStates.end() )
		{
			for( const PhysicsPartRuntime &runtime : collisionState->second.parts )
				if( runtime.actor ) m_eventHandler->purgeActorReferences( runtime.actor );
		}
		if( legacyActor )
			m_eventHandler->purgeActorReferences( legacyActor );
	}

	m_physicsAssets.erase( std::remove_if( m_physicsAssets.begin(), m_physicsAssets.end(),
		[entity]( const std::unique_ptr<PhysicsAssetInstance> &asset ) { return asset->Owner() == entity; } ), m_physicsAssets.end() );

	const int entityIndex = ENTINDEX( pEdict );
	auto collisionState = m_physicsAssetCollisionStates.find( entityIndex );
	if( collisionState != m_physicsAssetCollisionStates.end() )
	{
		ReleaseAnimatedCollisionActors( collisionState->second );
		m_physicsAssetCollisionStates.erase( collisionState );
	}
	m_ragdollLegacyCollisionState.erase( entityIndex );
	m_ragdollEntityState.erase( entityIndex );
	m_ragdollNetworkState.erase( entityIndex );
	m_pendingRagdollImpulses.erase( entityIndex );

	if( !m_pScene || pEdict->free || !entity )
		return; // scene purge all the objects automatically

	if (legacyActor)
	{
		m_pScene->removeActor(*legacyActor);
		legacyActor->release();
	}
	entity->m_pUserData = nullptr;
}

PxConvexMesh *CPhysicPhysX :: ConvexMeshFromBmodel( entvars_t *pev, int modelindex )
{
	if( !m_pCooking )
		return NULL; // don't spam console about missed NxCooking.dll

	if( modelindex == 1 )
	{
		ALERT( at_error, "ConvexMeshFromBmodel: can't create convex hull from worldmodel\n" );
		return NULL; // don't create rigidbody from world
          }

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( modelindex )) == NULL )
	{
		ALERT( at_error, "ConvexMeshFromBmodel: unbale to fetch model pointer %i\n", modelindex );
		return NULL;
	}

	if( bmodel->nummodelsurfaces <= 0 )
	{
		ALERT( at_aiconsole, "ConvexMeshFromBmodel: %s has no visible surfaces\n", bmodel->name );
		m_fDisableWarning = TRUE;
		return NULL;
	}

	int numVerts = 0, totalVerts = 0;
	PxConvexMesh *pHull = NULL;
	msurface_t *psurf;
	Vector *verts;
	int i, j;

	// compute vertexes count
	psurf = &bmodel->surfaces[bmodel->firstmodelsurface];
	for( i = 0; i < bmodel->nummodelsurfaces; i++, psurf++ )
		totalVerts += psurf->numedges;

	// create a temp vertices array
	verts = new Vector[totalVerts];

	psurf = &bmodel->surfaces[bmodel->firstmodelsurface];
	for( i = 0; i < bmodel->nummodelsurfaces; i++, psurf++ )
	{
		for( j = 0; j < psurf->numedges; j++ )
		{
			int e = bmodel->surfedges[psurf->firstedge+j];
			int v = (e > 0) ? bmodel->edges[e].v[0] : bmodel->edges[-e].v[1];
			verts[numVerts++] = bmodel->vertexes[v].position;
		}
	}

	PxConvexMeshDesc meshDesc;
	meshDesc.points.data = verts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.points.count = numVerts;
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	MemoryWriteBuffer outputBuffer;
	bool status = m_pCooking->cookConvexMesh(meshDesc, outputBuffer);
	delete [] verts;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );
		return NULL;
	}

	MemoryReadBuffer inputBuffer(outputBuffer.getData(), outputBuffer.getSize());
	pHull = m_pPhysics->createConvexMesh(inputBuffer);
	if( !pHull ) ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );

	return pHull;
}

PxTriangleMesh *CPhysicPhysX :: TriangleMeshFromBmodel( entvars_t *pev, int modelindex )
{
	if( !m_pCooking )
		return NULL; // don't spam console about missed NxCooking.dll

	if( modelindex == 1 )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: can't create triangle mesh from worldmodel\n" );
		return NULL; // don't create rigidbody from world
          }

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( modelindex )) == NULL )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: unable to fetch model pointer %i\n", modelindex );
		return NULL;
	}

	if( bmodel->nummodelsurfaces <= 0 )
	{
		ALERT( at_aiconsole, "TriangleMeshFromBmodel: %s has no visible surfaces\n", bmodel->name );
		m_fDisableWarning = TRUE;
		return NULL;
	}

	// don't build hulls for water
	if( FBitSet( bmodel->flags, MODEL_LIQUID ))
	{
		m_fDisableWarning = TRUE;
		return NULL;
	}

	int i, numElems = 0, totalElems = 0;
	PxTriangleMesh *pMesh = NULL;
	msurface_t *psurf;

	// compute vertexes count
	for( i = 0; i < bmodel->nummodelsurfaces; i++ )
	{
		psurf = &bmodel->surfaces[bmodel->firstmodelsurface + i];
		totalElems += (psurf->numedges - 2);
	}

	// create a temp indices array
	PxU32 *indices = new PxU32[totalElems * 3];

	for( i = 0; i < bmodel->nummodelsurfaces; i++ )
	{
		msurface_t *face = &bmodel->surfaces[bmodel->firstmodelsurface + i];
		bool reverse = (face->flags & SURF_PLANEBACK) ? true : false;
		int k = face->firstedge;

		// build the triangles from polygons
		for( int j = 0; j < face->numedges - 2; j++ )
		{
			indices[numElems*3+0] = ConvertEdgeToIndex( bmodel, k );
			indices[numElems*3+1] = ConvertEdgeToIndex( bmodel, k + j + 2 );
			indices[numElems*3+2] = ConvertEdgeToIndex( bmodel, k + j + 1 );
			numElems++;
		}
	}

	PxTriangleMeshDesc meshDesc;
	meshDesc.points.count = bmodel->numvertexes;
	meshDesc.points.stride = sizeof(mvertex_t);
	meshDesc.points.data = (const PxVec3*)&(bmodel->vertexes[0].position);	// pointer to all vertices in the map
	meshDesc.triangles.count = numElems;
	meshDesc.triangles.stride = 3 * sizeof(PxU32);
	meshDesc.triangles.data = indices;
	meshDesc.flags = (PxMeshFlags)0;

#ifdef _DEBUG
	// mesh should be validated before cooked without the mesh cleaning
	bool res = m_pCooking->validateTriangleMesh(meshDesc);
	PX_ASSERT(res);
#endif

	MemoryWriteBuffer outputBuffer;
	bool status = m_pCooking->cookTriangleMesh(meshDesc, outputBuffer);
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );
		return NULL;
	}

	MemoryReadBuffer inputBuffer(outputBuffer.getData(), outputBuffer.getSize());
	pMesh = m_pPhysics->createTriangleMesh(inputBuffer);
	if( !pMesh ) 
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );

	return pMesh;
}

void CPhysicPhysX :: StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q )
{
	mstudioanimvalue_t *panimvalue;
	Radian angle;

	for( int j = 0; j < 3; j++ )
	{
		if( !panim || panim->offset[j+3] == 0 )
		{
			angle[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			angle[j] = panimvalue[1].value;
			angle[j] = pbone->value[j+3] + angle[j] * pbone->scale[j+3];
		}
	}

	AngleQuaternion( angle, q );
}

void CPhysicPhysX :: StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos )
{
	mstudioanimvalue_t *panimvalue;

	for( int j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim && panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			pos[j] += panimvalue[1].value * pbone->scale[j];
		}
	}
}

clipfile::GeometryType CPhysicPhysX::ShapeTypeToGeomType(PxGeometryType::Enum geomType)
{
	switch (geomType)
	{
		case PxGeometryType::eCONVEXMESH: return clipfile::GeometryType::CookedConvexHull;
		case PxGeometryType::eTRIANGLEMESH: return clipfile::GeometryType::CookedTriangleMesh;
		case PxGeometryType::eBOX: return clipfile::GeometryType::CookedBox;
		default: return clipfile::GeometryType::Unknown;
	}
}

PxConvexMesh *CPhysicPhysX :: ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin )
{
	if( UTIL_GetModelType( modelindex ) != mod_studio )
	{
		ALERT( at_error, "CollisionFromStudio: not a studio model\n" );
		return NULL;
    }

	model_t *smodel = (model_t *)MODEL_HANDLE( modelindex );
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;

	if( !phdr || phdr->numbones < 1 )
	{
		ALERT( at_error, "CollisionFromStudio: bad model header\n" );
		return NULL;
	}

	PxConvexMesh *pHull = NULL;
	fs::Path cacheFileName;
	uint32_t modelStateHash = GetHashForModelState(smodel, body, skin);
	CacheNameForModel(smodel, cacheFileName, modelStateHash, ".hull");

	if (CheckFileTimes(smodel->name, cacheFileName.string().c_str()))
	{
		// hull is never than studiomodel. Trying to load it
		UserStream cacheFileStream(cacheFileName.string().c_str(), true);
		pHull = m_pPhysics->createConvexMesh(cacheFileStream);

		if( !pHull )
		{
			// we failed to loading existed hull and can't cooking new :(
			if( m_pCooking == NULL )
				return NULL; // don't spam console about missed nxCooking.dll

			// trying to rebuild hull
			ALERT( at_error, "Convex mesh for %s is corrupted. Rebuilding...\n", smodel->name );
		}
		else
		{
			// all is ok
			return pHull;
		}
	}
	else
	{
		// can't cooking new hull because nxCooking.dll is missed
		if( m_pCooking == NULL )
			return NULL; // don't spam console about missed nxCooking.dll

		// trying to rebuild hull
		ALERT( at_console, "Convex mesh for %s is out of Date. Rebuilding...\n", smodel->name );
	}

	// at this point nxCooking instance is always valid

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if( pseqdesc->seqgroup != 0 )
	{
		ALERT( at_error, "CollisionFromStudio: bad sequence group (must be 0)\n" );
		return NULL;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for( int i = 0; i < phdr->numbones; i++, pbone++, panim++ ) 
	{
		StudioCalcBoneQuaterion( pbone, panim, q[i] );
		StudioCalcBonePosition( pbone, panim, pos[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for( int i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 ) 
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix );
	}

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex);
	mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex);
	Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
	Vector *m_verts = new Vector[psubmodel->numverts];
	byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);
	Vector *verts = new Vector[psubmodel->numverts * 8];	// allocate temporary vertices array
	PxU32 *indices = new PxU32[psubmodel->numverts * 24];
	int numVerts = 0, numElems = 0;
	Vector tmp;

	// setup all the vertices
	for( int i = 0; i < psubmodel->numverts; i++ )
		m_verts[i] = bonetransform[pvertbone[i]].VectorTransform( pstudioverts[i] );

	for( int j = 0; j < psubmodel->nummesh; j++ ) 
	{
		int i;
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
		short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

		while( i = *( ptricmds++ ))
		{
			int	vertexState = 0;
			qboolean	tri_strip;

			if( i < 0 )
			{
				tri_strip = false;
				i = -i;
			}
			else
				tri_strip = true;

			for( ; i > 0; i--, ptricmds += 4 )
			{
				// build in indices
				if( vertexState++ < 3 )
				{
					indices[numElems++] = numVerts;
				}
				else if( tri_strip )
				{
					// flip triangles between clockwise and counter clockwise
					if( vertexState & 1 )
					{
						// draw triangle [n-2 n-1 n]
						indices[numElems++] = numVerts - 2;
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}
					else
					{
						// draw triangle [n-1 n-2 n]
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts - 2;
						indices[numElems++] = numVerts;
					}
				}
				else
				{
					// draw triangle fan [0 n-1 n]
					indices[numElems++] = numVerts - ( vertexState - 1 );
					indices[numElems++] = numVerts - 1;
					indices[numElems++] = numVerts;
				}

				verts[numVerts++] = m_verts[ptricmds[0]];
			}
		}
	}

	PxConvexMeshDesc meshDesc;
	meshDesc.indices.count = numElems / 3;
	meshDesc.indices.data = indices;
	meshDesc.indices.stride = 3 * sizeof( PxU32 );
	meshDesc.points.count = numVerts;
	meshDesc.points.data = verts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	UserStream outputFileStream(cacheFileName.string().c_str(), false);
	bool status = m_pCooking->cookConvexMesh(meshDesc, outputFileStream);

	delete [] verts;
	delete [] m_verts;
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );
		return NULL;
	}

	UserStream inputFileStream(cacheFileName.string().c_str(), true);
	pHull = m_pPhysics->createConvexMesh(inputFileStream);
	if( !pHull ) ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );

	return pHull;
}

PxTriangleMesh *CPhysicPhysX::TriangleMeshFromStudio(model_t *smodel, int32_t body, int32_t skin)
{
	if( !smodel || smodel->type != mod_studio )
	{
		ALERT(at_error, "TriangleMeshFromStudio: not a studio model\n");
		return NULL;
	}
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;
	int solidMeshes = 0;

	if (!phdr || phdr->numbones < 1)
	{
		ALERT(at_error, "TriangleMeshFromStudio: bad model header\n");
		return NULL;
	}

	// Keep cooked/loaded triangle meshes alive for the physics lifetime. Runtime
	// bodygroup switching can then replace PxShape geometry without recooking or
	// rereading the mesh from disk every time the same variant is selected.
	const std::string memoryCacheKey = fmt::format("{}|{}|{}", smodel->name, body, skin);
	auto memoryCacheIt = m_triangleMeshCache.find( memoryCacheKey );
	if( memoryCacheIt != m_triangleMeshCache.end() )
		return memoryCacheIt->second;

	mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);

	for (int i = 0; i < phdr->numtextures; i++)
	{
		// skip this mesh it's probably foliage or somewhat
		if (FBitSet(ptexture[i].flags, STUDIO_NF_MASKED) && !FBitSet(ptexture[i].flags, STUDIO_NF_SOLIDGEOM))
			continue;

		solidMeshes++;
	}

	// model is non-solid
	if (!solidMeshes)
	{
		m_fDisableWarning = TRUE;
		return NULL;
	}

	PxTriangleMesh *pMesh = NULL;
	fs::Path cacheFilePath;
	uint32_t modelStateHash = GetHashForModelState(smodel, body, skin);
	CacheNameForModel(smodel, cacheFilePath, modelStateHash, ".mesh");

	if (CheckFileTimes(smodel->name, cacheFilePath.string().c_str()) && !m_fWorldChanged)
	{
		// hull is never than studiomodel. Trying to load it
		UserStream cacheFileStream(cacheFilePath.string().c_str(), true);
		pMesh = m_pPhysics->createTriangleMesh(cacheFileStream);

		if (!pMesh)
		{
			// we failed to loading existed hull and can't cooking new :(
			if (m_pCooking == NULL)
				return NULL; // don't spam console about missed nxCooking.dll

			// trying to rebuild hull
			ALERT(at_error, "Triangle mesh for %s is corrupted. Rebuilding...\n", smodel->name);
		}
		else
		{
			// all is ok
			m_triangleMeshCache[memoryCacheKey] = pMesh;
			return pMesh;
		}
	}
	else
	{
		// can't cooking new hull because nxCooking.dll is missed
		if (m_pCooking == NULL)
			return NULL; // don't spam console about missed nxCooking.dll

		// trying to rebuild hull
		ALERT(at_console, "Triangle mesh for %s is out of Date. Rebuilding...\n", smodel->name);
	}

	// at this point nxCooking instance is always valid
	// Collision meshes are baked once from sequence 0, frame 0. Skeletal
	// animation is intentionally not recooked or skinned by PhysX here.
	// Whole-entity movement/rotation is handled by the actor transform.
	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if (pseqdesc->seqgroup != 0)
	{
		ALERT(at_error, "TriangleMeshFromStudio: bad sequence group (must be 0)\n");
		return NULL;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for (int i = 0; i < phdr->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(pbone, panim, q[i]);
		StudioCalcBonePosition(pbone, panim, pos[i]);
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4 transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for (int i = 0; i < phdr->numbones; i++)
	{
		// initialize bonematrix
		bonematrix = matrix3x4(pos[i], q[i]);

		if (pbone[i].parent == -1)
			bonetransform[i] = transform.ConcatTransforms(bonematrix);
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms(bonematrix);
	}

	int totalVertSize = 0;
	for (int i = 0; i < phdr->numbodyparts; i++)
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;
		int32_t index = (body / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		totalVertSize += psubmodel->numverts;
	}

	Vector *verts = new Vector[totalVertSize * 8]; // allocate temporary vertices array
	PxU32 *indices = new PxU32[totalVertSize * 24];
	int32_t skinNumber = bound( 0, skin, phdr->numskinfamilies );
	int numVerts = 0, numElems = 0;
	Vector tmp;

	for (int k = 0; k < phdr->numbodyparts; k++)
	{
		int i;
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + k;
		int32_t index = (body / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
		Vector *m_verts = new Vector[psubmodel->numverts];
		byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);

		// setup all the vertices
		for (i = 0; i < psubmodel->numverts; i++) {
			m_verts[i] = bonetransform[pvertbone[i]].VectorTransform(pstudioverts[i]);
		}

		ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
		short *pskinref = (short *)((byte *)phdr + phdr->skinindex);
		if (skinNumber != 0 && skinNumber < phdr->numskinfamilies)
			pskinref += (skinNumber * phdr->numskinref);

		for (int j = 0; j < psubmodel->nummesh; j++)
		{
			mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
			short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

			if (phdr->numtextures != 0 && phdr->textureindex != 0)
			{
				// skip this mesh it's probably foliage or somewhat
				const int flags = ptexture[pskinref[pmesh->skinref]].flags;
				if (FBitSet(flags, STUDIO_NF_MASKED) && !FBitSet(flags, STUDIO_NF_SOLIDGEOM))
					continue;
			}

			while (i = *(ptricmds++))
			{
				int	vertexState = 0;
				bool tri_strip;

				if (i < 0)
				{
					tri_strip = false;
					i = -i;
				}
				else
					tri_strip = true;

				for (; i > 0; i--, ptricmds += 4)
				{
					// build in indices
					if (vertexState++ < 3)
					{
						indices[numElems++] = numVerts;
					}
					else if (tri_strip)
					{
						// flip triangles between clockwise and counter clockwise
						if (vertexState & 1)
						{
							// draw triangle [n-2 n-1 n]
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts;
						}
						else
						{
							// draw triangle [n-1 n-2 n]
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts;
						}
					}
					else
					{
						// draw triangle fan [0 n-1 n]
						indices[numElems++] = numVerts - (vertexState - 1);
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}

					//	verts[numVerts++] = m_verts[ptricmds[0]];
					verts[numVerts] = m_verts[ptricmds[0]];
					numVerts++;
				}
			}
		}

		delete[] m_verts;
	}

	PxTriangleMeshDesc meshDesc;
	meshDesc.triangles.data = indices;
	meshDesc.triangles.count = numElems / 3;
	meshDesc.triangles.stride = 3 * sizeof(PxU32);
	meshDesc.points.data = verts;
	meshDesc.points.count = numVerts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.flags = PxMeshFlag::eFLIPNORMALS;

#ifdef _DEBUG
	// mesh should be validated before cooked without the mesh cleaning
	bool res = m_pCooking->validateTriangleMesh(meshDesc);
	PX_ASSERT(res);
#endif

	UserStream outputFileStream(cacheFilePath.string().c_str(), false);
	bool status = m_pCooking->cookTriangleMesh(meshDesc, outputFileStream);
	delete[] verts;
	delete[] indices;

	if (!status)
	{
		ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);
		return NULL;
	}

	UserStream inputFileStream(cacheFilePath.string().c_str(), true);
	pMesh = m_pPhysics->createTriangleMesh(inputFileStream);
	if (!pMesh)
	{
		ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);
		return NULL;
	}

	m_triangleMeshCache[memoryCacheKey] = pMesh;
	return pMesh;
}

static bool BuildPhysicsAssetCollisionBuffers( CBaseEntity *pObject, const PhysicsAssetDef &asset,
	std::vector<PxVec3> &vertices, std::vector<PxU32> &indices )
{
	vertices.clear();
	indices.clear();
	if( !pObject || asset.collisions.empty() )
		return false;

	model_t *renderModel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	if( !renderModel || renderModel->type != mod_studio )
		return false;

	for( const physicsasset::CollisionDef &collision : asset.collisions )
	{
		// Bone-bound v3 sections are represented by their own kinematic actors.
		// The legacy/root prop actor contains only entity-space collision.
		if( !collision.bone.empty() || !CollisionSectionActive( collision, renderModel, pObject->pev->body ) || collision.disabled )
			continue;

		const PxU32 base = static_cast<PxU32>( vertices.size() );
		for( const physicsasset::Vec3 &v : collision.vertices )
			vertices.emplace_back( v.x, v.y, v.z );
		for( std::uint32_t index : collision.indices )
			indices.push_back( base + static_cast<PxU32>( index ));
	}

	return vertices.size() >= 3 && indices.size() >= 3 && ( indices.size() % 3 ) == 0;
}

static bool PhysicsAssetCollisionSelectionHasGeometry( CBaseEntity *pObject, const PhysicsAssetDef &asset )
{
	if( !pObject ) return false;
	model_t *renderModel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	if( !renderModel || renderModel->type != mod_studio ) return false;
	for( const physicsasset::CollisionDef &collision : asset.collisions )
	{
		if( collision.bone.empty() && CollisionSectionActive( collision, renderModel, pObject->pev->body ) && !collision.disabled &&
			!collision.vertices.empty() && !collision.indices.empty() )
			return true;
	}
	return false;
}

PxTriangleMesh *CPhysicPhysX::TriangleMeshFromPhysicsAsset( CBaseEntity *pObject, const PhysicsAssetDef &asset )
{
	if( !pObject || !m_pPhysics || !m_pCooking )
		return NULL;

	const std::string cacheKey = fmt::format( "physasset:{}|fingerprint:{}|body:{}|triangle", asset.sourcePath, asset.collisionFingerprint, pObject->pev->body );
	auto cached = m_triangleMeshCache.find( cacheKey );
	if( cached != m_triangleMeshCache.end() )
		return cached->second;

	std::vector<PxVec3> vertices;
	std::vector<PxU32> indices;
	if( !BuildPhysicsAssetCollisionBuffers( pObject, asset, vertices, indices ))
		return NULL;

	PxTriangleMeshDesc desc;
	desc.points.count = static_cast<PxU32>( vertices.size() );
	desc.points.stride = sizeof(PxVec3);
	desc.points.data = vertices.data();
	desc.triangles.count = static_cast<PxU32>( indices.size() / 3 );
	desc.triangles.stride = 3 * sizeof(PxU32);
	desc.triangles.data = indices.data();
	desc.flags = PxMeshFlag::eFLIPNORMALS;

#ifdef _DEBUG
	if( !m_pCooking->validateTriangleMesh( desc ))
	{
		ALERT( at_error, "%s: invalid v2 physics-asset collision triangle mesh for body state %d\n",
			asset.sourcePath.c_str(), pObject->pev->body );
		return NULL;
	}
#endif

	MemoryWriteBuffer cooked;
	if( !m_pCooking->cookTriangleMesh( desc, cooked ))
	{
		ALERT( at_error, "%s: failed to cook v2 physics-asset triangle collision\n", asset.sourcePath.c_str() );
		return NULL;
	}
	MemoryReadBuffer input( cooked.getData(), cooked.getSize() );
	PxTriangleMesh *mesh = m_pPhysics->createTriangleMesh( input );
	if( !mesh )
		return NULL;
	m_triangleMeshCache[cacheKey] = mesh;
	return mesh;
}

PxConvexMesh *CPhysicPhysX::ConvexMeshFromPhysicsAsset( CBaseEntity *pObject, const PhysicsAssetDef &asset )
{
	if( !pObject || !m_pPhysics || !m_pCooking )
		return NULL;

	const std::string cacheKey = fmt::format( "physasset:{}|fingerprint:{}|body:{}|convex", asset.sourcePath, asset.collisionFingerprint, pObject->pev->body );
	auto cached = m_physicsAssetConvexMeshCache.find( cacheKey );
	if( cached != m_physicsAssetConvexMeshCache.end() )
		return cached->second;

	std::vector<PxVec3> vertices;
	std::vector<PxU32> indices;
	if( !BuildPhysicsAssetCollisionBuffers( pObject, asset, vertices, indices ))
		return NULL;

	PxConvexMeshDesc desc;
	desc.points.count = static_cast<PxU32>( vertices.size() );
	desc.points.stride = sizeof(PxVec3);
	desc.points.data = vertices.data();
	desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	MemoryWriteBuffer cooked;
	if( !m_pCooking->cookConvexMesh( desc, cooked ))
	{
		ALERT( at_error, "%s: failed to cook v2 physics-asset convex collision\n", asset.sourcePath.c_str() );
		return NULL;
	}
	MemoryReadBuffer input( cooked.getData(), cooked.getSize() );
	PxConvexMesh *mesh = m_pPhysics->createConvexMesh( input );
	if( !mesh )
		return NULL;
	m_physicsAssetConvexMeshCache[cacheKey] = mesh;
	return mesh;
}

PxTriangleMesh *CPhysicPhysX::TriangleMeshFromPhysicsAssetSection( const PhysicsAssetDef &asset, size_t collisionIndex )
{
	if( !m_pPhysics || !m_pCooking || collisionIndex >= asset.collisions.size() ) return NULL;
	const physicsasset::CollisionDef &collision = asset.collisions[collisionIndex];
	if( collision.disabled || collision.vertices.size() < 3 || collision.indices.size() < 3 ) return NULL;

	const std::string cacheKey = fmt::format( "physasset:{}|fingerprint:{}|section:{}|triangle", asset.sourcePath, asset.collisionFingerprint, collisionIndex );
	auto cached = m_triangleMeshCache.find( cacheKey );
	if( cached != m_triangleMeshCache.end() ) return cached->second;

	std::vector<PxVec3> vertices;
	vertices.reserve( collision.vertices.size() );
	for( const physicsasset::Vec3 &v : collision.vertices ) vertices.emplace_back( v.x, v.y, v.z );
	std::vector<PxU32> indices;
	indices.reserve( collision.indices.size() );
	for( std::uint32_t i : collision.indices ) indices.push_back( static_cast<PxU32>( i ));

	PxTriangleMeshDesc desc;
	desc.points.count = static_cast<PxU32>( vertices.size() );
	desc.points.stride = sizeof(PxVec3);
	desc.points.data = vertices.data();
	desc.triangles.count = static_cast<PxU32>( indices.size() / 3 );
	desc.triangles.stride = 3 * sizeof(PxU32);
	desc.triangles.data = indices.data();
	desc.flags = PxMeshFlag::eFLIPNORMALS;

	MemoryWriteBuffer cooked;
	if( !m_pCooking->cookTriangleMesh( desc, cooked ) )
	{
		ALERT( at_error, "%s: failed to cook bone-bound collision section '%s'\n", asset.sourcePath.c_str(), collision.name.c_str() );
		return NULL;
	}
	MemoryReadBuffer input( cooked.getData(), cooked.getSize() );
	PxTriangleMesh *mesh = m_pPhysics->createTriangleMesh( input );
	if( mesh ) m_triangleMeshCache[cacheKey] = mesh;
	return mesh;
}

PxConvexMesh *CPhysicPhysX::ConvexMeshFromPhysicsAssetSection( const PhysicsAssetDef &asset, size_t collisionIndex )
{
	if( !m_pPhysics || !m_pCooking || collisionIndex >= asset.collisions.size() ) return NULL;
	const physicsasset::CollisionDef &collision = asset.collisions[collisionIndex];
	if( collision.disabled || collision.vertices.size() < 3 ) return NULL;

	const std::string cacheKey = fmt::format( "physasset:{}|fingerprint:{}|section:{}|convex", asset.sourcePath, asset.collisionFingerprint, collisionIndex );
	auto cached = m_physicsAssetConvexMeshCache.find( cacheKey );
	if( cached != m_physicsAssetConvexMeshCache.end() ) return cached->second;

	std::vector<PxVec3> vertices;
	vertices.reserve( collision.vertices.size() );
	for( const physicsasset::Vec3 &v : collision.vertices ) vertices.emplace_back( v.x, v.y, v.z );
	PxConvexMeshDesc desc;
	desc.points.count = static_cast<PxU32>( vertices.size() );
	desc.points.stride = sizeof(PxVec3);
	desc.points.data = vertices.data();
	desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	MemoryWriteBuffer cooked;
	if( !m_pCooking->cookConvexMesh( desc, cooked ) )
	{
		ALERT( at_error, "%s: failed to cook bone-bound convex section '%s'\n", asset.sourcePath.c_str(), collision.name.c_str() );
		return NULL;
	}
	MemoryReadBuffer input( cooked.getData(), cooked.getSize() );
	PxConvexMesh *mesh = m_pPhysics->createConvexMesh( input );
	if( mesh ) m_physicsAssetConvexMeshCache[cacheKey] = mesh;
	return mesh;
}

bool CPhysicPhysX::HasPhysicsAssetCollision( CBaseEntity *pObject )
{
	std::shared_ptr<PhysicsAssetDef> def = GetPhysicsAssetDefinition( pObject );
	return def && physicsasset::HasCollisionGeometry( *def );
}

PxConvexMesh *CPhysicPhysX :: ConvexMeshFromEntity( CBaseEntity *pObject )
{
	if( !pObject || !m_pPhysics )
		return NULL;

	// check for bspmodel
	model_t *model = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );

	if( !model || model->type == mod_bad )
	{
		ALERT( at_aiconsole, "ConvexMeshFromEntity: entity %s has NULL model\n", pObject->GetClassname( )); 
		return NULL;
	}

	PxConvexMesh *pCollision = NULL;

	if( model->type == mod_studio )
	{
		std::shared_ptr<PhysicsAssetDef> asset = GetPhysicsAssetDefinition( pObject );
		if( asset && physicsasset::HasCollisionGeometry( *asset ))
			pCollision = ConvexMeshFromPhysicsAsset( pObject, *asset );
	}

	// Preserve original PrimeXT collision generation when the model has no .phys
	// collision section or the asset collision could not be cooked.
	if( pCollision )
		return pCollision;

	// call the appropriate legacy loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = ConvexMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = ConvexMeshFromStudio( pObject->pev, pObject->pev->modelindex, pObject->pev->body, pObject->pev->skin );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
		ALERT( at_warning, "ConvexMeshFromEntity: %i has missing collision\n", pObject->pev->modelindex ); 
	m_fDisableWarning = FALSE;

	return pCollision;
}

PxTriangleMesh *CPhysicPhysX :: TriangleMeshFromEntity( CBaseEntity *pObject )
{
	if( !pObject || !m_pPhysics )
		return NULL;

	model_t *model = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	if( !model || model->type == mod_bad )
	{
		ALERT( at_aiconsole, "TriangleMeshFromEntity: entity %s has NULL model\n", pObject->GetClassname() );
		return NULL;
	}

	PxTriangleMesh *pCollision = NULL;
	if( model->type == mod_studio )
	{
		std::shared_ptr<PhysicsAssetDef> asset = GetPhysicsAssetDefinition( pObject );
		if( asset && physicsasset::HasCollisionGeometry( *asset ))
			pCollision = TriangleMeshFromPhysicsAsset( pObject, *asset );
	}

	if( !pCollision )
	{
		switch( model->type )
		{
		case mod_brush:
			pCollision = TriangleMeshFromBmodel( pObject->pev, pObject->pev->modelindex );
			break;
		case mod_studio:
			pCollision = TriangleMeshFromStudio( model, pObject->pev->body, pObject->pev->skin );
			break;
		}
	}

	if( !pCollision && !m_fDisableWarning )
		ALERT( at_warning, "TriangleMeshFromEntity: %s has missing collision\n", pObject->GetClassname() );
	m_fDisableWarning = FALSE;
	return pCollision;
}

PxActor *CPhysicPhysX :: ActorFromEntity( CBaseEntity *pObject )
{
	if( FNullEnt( pObject ) || !pObject->m_pUserData )
		return NULL;
#if defined (HAS_PHYSIC_VEHICLE)
	if( pObject->m_iActorType == ACTOR_VEHICLE )
	{
		NxVehicle	*pVehicle = (NxVehicle *)pObject->m_pUserData;
		return pVehicle->getActor();
	}
#endif
	return (PxActor *)pObject->m_pUserData;
}

void CPhysicPhysX::ReleaseAnimatedCollisionActors( PhysicsAssetCollisionState &state )
{
    // Model replacement/teardown must terminate per-bone presentation before
    // the old actors and bone indices disappear. Clients also have a timeout
    // fallback for clears lost outside the owner's PVS.
    if( state.edict && !state.edict->free && gmsgPartPose &&
        GET_SERVER_STATE() == SERVER_ACTIVE )
    {
        CBaseEntity *entity=CBaseEntity::Instance(state.edict);
        if( entity ) for( const PhysicsPartRuntime &part : state.parts )
        {
            if( !part.partNetActive || part.boneIndex<0 || part.boneIndex>=MAXSTUDIOBONES ) continue;
            ++m_partSnapshotSequence;
            if( !m_partSnapshotSequence ) ++m_partSnapshotSequence;
            MESSAGE_BEGIN(MSG_PVS,gmsgPartPose,entity->GetAbsOrigin());
                WRITE_ENTITY(entity->entindex());
                WRITE_BYTE(partnet::kVersion);
                WRITE_BYTE(partnet::kClear);
                WRITE_LONG(static_cast<int32_t>(m_partSnapshotSequence));
                WRITE_BYTE(part.boneIndex);
                WRITE_FLOAT(0); WRITE_FLOAT(0); WRITE_FLOAT(0);
                WRITE_FLOAT(0); WRITE_FLOAT(0); WRITE_FLOAT(0); WRITE_FLOAT(1);
            MESSAGE_END();
        }
    }
    for( PhysicsPartRuntime &part : state.parts )
        part.hinge = nullptr;
    for( PxRevoluteJoint *hinge : state.hinges )
        if( hinge ) hinge->release();
    state.hinges.clear();
    for( PhysicsPartRuntime &part : state.parts )
    {
        if( !part.actor ) continue;
        if( m_eventHandler ) m_eventHandler->purgeActorReferences( part.actor );
        if( m_pScene ) m_pScene->removeActor( *part.actor );
        part.actor->release(); // exclusive shapes belong to this actor
        part.actor = nullptr;
        part.shapes.clear();
    }
    state.parts.clear();
    state.animatedTraceMesh.reset();
    state.traceKey.valid = state.traceCandidate.valid = false;
}

void CPhysicPhysX::SetAnimatedCollisionEnabled( PhysicsAssetCollisionState &state, bool enabled )
{
    CBaseEntity *entity = state.edict && !state.edict->free ? CBaseEntity::Instance( state.edict ) : nullptr;
    std::shared_ptr<PhysicsAssetDef> asset = entity ? GetPhysicsAssetDefinition( entity ) : nullptr;
    if( !entity || !asset ) return;
    model_t *renderModel = (model_t *)MODEL_HANDLE( entity->pev->modelindex );
    for( PhysicsPartRuntime &part : state.parts )
    {
        const bool partActive = !state.articulatedOverride && part.actor;
        for( PhysicsPartShape &runtime : part.shapes )
        {
            if( !runtime.shape ) continue;
            if( runtime.collisionIndex >= asset->collisions.size() )
            {
                const bool active = enabled && partActive;
                const PxShapeFlags flags = runtime.shape->getFlags();
                if(flags.isSet(PxShapeFlag::eSIMULATION_SHAPE) != active)
                { runtime.shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, active); ++m_performance.shapeChanges; }
                if(flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) != active)
                { runtime.shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, active); ++m_performance.shapeChanges; }
                continue;
            }
            const physicsasset::CollisionDef &collision = asset->collisions[runtime.collisionIndex];
            const bool active = enabled && partActive && !collision.disabled &&
                CollisionSectionActive( collision, renderModel, entity->pev->body );
            const PxShapeFlags flags = runtime.shape->getFlags();
            if(flags.isSet(PxShapeFlag::eSIMULATION_SHAPE) != active)
            { runtime.shape->setFlag( PxShapeFlag::eSIMULATION_SHAPE, active ); ++m_performance.shapeChanges; }
            if(flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) != active)
            { runtime.shape->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, active ); ++m_performance.shapeChanges; }
        }
    }
}

// Record only distinct, monotonically advancing animation samples. This
// avoids fake angular spikes when entity update and physics tick run in one frame.
void CPhysicPhysX::SampleInteractivePart( CBaseEntity *entity, PhysicsPartRuntime &part )
{
    if( !entity || !part.actor || part.boneIndex < 0 ||
        (part.owner != physicsasset::PartOwner::AnimatedProxy && part.mode != PhysicsPartMode::Driven) ) return;
    const float now = gpGlobals ? gpGlobals->time : 0.0f;
    const PxTransform target = StudioBoneWorldTransform(entity, part.boneIndex);
    if( !target.isValid() || !std::isfinite(now) ) return;
    if( !part.sampledPose || now < part.latestSampleTime || now - part.latestSampleTime > 0.25f )
    {
        part.previousAnimatedPose = target;
        part.latestAnimatedPose = target;
        part.previousSampleTime = now;
        part.latestSampleTime = now;
        part.sampledPose = true;
    }
    else if( now > part.latestSampleTime + 0.00001f )
    {
        part.previousAnimatedPose = part.latestAnimatedPose;
        part.previousSampleTime = part.latestSampleTime;
        part.latestAnimatedPose = target;
        part.latestSampleTime = now;
    }
    else
        part.latestAnimatedPose = target; // multiple animation updates same timestamp
    if( part.mode == PhysicsPartMode::Animated && part.owner == physicsasset::PartOwner::AnimatedProxy )
        part.actor->setKinematicTarget(target);
}

// Server-authoritative handoff of the EXISTING jointed part actor. Driven
// uses a bounded hinge motor while Free disables it, with no duplicate collider.
bool CPhysicPhysX::SetPhysicsPartMode( CBaseEntity *entity, const char *partName, PhysicsPartMode mode )
{
    if( !entity || !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE ) return false;
    if( mode != PhysicsPartMode::Animated && mode != PhysicsPartMode::Driven &&
        mode != PhysicsPartMode::Free ) return false;
    const auto stateIt = m_physicsAssetCollisionStates.find(entity->entindex());
    if( stateIt == m_physicsAssetCollisionStates.end() || stateIt->second.articulatedOverride ||
        stateIt->second.edict != entity->edict() ) return false;
    PhysicsPartRuntime *part = FindPhysicsPart(entity,partName);
    if( !part || !part->actor || !part->dynamicCompatible ) return false;
    if( part->detached ) return mode == PhysicsPartMode::Free;
    if( !part->hasHinge ) return false;
    if( part->mode == mode ) return true;
    if( mode == PhysicsPartMode::Driven && (!part->hinge || !(part->driveStrength > 0.0f)) )
        return false;
    const bool physical = mode != PhysicsPartMode::Animated;
    const bool wasPhysical = part->owner == physicsasset::PartOwner::ArticulatedBody;
    PxRigidDynamic *actor = part->actor;
    if(physical && !wasPhysical)
    {
        // Capture the current animation pose before removing kinematic ownership.
        SampleInteractivePart(entity,*part);
        if( !part->sampledPose ) return false;
        const PxTransform current = part->latestAnimatedPose;
        physicsasset::MotionPose from, to;
        auto convert = [](const PxTransform &p) -> physicsasset::MotionPose {
            return {{p.p.x,p.p.y,p.p.z},{p.q.x,p.q.y,p.q.z,p.q.w}};
        };
        from = convert(part->previousAnimatedPose);
        to = convert(current);
        const auto velocity = physicsasset::EstimateMotion(from,to,
            part->latestSampleTime-part->previousSampleTime);
        actor->setGlobalPose(current);
        actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC,false);
        actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,part->ccd);
        actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY,false);
        const PxVec3 linear = velocity.valid ? PxVec3(velocity.linear.x,velocity.linear.y,velocity.linear.z) : PxVec3(0);
        const PxVec3 angular = velocity.valid ? PxVec3(velocity.angular.x,velocity.angular.y,velocity.angular.z) : PxVec3(0);
        actor->setLinearVelocity(linear);
        actor->setAngularVelocity(angular);
        actor->wakeUp();
        part->owner = physicsasset::PartOwner::ArticulatedBody;
    }
    else if( !physical )
    {
        // Return to animation only when the physics body is already aligned
        // and settled. Driven is the motor-assisted recovery mode.
        // This deliberately avoids interpolating a joint through walls.
        const PxTransform target = StudioBoneWorldTransform(entity,part->boneIndex);
        const PxTransform current = actor->getGlobalPose();
        const PxQuat delta = target.q * current.q.getConjugate();
        const float orientation = PxAbs(delta.w);
        if( (target.p-current.p).magnitude() > 0.05f || orientation < 0.999999f ||
            actor->getLinearVelocity().magnitude() > 0.5f ||
            actor->getAngularVelocity().magnitude() > 0.05f )
            return false; // request Driven first to recover without snapping
        // Clear residual momentum while this actor is still dynamic.
        actor->setLinearVelocity(PxVec3(0));
        actor->setAngularVelocity(PxVec3(0));
        actor->setGlobalPose(target);
        actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC,true);
        actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY,true);
        part->sampledPose = false; // new animation sampling epoch
        part->owner = physicsasset::PartOwner::AnimatedProxy;
    }
    if( part->hinge )
        part->hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
    part->mode = mode;
    part->hasTargetAngle = false; // new target sampling epoch on each handoff
    part->targetAngularVelocity = 0.0f;
    if( part->hinge && mode == PhysicsPartMode::Driven )
    {
        part->hinge->setDriveForceLimit(part->driveMaxForce > 0.0f ? part->driveMaxForce : 4000.0f);
        part->hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, true);
        part->actor->wakeUp();
    }
    SetAnimatedCollisionEnabled(stateIt->second,stateIt->second.collisionRequested);
    if(auto asset = GetPhysicsAssetDefinition(entity))
        RebuildAnimatedCollisionTraceMesh(entity,stateIt->second,*asset);
    UpdateEntityAABB(entity);
    entity->RelinkEntity(FALSE);
    return true;
}

// I.7: the detached part continues to use its existing dynamic actor and
// collision shapes. Only the constraint is removed, so no duplicate door is
// spawned and the existing impulse/world-pose paths remain authoritative.
bool CPhysicPhysX::DetachPhysicsPart( CBaseEntity *entity, PhysicsPartRuntime &part )
{
    if( !entity || !part.actor || !part.hinge || part.detached ) return false;
    if( part.owner != physicsasset::PartOwner::ArticulatedBody &&
        !SetPhysicsPartMode(entity, part.name.c_str(), PhysicsPartMode::Free) ) return false;
    if( part.mode != PhysicsPartMode::Free &&
        !SetPhysicsPartMode(entity, part.name.c_str(), PhysicsPartMode::Free) ) return false;
    auto stateIt = m_physicsAssetCollisionStates.find(entity->entindex());
    if( stateIt == m_physicsAssetCollisionStates.end() || stateIt->second.edict != entity->edict() )
        return false;
    PxRevoluteJoint *joint = part.hinge;
    joint->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
    auto &hinges = stateIt->second.hinges;
    hinges.erase(std::remove(hinges.begin(), hinges.end(), joint), hinges.end());
    part.hinge = nullptr;
    part.hasHinge = false;
    part.detached = true;
    part.mode = PhysicsPartMode::Free;
    part.hasTargetAngle = false;
    joint->release();
    part.actor->wakeUp();
    return true;
}

bool CPhysicPhysX::ApplyPhysicsPartDamageEvent( CBaseEntity *entity,
    PhysicsPartRuntime &part, float damage, float impulseMagnitude )
{
    if( !entity || !part.actor || !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE ) return false;
    const physicsasset::PartFailurePolicy policy{
        part.releaseDamage, part.breakDamage, part.breakImpulse };
    const auto decision = physicsasset::EvaluatePartFailure(policy,
        part.accumulatedDamage, damage, impulseMagnitude, part.detached);
    if( !decision.valid ) return false;
    part.accumulatedDamage = decision.accumulatedDamage;
    if( decision.detach ) return DetachPhysicsPart(entity, part);
    if( decision.release && !part.detached && part.mode != PhysicsPartMode::Free )
        return SetPhysicsPartMode(entity, part.name.c_str(), PhysicsPartMode::Free);
    return true;
}

int CPhysicPhysX::CaptureInteractiveParts(CBaseEntity *entity, PhysicsPartSaveRecord *out, int capacity)
{
    if (!entity || !out || capacity<=0) return 0;
    auto state=m_physicsAssetCollisionStates.find(entity->entindex());
    if (state==m_physicsAssetCollisionStates.end() || state->second.edict!=entity->edict()) return 0;
    int count=0;
    for (const PhysicsPartRuntime &part : state->second.parts) {
        if (!part.actor || part.name.size()>=sizeof(out[0].name) || count>=capacity) continue;
        PhysicsPartSaveRecord &save=out[count++];
        std::memset(&save,0,sizeof(save));
        std::memcpy(save.name,part.name.c_str(),part.name.size());
        save.mode=static_cast<int>(part.mode);
        save.detached=part.detached?1:0;
        save.damage=part.accumulatedDamage;
        const PxTransform pose=part.actor->getGlobalPose();
        save.pose[0]=pose.p.x; save.pose[1]=pose.p.y; save.pose[2]=pose.p.z;
        save.pose[3]=pose.q.x; save.pose[4]=pose.q.y;
        save.pose[5]=pose.q.z; save.pose[6]=pose.q.w;
        save.linear[0]=part.actor->getLinearVelocity().x;
        save.linear[1]=part.actor->getLinearVelocity().y;
        save.linear[2]=part.actor->getLinearVelocity().z;
        save.angular[0]=part.actor->getAngularVelocity().x;
        save.angular[1]=part.actor->getAngularVelocity().y;
        save.angular[2]=part.actor->getAngularVelocity().z;
    }
    if (static_cast<int>(state->second.parts.size())>capacity)
        ALERT(at_warning,"Physics part save limit %d reached for %s\n",capacity,entity->GetClassname());
    return count;
}

void CPhysicPhysX::RestoreInteractiveParts(CBaseEntity *entity, const PhysicsPartSaveRecord *records, int count)
{
    if(!entity || !records || count <= 0 || count > kSavedPhysicsPartLimit || !m_pScene) return;
    auto state = m_physicsAssetCollisionStates.find(entity->entindex());
    if(state == m_physicsAssetCollisionStates.end() || state->second.edict != entity->edict())
    {
        TrackPhysicsAssetCollision(entity);
        state = m_physicsAssetCollisionStates.find(entity->entindex());
    }
    if(state == m_physicsAssetCollisionStates.end() || state->second.edict != entity->edict() ||
        state->second.articulatedOverride) return;

    // Assets may have changed since the save. Positive uniform scale alone is
    // representable by the current hinge/primitive geometry; nonuniform scale
    // cannot safely activate the actor. Do not mutate an actor on rejection.
    const Vector scale = entity->GetScale();
    float uniformScale = 1.0f;
    const bool scaleOK = physicsasset::RigidUniformScale(
        {scale.x, scale.y, scale.z}, uniformScale);
    for(int i = 0; i < count; ++i)
    {
        const PhysicsPartSaveRecord &saved = records[i];
        if(!std::memchr(saved.name, 0, sizeof(saved.name)) ||
            !physicsasset::ValidRestoredPartSnapshot(saved.damage,
                saved.pose, saved.linear, saved.angular)) continue;
        PhysicsPartRuntime *part = FindPhysicsPart(entity, saved.name);
        if(!part || !part->actor) continue;

        PxRigidActor *parent = nullptr, *child = nullptr;
        if(part->hinge) part->hinge->getActors(parent, child);
        const bool hingeAttached = part->hasHinge && part->hinge &&
            parent && parent != part->actor && child == part->actor &&
            std::find(state->second.hinges.begin(), state->second.hinges.end(),
                part->hinge) != state->second.hinges.end();
        const physicsasset::PartRestoreSetup setup{
            true, part->dynamicCompatible, hingeAttached,
            part->owner == physicsasset::PartOwner::AnimatedProxy &&
                part->mode == PhysicsPartMode::Animated,
            part->actor->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC),
            part->detached, state->second.articulatedOverride,
            state->second.collisionRequested, scaleOK,
            part->driveStrength, part->driveDamping, part->driveMaxForce
        };
        if(!physicsasset::CanRestoreInteractivePart(setup, saved.mode, saved.detached))
        {
            ALERT(at_warning, "Skipping incompatible saved physics part '%s' for %s\n",
                saved.name, entity->GetClassname());
            continue;
        }

        // Animated records only carry damage; the current animation determines
        // pose, never the possibly obsolete position stored in the savegame.
        if(saved.mode == 0)
        {
            part->accumulatedDamage = saved.damage;
            part->partNetNextSend = 0.0f;
            part->partNetActive = false;
            continue;
        }
        const PxTransform pose(PxVec3(saved.pose[0], saved.pose[1], saved.pose[2]),
            PxQuat(saved.pose[3], saved.pose[4], saved.pose[5], saved.pose[6]));
        const PxVec3 linear(saved.linear[0], saved.linear[1], saved.linear[2]);
        const PxVec3 angular(saved.angular[0], saved.angular[1], saved.angular[2]);
        if(!pose.isValid() || !linear.isFinite() || !angular.isFinite()) continue;

        part->actor->setGlobalPose(pose);
        part->actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
        part->actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,part->ccd);
        part->actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, false);
        part->actor->setLinearVelocity(linear);
        part->actor->setAngularVelocity(angular);
        part->mode = static_cast<PhysicsPartMode>(saved.mode);
        part->owner = physicsasset::PartOwner::ArticulatedBody;
        part->accumulatedDamage = saved.damage;
        part->hasTargetAngle = false;
        part->targetAngularVelocity = 0.0f;
        part->sampledPose = false;
        if(saved.detached)
        {
            // It was a valid, attached joint when this restore began. Release
            // that joint only AFTER all eligibility and pose checks pass.
            PxRevoluteJoint *hinge = part->hinge;
            auto &hinges = state->second.hinges;
            hinges.erase(std::remove(hinges.begin(), hinges.end(), hinge), hinges.end());
            hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
            hinge->release();
            part->hinge = nullptr;
            part->hasHinge = false;
            part->detached = true;
            part->mode = PhysicsPartMode::Free;
        }
        else if(part->hinge)
        {
            const bool driven = part->mode == PhysicsPartMode::Driven;
            if(driven)
                part->hinge->setDriveForceLimit(part->driveMaxForce > 0.0f ?
                    part->driveMaxForce : 4000.0f);
            part->hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, driven);
        }
        part->actor->wakeUp();
        part->partNetNextSend = 0.0f;
        part->partNetActive = false;
    }
    SetAnimatedCollisionEnabled(state->second, state->second.collisionRequested);
    if(auto asset = GetPhysicsAssetDefinition(entity))
        RebuildAnimatedCollisionTraceMesh(entity, state->second, *asset);
    UpdateEntityAABB(entity);
    entity->RelinkEntity(FALSE);
}

bool CPhysicPhysX::DamagePhysicsPart( CBaseEntity *entity, const char *partName, float damage )
{
    if( !entity || !std::isfinite(damage) || damage <= 0.0f ) return false;
    PhysicsPartRuntime *part = FindPhysicsPart(entity, partName);
    if( !part ) return false;
    const auto stateIt = m_physicsAssetCollisionStates.find(entity->entindex());
    if( stateIt == m_physicsAssetCollisionStates.end() || stateIt->second.edict != entity->edict() ||
        stateIt->second.articulatedOverride || !stateIt->second.collisionRequested ) return false;
    return ApplyPhysicsPartDamageEvent(entity, *part, damage, 0.0f);
}

bool CPhysicPhysX::GetPhysicsPartWorldPose(CBaseEntity *entity,const char *partName,matrix4x4 &pose)
{
    PhysicsPartRuntime *part = FindPhysicsPart(entity,partName);
    if(!part || !part->actor) return false;
    // setKinematicTarget can be pending until the next PhysX step. Report
    // the latest animation sample immediately for kinematic parts.
    const PxTransform worldPose = part->owner == physicsasset::PartOwner::AnimatedProxy && part->sampledPose
        ? part->latestAnimatedPose : part->actor->getGlobalPose();
    pose = MatrixFromPxTransform(worldPose);
    return true;
}

// Resolve a stable game-facing part name (case-insensitive) into its one
// current actor/shape group. A bodygroup swap never changes that identity.
CPhysicPhysX::PhysicsPartRuntime *CPhysicPhysX::FindPhysicsPart( CBaseEntity *pEntity, const char *partName )
{
    if( !pEntity || !partName || !*partName ) return nullptr;
    auto state = m_physicsAssetCollisionStates.find( pEntity->entindex() );
    if( state == m_physicsAssetCollisionStates.end() ) return nullptr;
    for( PhysicsPartRuntime &part : state->second.parts )
        if( !Q_stricmp( part.name.c_str(), partName ) ) return &part;
    return nullptr;
}

// Internal collision owner tag. Gameplay transitions MUST use
// SetPhysicsPartMode, which also switches the actual actor kinematic state.
bool CPhysicPhysX::SetPhysicsPartOwnership( CBaseEntity *pEntity, const char *partName, physicsasset::PartOwner owner )
{
    PhysicsPartRuntime *part = FindPhysicsPart( pEntity, partName );
    if( !part ) return false;
    // Never bypass kinematic/dynamic state transition through a mere tag.
    if( owner == physicsasset::PartOwner::ArticulatedBody )
        return SetPhysicsPartMode(pEntity,partName,PhysicsPartMode::Free);
    if( part->owner == physicsasset::PartOwner::ArticulatedBody )
        return SetPhysicsPartMode(pEntity,partName,PhysicsPartMode::Animated);
    part->owner = owner;
    auto it = m_physicsAssetCollisionStates.find( pEntity->entindex() );
    SetAnimatedCollisionEnabled( it->second, it->second.collisionRequested );
    if( auto asset = GetPhysicsAssetDefinition( pEntity ) )
        RebuildAnimatedCollisionTraceMesh( pEntity, it->second, *asset );
    return true;
}

// Called only after a successful articulated-body activation/deactivation.
// A proxy and an articulated body may never own the same collision at once.
// One gate for *all* root actor collision flag writers. Bodygroup switches,
// generic collision toggles and ragdoll ownership changes must obey the same
// policy. Keep gameplay's requested state separate from temporary suspension.
void CPhysicPhysX::ApplyPhysicsAssetRootCollision( CBaseEntity *entity,
    PhysicsAssetCollisionState &state, bool enableWhenAllowed )
{
    if( !entity ) return;
    PxActor *actor = ActorFromEntity( entity );
    PxRigidActor *rigid = actor ? actor->is<PxRigidActor>() : nullptr;
    if( !rigid ) return;

    const bool pendingRestore = m_ragdollLegacyCollisionState.find(entity->entindex()) !=
        m_ragdollLegacyCollisionState.end();
    const physicsasset::RootCollisionAction action = physicsasset::ResolveRootCollision(
        state.collisionRequested, state.selectionHasGeometry,
        state.articulatedOverride, pendingRestore, enableWhenAllowed );
    if( action == physicsasset::RootCollisionAction::Preserve ) return;
    const bool active = action == physicsasset::RootCollisionAction::Enable;
    std::vector<PxShape *> shapes(rigid->getNbShapes());
    if( !shapes.empty() ) rigid->getShapes(shapes.data(), static_cast<PxU32>(shapes.size()));
    for( PxShape *shape : shapes )
    {
        if( !shape ) continue;
        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, active);
        shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, active);
    }
}

void CPhysicPhysX::SetArticulatedCollisionOwnership( CBaseEntity *pEntity, bool enabled )
{
    if( !pEntity ) return;
    auto it = m_physicsAssetCollisionStates.find( pEntity->entindex() );
    if( it == m_physicsAssetCollisionStates.end() ) return;
    PhysicsAssetCollisionState &state = it->second;
    state.articulatedOverride = enabled;
    for( PhysicsPartRuntime &part : state.parts )
    {
        // Full ragdoll activation supersedes per-prop hinge simulation. A
        // still-dynamic child must be parked before the proxy is re-enabled.
        if( part.actor && !part.actor->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC) )
        {
            part.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC,true);
            part.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY,true);
        }
        if( part.hinge )
            part.hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
        part.mode = PhysicsPartMode::Animated;
        part.hasTargetAngle = false;
        part.sampledPose = false;
        part.owner = enabled ? physicsasset::PartOwner::ArticulatedBody : physicsasset::PartOwner::AnimatedProxy;
        // Restore an already-detached part to its autonomous dynamic actor
        // after the temporary whole-asset/ragdoll collision override ends.
        if( !enabled && part.detached && part.actor )
        {
            part.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
            part.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,part.ccd);
            part.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, false);
            part.mode = PhysicsPartMode::Free;
            part.owner = physicsasset::PartOwner::ArticulatedBody;
            part.actor->wakeUp();
        }
    }
    // Suppress the root as well as the bone-bound children. On exit the
    // suspended legacy flags are restored before collision may be reenabled.
    ApplyPhysicsAssetRootCollision( pEntity, state, true );
    SetAnimatedCollisionEnabled( state, state.collisionRequested );
    std::shared_ptr<PhysicsAssetDef> def = GetPhysicsAssetDefinition( pEntity );
    if( def ) RebuildAnimatedCollisionTraceMesh( pEntity, state, *def );
}

void CPhysicPhysX::BuildAnimatedTraceKey(CBaseEntity *entity, const PhysicsAssetCollisionState &state,
    const PhysicsAssetDef &asset, performance::TraceMeshKey &key)
{
    model_t *model = (model_t *)MODEL_HANDLE(entity->pev->modelindex);
    key.model = model;
    key.modelCRC = model ? model->modelCRC : 0;
    key.definition = &asset;
    key.fingerprint = asset.collisionFingerprint;
    key.body = entity->pev->body;
    key.skin = entity->pev->skin;
    key.poses.clear();
    key.valid = model && model->type == mod_studio;
    const Vector origin = entity->GetAbsOrigin(), angles = entity->GetAbsAngles(), scale = entity->GetScale();
    for(int i = 0; i < 3; ++i)
    { key.origin[i] = origin[i]; key.angles[i] = angles[i]; key.scale[i] = scale[i]; }
    if(!key.valid) return;
    for(size_t i = 0; i < asset.collisions.size(); ++i)
    {
        const auto &section = asset.collisions[i];
        if(section.disabled || !CollisionSectionActive(section, model, entity->pev->body)) continue;
        bool available = section.bone.empty();
        for(const PhysicsPartRuntime &part : state.parts)
            for(const PhysicsPartShape &shape : part.shapes)
                if(shape.collisionIndex == i && part.actor) available = true;
        if(!available) continue;
        matrix4x4 pose;
        pose.Identity();
        if(!section.bone.empty())
        {
            const int bone = FindStudioBoneIndex(model, section.bone.c_str());
            if(bone < 0) { key.valid = false; continue; }
            bool physical = false;
            for(const PhysicsPartRuntime &part : state.parts)
                if(part.boneIndex == bone && part.actor && part.owner == physicsasset::PartOwner::ArticulatedBody)
                { pose = MatrixFromPxTransform(part.actor->getGlobalPose()); physical = true; break; }
            if(!physical) pose = MatrixFromPxTransform(StudioBoneWorldTransform(entity, bone));
        }
        key.poses.push_back(performance::MakeTracePose(static_cast<std::uint32_t>(i), pose));
    }
    for(size_t i = 0; i < state.parts.size(); ++i)
    {
        const PhysicsPartRuntime &part = state.parts[i];
        if(!part.actor || part.bodyIndex < 0 || size_t(part.bodyIndex) >= asset.bodies.size()) continue;
        bool active = false;
        for(const PhysicsPartShape &shape : part.shapes)
            if(shape.shape && shape.collisionIndex == asset.collisions.size() &&
                (shape.shape->getFlags() & (PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE)))
                active = true;
        if(!active) continue;
        const PxTransform pose = part.owner == physicsasset::PartOwner::AnimatedProxy && part.mode == PhysicsPartMode::Animated
            ? StudioBoneWorldTransform(entity, part.boneIndex) : part.actor->getGlobalPose();
        if(!pose.isValid()) { key.valid = false; continue; }
        key.poses.push_back(performance::MakeTracePose(
            performance::kPrimitivePose | static_cast<std::uint32_t>(i), MatrixFromPxTransform(pose)));
    }
    key.valid = key.valid && performance::FiniteTraceKey(key);
}

void CPhysicPhysX::RebuildAnimatedCollisionTraceMesh( CBaseEntity *pObject, PhysicsAssetCollisionState &state, const PhysicsAssetDef &asset )
{
    if( state.articulatedOverride )
    {
        state.animatedTraceMesh.reset();
        state.traceKey.valid = false;
        return;
    }
    BuildAnimatedTraceKey(pObject, state, asset, state.traceCandidate);
    if(state.animatedTraceMesh && state.animatedTraceMesh->GetMesh() && state.traceKey == state.traceCandidate)
    { ++m_performance.meshHits; return; }
    performance::ScopedTimer timer(m_performance.meshMilliseconds, p_speeds && p_speeds->value >= 2.0f);
    // The same actor owns each section before and after handoff. The GoldSrc
    // gameplay sweep representation follows its CURRENT pose in either mode.
    const auto sectionAvailable = [&]( size_t index ) -> bool {
        if( asset.collisions[index].bone.empty() ) return true;
        for( const PhysicsPartRuntime &part : state.parts )
            for( const PhysicsPartShape &shape : part.shapes )
                if( shape.collisionIndex == index ) return part.actor != nullptr;
        return false;
    };
    struct PrimitivePartMesh
    {
        const PhysicsPartRuntime *part;
        const physicsasset::BodyDef *body;
        std::vector<physicsasset::PrimitiveTriangle> triangles;
    };
    std::vector<PrimitivePartMesh> primitives;
    for( const PhysicsPartRuntime &part : state.parts )
    {
        if( !part.actor || part.bodyIndex < 0 ||
            static_cast<size_t>(part.bodyIndex) >= asset.bodies.size() ) continue;
        bool hasActivePrimitive = false;
        for( const PhysicsPartShape &shape : part.shapes )
            if( shape.shape && shape.collisionIndex == asset.collisions.size() &&
                (shape.shape->getFlags() & (PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE)))
                hasActivePrimitive = true;
        if( !hasActivePrimitive ) continue; // never duplicate a mesh-backed part
        PrimitivePartMesh entry{ &part, &asset.bodies[part.bodyIndex], {} };
        physicsasset::BuildPrimitiveTraceTriangles( entry.body->shape, entry.triangles );
        if( !entry.triangles.empty() ) primitives.push_back(std::move(entry));
    }
	bool hasBoneBound = false;
	for( const auto &collision : asset.collisions )
		if( !collision.bone.empty() ) { hasBoneBound = true; break; }
	if( !hasBoneBound && primitives.empty() )
	{
		state.animatedTraceMesh.reset();
		return;
	}

	model_t *renderModel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	if( !renderModel || renderModel->type != mod_studio )
	{
		state.animatedTraceMesh.reset();
		return;
	}

	size_t triangleCount = 0;
	for( size_t sectionIndex = 0; sectionIndex < asset.collisions.size(); ++sectionIndex )
	{
        const auto &collision = asset.collisions[sectionIndex];
		if( !sectionAvailable( sectionIndex ) || collision.disabled || !CollisionSectionActive( collision, renderModel, pObject->pev->body ) ) continue;
		triangleCount += collision.indices.size() / 3;
	}
	for( const auto &primitive : primitives ) triangleCount += primitive.triangles.size();
	if( triangleCount == 0 )
	{
		state.animatedTraceMesh.reset();
		return;
	}

    ++m_performance.meshBuilds;

	auto mesh = std::make_unique<CMeshDesc>();
	mesh->SetDebugName( pObject->GetModel() );
	mesh->SetModel( renderModel, pObject->pev->body, pObject->pev->skin );
	mesh->SetGeometryType( clipfile::GeometryType::CookedTriangleMesh );
    if( triangleCount > static_cast<size_t>(MAX_TRIANGLES_SOFT) ||
        !mesh->InitMeshBuild(static_cast<int>(triangleCount)) )
    {
        state.animatedTraceMesh.reset();
        return;
    }

	// The finished trace mesh is unscaled entity-local. SetMeshOrientation
    // applies GetScale() once. Never store an already-scaled bone pivot here.
    const matrix4x4 entityToWorld( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f );
    const Vector modelScale = pObject->GetScale();
	for( size_t sectionIndex = 0; sectionIndex < asset.collisions.size(); ++sectionIndex )
	{
        const auto &collision = asset.collisions[sectionIndex];
		if( !sectionAvailable( sectionIndex ) || collision.disabled || !CollisionSectionActive( collision, renderModel, pObject->pev->body ) ) continue;

		matrix4x4 boneToWorld;
        physicsasset::ScaledTraceBoneFrame traceBone;
		bool boneBound = !collision.bone.empty();
		if( boneBound )
		{
            const performance::TracePose *pose = performance::FindTracePose(state.traceCandidate,
                static_cast<std::uint32_t>(sectionIndex));
            if(!pose) continue;
            boneToWorld = matrix4x4(pose->matrix.data());
            if( !traceBone.Initialize( entityToWorld, boneToWorld, modelScale ) ) continue;
		}

		for( size_t i = 0; i + 2 < collision.indices.size(); i += 3 )
		{
			Vector triangle[3];
			for( int corner = 0; corner < 3; ++corner )
			{
				const std::uint32_t index = collision.indices[i + corner];
				if( index >= collision.vertices.size() ) continue;
				const physicsasset::Vec3 &v = collision.vertices[index];
				Vector local( v.x, v.y, v.z );
				if( boneBound )
				{
                    local = traceBone.ToTraceLocal( local );
				}
				triangle[corner] = local;
			}
			mesh->AddMeshTrinagle( triangle );
		}
	}

    // Primitive-only hinges have no authored collision section. Their actual
    // PhysX actor pose (including Free/Driven motion) owns the trace surface.
    // The shape-local offset/angles are applied before the shared I.10.2
    // bone-frame conversion; SetMeshOrientation applies entity scale once.
    for( const PrimitivePartMesh &primitive : primitives )
    {
        physicsasset::ScaledTraceBoneFrame frame;
        // Kinematic animation may be ahead of the last PhysX simulation
        // step. Use the same live Studio pivot as animated mesh-backed parts;
        // dynamic/Driven parts use their actual PhysX pose instead.
        const size_t partIndex = size_t(primitive.part - state.parts.data());
        const performance::TracePose *pose = performance::FindTracePose(state.traceCandidate,
            performance::kPrimitivePose | static_cast<std::uint32_t>(partIndex));
        if(!pose) continue;
        if( !frame.Initialize(entityToWorld,
            matrix4x4(pose->matrix.data()),modelScale) ) continue;
        const auto &body = *primitive.body;
        const matrix4x4 shapeToBone(Vector(body.offset.x,body.offset.y,body.offset.z),
            Vector(body.angles.x,body.angles.y,body.angles.z),1.0f);
        for( const auto &src : primitive.triangles )
        {
            Vector triangle[3];
            for( int i=0;i<3;++i )
            {
                const Vector local(src[i].x,src[i].y,src[i].z);
                triangle[i] = frame.ToTraceLocal(shapeToBone.VectorTransform(local));
            }
            mesh->AddMeshTrinagle(triangle);
        }
    }

	if( !mesh->FinishMeshBuild() )
	{
		mesh->FreeMeshBuild();
		return;
	}
	mesh->FreeMeshBuild();
	state.animatedTraceMesh = std::move( mesh );
    std::swap(state.traceKey, state.traceCandidate);
}

void CPhysicPhysX::BuildAnimatedCollisionActors( CBaseEntity *pObject, PhysicsAssetCollisionState &state, const PhysicsAssetDef &asset )
{
    if( !pObject || !m_pPhysics || !m_pScene || !m_pDefaultMaterial ) return;
    model_t *renderModel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
    if( !renderModel || renderModel->type != mod_studio ) return;

    // Group all collision sections of the same bone into ONE rigid actor.
    // Multiple convex pieces / bodygroup variants are shapes on that actor,
    // not independent rigid bodies. I.3 can attach a hinge to this actor,
    // without transferring animation ownership until I.4.
    for( const physicsasset::PartLayout &layout : physicsasset::BuildPartLayout( asset ) )
    {
        PhysicsPartRuntime part;
        part.name = layout.name;
        part.bodyIndex = layout.bodyIndex;
        part.initialMode = layout.initialMode;
        if( layout.bodyIndex >= 0 )
        {
            const auto &body = asset.bodies[layout.bodyIndex];
            part.driveStrength = body.driveStrength;
            part.driveDamping = body.driveDamping;
            part.driveMaxForce = body.driveMaxForce;
        }
        part.owner = state.articulatedOverride ? physicsasset::PartOwner::ArticulatedBody : physicsasset::PartOwner::AnimatedProxy;
        part.boneIndex = FindStudioBoneIndex( renderModel, layout.bone.c_str() );
        if( part.boneIndex < 0 )
        {
            ALERT( at_warning, "%s: physics part '%s' references missing bone '%s'\n",
                asset.sourcePath.c_str(), part.name.c_str(), layout.bone.c_str() );
            continue;
        }

        // A metadata-only body becomes a physical actor only when it is the
        // child of an authored hinge. Root/chassis metadata continues to use
        // the existing entity actor, avoiding a duplicate chassis collider.
        bool needsHingeBody = false;
        bool isHingeChild = false;
        if( layout.bodyIndex >= 0 )
            for( const auto &joint : asset.joints )
                if( joint.kind == physicsasset::JointKind::Hinge &&
                    !Q_stricmp( joint.child.c_str(), asset.bodies[layout.bodyIndex].name.c_str() ) )
                    isHingeChild = true;
        needsHingeBody = isHingeChild && layout.collisionIndices.empty();
        if( needsHingeBody )
        {
            const Vector entityScale = pObject->GetScale();
            float rigidScale = 1.0f;
            if( !physicsasset::RigidUniformScale({entityScale.x, entityScale.y, entityScale.z}, rigidScale) )
            {
                ALERT(at_warning, "%s: hinged primitive '%s' skipped: non-uniform or invalid scale\n",
                    asset.sourcePath.c_str(), part.name.c_str());
                state.parts.push_back(std::move(part));
                continue;
            }
        }
        if( !layout.collisionIndices.empty() || needsHingeBody )
        {
            part.actor = m_pPhysics->createRigidDynamic( StudioBoneWorldTransform( pObject, part.boneIndex ) );
            if( !part.actor ) continue;
            part.actor->setRigidBodyFlag( PxRigidBodyFlag::eKINEMATIC, true );
            part.actor->setActorFlag( PxActorFlag::eDISABLE_GRAVITY, true );
            part.actor->setSolverIterationCounts( k_SolverIterationCount );
            // The pointer is stable because this asset definition is cached for
            // the lifetime of the map. It is not a temporary PartLayout string.
            const char *stableName = (layout.bodyIndex >= 0) ?
                asset.bodies[layout.bodyIndex].name.c_str() : asset.collisions[layout.collisionIndices.front()].name.c_str();
            part.actor->setName( stableName );
            part.actor->userData = pObject->edict();
            const PxMeshScale scale( pObject->GetScale() );
            for( size_t collisionIndex : layout.collisionIndices )
            {
                const physicsasset::CollisionDef &collision = asset.collisions[collisionIndex];
                if( collision.disabled ) continue;
                PxShape *shape = nullptr;
                if( collision.type == physicsasset::CollisionGeometryType::Convex )
                {
                    if( PxConvexMesh *convex = ConvexMeshFromPhysicsAssetSection( asset, collisionIndex ) )
                        shape = PxRigidActorExt::createExclusiveShape( *part.actor, PxConvexMeshGeometry( convex, scale ), *m_pDefaultMaterial );
                }
                else
                {
                    if( PxTriangleMesh *tri = TriangleMeshFromPhysicsAssetSection( asset, collisionIndex ) )
                        shape = PxRigidActorExt::createExclusiveShape( *part.actor, PxTriangleMeshGeometry( tri, scale ), *m_pDefaultMaterial );
                }
                if( !shape ) continue;
                CollisionFilterData filterData( shape->getSimulationFilterData() );
                filterData.SetPhysicsAssetBody( true );
                filterData.SetPhysicsAssetOwner( static_cast<PxU32>( pObject->entindex() ) );
                shape->setSimulationFilterData( filterData.ToNativeType() );
                const bool active = state.collisionRequested && !state.articulatedOverride &&
                    CollisionSectionActive( collision, renderModel, pObject->pev->body );
                shape->setFlag( PxShapeFlag::eSIMULATION_SHAPE, active );
                shape->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, active );
                part.shapes.push_back( { collisionIndex, shape } );
            }
            // An interactive hinge can be authored without a separate v3
            // collision mesh. In that case its one actor receives the v4
            // primitive body shape; never spawn a second rigid actor.
            if( part.shapes.empty() && needsHingeBody && layout.bodyIndex >= 0 )
            {
                const auto &body = asset.bodies[layout.bodyIndex];
                PxShape *shape = nullptr;
                switch( body.shape.type )
                {
                case physicsasset::ShapeType::Box:
                    shape = PxRigidActorExt::createExclusiveShape( *part.actor,
                        PxBoxGeometry( PxVec3(body.shape.halfExtents.x * pObject->GetScale().x, body.shape.halfExtents.y * pObject->GetScale().x, body.shape.halfExtents.z * pObject->GetScale().x) ), *m_pDefaultMaterial ); break;
                case physicsasset::ShapeType::Sphere:
                    shape = PxRigidActorExt::createExclusiveShape( *part.actor,
                        PxSphereGeometry( body.shape.radius * pObject->GetScale().x ), *m_pDefaultMaterial ); break;
                case physicsasset::ShapeType::Capsule:
                    shape = PxRigidActorExt::createExclusiveShape( *part.actor,
                        PxCapsuleGeometry( body.shape.radius * pObject->GetScale().x, body.shape.halfHeight * pObject->GetScale().x ), *m_pDefaultMaterial ); break;
                }
                if( shape )
                {
                    matrix4x4 local( Vector(body.offset.x, body.offset.y, body.offset.z) * pObject->GetScale().x,
                        Vector(body.angles.x, body.angles.y, body.angles.z), 1.0f );
                    float values[16];
                    local.CopyToArray(values);
                    shape->setLocalPose( PxTransform(PxMat44(values)) );
                    CollisionFilterData filterData( shape->getSimulationFilterData() );
                    filterData.SetPhysicsAssetBody(true);
                    filterData.SetPhysicsAssetOwner(static_cast<PxU32>(pObject->entindex()));
                    shape->setSimulationFilterData(filterData.ToNativeType());
                    const bool active = state.collisionRequested && !state.articulatedOverride;
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, active);
                    shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, active);
                    // Sentinel denotes the body primitive, not a collision
                    // section. Its flags are maintained separately below.
                    part.shapes.push_back({ asset.collisions.size(), shape });
                }
            }
            bool convexOnly = true;
            for( size_t section : layout.collisionIndices )
                if( !asset.collisions[section].disabled &&
                    asset.collisions[section].type != physicsasset::CollisionGeometryType::Convex )
                    convexOnly = false;
            if( layout.bodyIndex >= 0 && !part.shapes.empty() )
            {
                const auto &body = asset.bodies[layout.bodyIndex];
                bool stabilityOK=physicsasset::ConfigureBodyStability(*part.actor,asset,body,pObject->GetScale().x);
                for(const auto &entry:part.shapes) stabilityOK=physicsasset::ConfigureBodyContact(*entry.shape,asset,body,pObject->GetScale().x)&&stabilityOK;
                part.ccd=physicsasset::ResolveBodySettings(asset,body).ccd;
                const bool massOK = PxRigidBodyExt::setMassAndUpdateInertia(*part.actor, body.mass);
                part.dynamicCompatible = convexOnly && stabilityOK && massOK;
                if( !part.dynamicCompatible && isHingeChild )
                    ALERT(at_warning, "%s: hinged part '%s' requires convex/primitive shapes and valid mass/inertia\n",
                        asset.sourcePath.c_str(), part.name.c_str());
            }
            if( part.shapes.empty() )
            {
                part.actor->release();
                part.actor = nullptr;
            }
            else
            {
                m_pScene->addActor( *part.actor );
            }
        }
        // Preserve metadata-only parts. Hinge children without v3 collision
        // receive one primitive shape on their existing part actor above.
        state.parts.push_back( std::move( part ) );
    }
    BuildInteractiveHingeJoints( pObject, state, asset );
    RebuildAnimatedCollisionTraceMesh( pObject, state, asset );
}

// Studio reference skeleton in MODEL space, independent of live animation.
// All hinge anchors/axes refer to this pose, so opening a door before its
// constraint is built cannot redefine the zero angle or relocate the pivot.
static bool ReferenceStudioBoneModelPose( CBaseEntity *entity, int boneIndex, PxTransform &result )
{
    if( !entity || boneIndex < 0 ) return false;
    studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR( entity->edict() );
    if( !hdr || boneIndex >= hdr->numbones ) return false;
    mstudiobone_t *bones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
    std::vector<matrix3x4> poses(hdr->numbones);
    std::vector<unsigned char> status(hdr->numbones, 0);
    std::function<bool(int)> visit = [&]( int index ) -> bool
    {
        if( index < 0 || index >= hdr->numbones || status[index] == 1 ) return false;
        if( status[index] == 2 ) return true;
        status[index] = 1;
        const Vector position = Vector( bones[index].value[0], bones[index].value[1], bones[index].value[2] ) * entity->GetScale().x;
        const Radian angles( bones[index].value[3], bones[index].value[4], bones[index].value[5] );
        const matrix3x4 local(position, angles);
        const int parent = bones[index].parent;
        if( parent >= 0 )
        {
            if( !visit(parent) ) return false;
            poses[index] = poses[parent].ConcatTransforms(local);
        }
        else poses[index] = local;
        status[index] = 2;
        return true;
    };
    if( !visit(boneIndex) ) return false;
    float values[16];
    poses[boneIndex].CopyToArray(values);
    result = PxTransform(PxMat44(values));
    return true;
}

// I.3 constructs constraints on I.2's SINGLE per-bone actor, with no
// extra physical door representation. The actors intentionally remain
// kinematic until I.4 implements pose/velocity ownership transfer.
void CPhysicPhysX::BuildInteractiveHingeJoints( CBaseEntity *entity,
    PhysicsAssetCollisionState &state, const PhysicsAssetDef &asset )
{
    if( !entity || !m_pPhysics ) return;
    if( asset.joints.empty() ) return;
    // Primitive geometry, Studio reference pivots and hinge local anchors
    // must share one uniform scale. Convex meshes already use PxMeshScale.
    const Vector entityScale = entity->GetScale();
    float uniformScale = 1.0f;
    if( !physicsasset::RigidUniformScale({entityScale.x, entityScale.y, entityScale.z}, uniformScale) )
    {
        ALERT(at_warning, "%s: interactive hinges require positive uniform model scale\n",
            asset.sourcePath.c_str());
        return;
    }
    PxActor *root = ActorFromEntity(entity);
    PxRigidActor *rootActor = root ? root->is<PxRigidActor>() : nullptr;
    for( const auto &def : asset.joints )
    {
        if( def.kind != physicsasset::JointKind::Hinge ) continue;
        PhysicsPartRuntime *child = nullptr;
        PhysicsPartRuntime *parent = nullptr;
        const int childBody = physicsasset::FindBody(asset, def.child.c_str());
        const int parentBody = physicsasset::FindBody(asset, def.parent.c_str());
        for( auto &part : state.parts )
        {
            if( part.bodyIndex == childBody && childBody >= 0 ) child = &part;
            if( part.bodyIndex == parentBody && parentBody >= 0 ) parent = &part;
        }
        if( !child || !child->actor || !child->dynamicCompatible )
        {
            ALERT(at_warning, "%s: hinge '%s' has no child rigid actor\n",
                asset.sourcePath.c_str(), def.name.c_str());
            continue;
        }
        // If the parent has no per-bone actor it must be the root Studio bone
        // and use the existing chassis actor. A non-root missing parent is a
        // configuration error, not an invitation to silently weld to chassis.
        PxRigidActor *parentActor = parent ? parent->actor : nullptr;
        if( !parentActor && parentBody >= 0 && rootActor )
        {
            // Root chassis bodies intentionally have no opt-in interactive
            // proxy actor. Resolve the root Studio bone from the .phys body
            // itself; BuildPartLayout excludes non-interactive root metadata.
            model_t *model = (model_t *)MODEL_HANDLE(entity->pev->modelindex);
            const int parentBoneIndex = model ? FindStudioBoneIndex(model,
                asset.bodies[parentBody].bone.c_str()) : -1;
            studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR(entity->edict());
            if( hdr && parentBoneIndex >= 0 && parentBoneIndex < hdr->numbones )
            {
                mstudiobone_t *bones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
                if( bones[parentBoneIndex].parent < 0 ) parentActor = rootActor;
            }
        }
        if( !parentActor || parentActor == child->actor )
        {
            ALERT(at_warning, "%s: hinge '%s' needs a valid root/part parent actor\n",
                asset.sourcePath.c_str(), def.name.c_str());
            continue;
        }
        // Derive the parent frame from the BIND skeleton; joint limits remain
        // anchored to the authored reference pose, never the current sequence.
        PxTransform referenceChild(PxIdentity), referenceParent(PxIdentity);
        if( !ReferenceStudioBoneModelPose(entity, child->boneIndex, referenceChild) ||
            (parent && parent->actor &&
             !ReferenceStudioBoneModelPose(entity, parent->boneIndex, referenceParent)) )
        {
            ALERT(at_warning, "%s: missing reference frame for hinge '%s'\n",
                asset.sourcePath.c_str(), def.name.c_str());
            continue;
        }
        PxJoint *built=physicsasset::CreateAssetJoint(*m_pPhysics,*parentActor,*child->actor,referenceParent,referenceChild,referenceChild,asset,def,uniformScale);
        PxRevoluteJoint *joint=built?built->is<PxRevoluteJoint>():nullptr;
        if( !joint )
        {
            ALERT(at_warning, "%s: failed to construct hinge '%s'\n",
                asset.sourcePath.c_str(), def.name.c_str());
            continue;
        }
        child->hasHinge = true;
        child->hinge = joint;
        child->releaseDamage = def.releaseDamage;
        child->breakDamage = def.breakDamage;
        child->breakImpulse = def.breakImpulse;
        state.hinges.push_back(joint);
    }
}

// I.6: target the animated bone through the real revolute joint motor.
// The same actor remains dynamic; no kinematic target or duplicate proxy is
// written while Driven owns the door. The motor supplies equal/opposite
// constraint reaction to its two attached actors.
void CPhysicPhysX::ApplyInteractivePartDrives( float dt )
{
    if( !m_pScene || !std::isfinite(dt) || dt <= 0.0f ) return;
    const float now = gpGlobals ? gpGlobals->time : 0.0f;
    for( auto &entry : m_physicsAssetCollisionStates )
    {
        PhysicsAssetCollisionState &state = entry.second;
        if( !state.edict || state.edict->free ) continue;
        for( PhysicsPartRuntime &part : state.parts )
        {
            if( part.mode != PhysicsPartMode::Driven || !part.hinge || !part.actor ) continue;
            bool activeShape = false;
            for( const PhysicsPartShape &shape : part.shapes )
                if( shape.shape && (shape.shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE) )
                    { activeShape = true; break; }
            if( state.articulatedOverride || !state.collisionRequested || !activeShape ||
                part.owner != physicsasset::PartOwner::ArticulatedBody || !part.sampledPose ||
                part.actor->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC) )
            {
                part.hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
                continue;
            }
            // Invalid/stale targets must not keep applying an old motor command.
            if( !std::isfinite(now) || now < part.latestSampleTime ||
                now - part.latestSampleTime > 0.25f )
            {
                part.hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
                continue;
            }
            PxRigidActor *parent = nullptr, *child = nullptr;
            part.hinge->getActors(parent, child);
            if( !parent || child != part.actor ) continue;
            const PxTransform parentFrame = parent->getGlobalPose() *
                part.hinge->getLocalPose(PxJointActorIndex::eACTOR0);
            const PxTransform targetFrame = part.latestAnimatedPose *
                part.hinge->getLocalPose(PxJointActorIndex::eACTOR1);
            if( !parentFrame.isValid() || !targetFrame.isValid() ) continue;
            // The authored hinge frame rotates around X; find target Y in the
            // parent's reference frame to get a signed revolute target angle.
            const PxVec3 y = parentFrame.q.rotateInv(
                targetFrame.q.rotate(PxVec3(0,1,0)));
            if( !y.isFinite() ) continue;
            const float unclamped = std::atan2(y.z, y.y);
            const PxJointAngularLimitPair limits = part.hinge->getLimit();
            const float target = PxClamp(unclamped, limits.lower, limits.upper);
            float targetRate = 0.0f;
            if( part.hasTargetAngle )
                targetRate = physicsasset::HingeTargetRate(target, part.lastTargetAngle,
                    now - part.targetSampleTime);
            if( !part.hasTargetAngle || now > part.targetSampleTime + 0.0001f )
            {
                part.lastTargetAngle = target;
                part.targetSampleTime = now;
                part.hasTargetAngle = true;
                part.targetAngularVelocity = targetRate;
            }
            const physicsasset::HingeDriveCommand drive = physicsasset::ComputeHingeDrive(
                target, part.hinge->getAngle(), part.targetAngularVelocity,
                part.hinge->getVelocity(), part.driveStrength, part.driveDamping,
                part.driveMaxForce > 0.0f ? part.driveMaxForce : 4000.0f);
            if( !drive.valid ) {
                part.hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, false);
                continue;
            }
            part.hinge->setDriveForceLimit(drive.forceLimit);
            part.hinge->setDriveVelocity(drive.velocity);
            part.hinge->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, true);
            if( std::fabs(drive.velocity) > 0.02f ) part.actor->wakeUp();
        }
    }
}

void CPhysicPhysX::SendInteractivePartPoses()
{
    if (!gmsgPartPose || !gpGlobals) return;
    const float now=gpGlobals->time;
    for (auto &entry : m_physicsAssetCollisionStates) {
        PhysicsAssetCollisionState &state=entry.second;
        if (!state.edict || state.edict->free) continue;
        CBaseEntity *entity=CBaseEntity::Instance(state.edict);
        if (!entity || entity->pev->modelindex<=0) continue;
        for (PhysicsPartRuntime &part : state.parts) {
            if (!part.actor || part.boneIndex<0 || part.boneIndex>=MAXSTUDIOBONES) continue;
            const bool active=!state.articulatedOverride && (part.mode!=PhysicsPartMode::Animated) &&
                part.owner==physicsasset::PartOwner::ArticulatedBody;
            const bool changed=part.partNetActive!=active || (active && part.partNetDetached!=part.detached);
            if (!changed && (!active || now<part.partNetNextSend)) continue;
            const PxTransform pose=part.actor->getGlobalPose();
            if (!pose.isValid() || !pose.p.isFinite() || !pose.q.isFinite()) continue;
            ++m_partSnapshotSequence;
            if (!m_partSnapshotSequence) ++m_partSnapshotSequence;
            MESSAGE_BEGIN(MSG_PVS,gmsgPartPose,entity->GetAbsOrigin());
                WRITE_ENTITY(entity->entindex());
                WRITE_BYTE(partnet::kVersion);
                WRITE_BYTE((active?0:partnet::kClear) | (part.detached?partnet::kDetached:0));
                WRITE_LONG(static_cast<int32_t>(m_partSnapshotSequence));
                WRITE_BYTE(part.boneIndex);
                WRITE_FLOAT(pose.p.x); WRITE_FLOAT(pose.p.y); WRITE_FLOAT(pose.p.z);
                WRITE_FLOAT(pose.q.x); WRITE_FLOAT(pose.q.y);
                WRITE_FLOAT(pose.q.z); WRITE_FLOAT(pose.q.w);
            MESSAGE_END();
            part.partNetActive=active;
            part.partNetDetached=part.detached;
            part.partNetNextSend=now+(active && !part.actor->isSleeping()?
                partnet::kActiveInterval:partnet::kSleepInterval);
        }
    }
}

void CPhysicPhysX::UpdateAnimatedCollisionActors()
{
	for( auto it = m_physicsAssetCollisionStates.begin(); it != m_physicsAssetCollisionStates.end(); )
	{
		PhysicsAssetCollisionState &state = it->second;
		if( !state.edict || state.edict->free )
		{
			ReleaseAnimatedCollisionActors( state );
			it = m_physicsAssetCollisionStates.erase( it );
			continue;
		}
		CBaseEntity *entity = CBaseEntity::Instance( state.edict );
		std::shared_ptr<PhysicsAssetDef> asset = entity ? GetPhysicsAssetDefinition( entity ) : nullptr;
		if( !entity || !asset )
		{
			ReleaseAnimatedCollisionActors( state );
			it = m_physicsAssetCollisionStates.erase( it );
			continue;
		}
		model_t *renderModel = (model_t *)MODEL_HANDLE( entity->pev->modelindex );
		if( renderModel != state.renderModel )
		{
			// TrackPhysicsAssetCollision owns teardown/replacement. Advance the
			// std::map iterator before it erases this entry, and retain the old
			// gameplay-requested collision state across model replacement.
			++it;
			TrackPhysicsAssetCollision( entity );
			continue;
		}

		for( PhysicsPartRuntime &part : state.parts )
        {
            if( part.actor && (part.owner == physicsasset::PartOwner::AnimatedProxy ||
                part.mode == PhysicsPartMode::Driven) &&
                !state.articulatedOverride && part.boneIndex >= 0 )
                SampleInteractivePart( entity, part );
        }
		SetAnimatedCollisionEnabled( state, state.collisionRequested );
		RebuildAnimatedCollisionTraceMesh( entity, state, *asset );
		const Vector oldMins = entity->pev->absmin;
		const Vector oldMaxs = entity->pev->absmax;
		UpdateEntityAABB( entity );
		// Animated doors can move outside their original BSP area node. Updating
		// pev bounds alone does not move the edict between engine area links.
		if( oldMins != entity->pev->absmin || oldMaxs != entity->pev->absmax )
			entity->RelinkEntity( FALSE );
		++it;
	}
}

void CPhysicPhysX::TrackPhysicsAssetCollision( CBaseEntity *pObject )
{
	if( !pObject || !pObject->edict() )
		return;
	const int entIndex = pObject->entindex();
	model_t *renderModel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	std::shared_ptr<PhysicsAssetDef> asset = GetPhysicsAssetDefinition( pObject );
    // A v4 metadata-only interactive part still needs a runtime registry entry.
    // Its lack of collision sections MUST NOT disable the original root actor.
    const bool hasCollisionSections = asset && physicsasset::HasCollisionGeometry( *asset );
    const bool hasInteractiveParts = asset && !physicsasset::BuildPartLayout( *asset ).empty();
	if( !renderModel || renderModel->type != mod_studio || !asset || ( !hasCollisionSections && !hasInteractiveParts ))
	{
		auto old = m_physicsAssetCollisionStates.find( entIndex );
		if( old != m_physicsAssetCollisionStates.end() )
		{
			ReleaseAnimatedCollisionActors( old->second );
			m_physicsAssetCollisionStates.erase( old );
		}
		return;
	}

	auto old = m_physicsAssetCollisionStates.find( entIndex );
	// The root shape can be deliberately disabled by a bodygroup variant while
	// an animated child remains active. Rebuilding from CheckCollision(root)
	// would mistake that for a gameplay request to disable the whole asset.
	const bool preserveRequested = old != m_physicsAssetCollisionStates.end();
	const bool requestedCollision = preserveRequested ? old->second.collisionRequested : true;
    const bool preserveArticulated = preserveRequested ? old->second.articulatedOverride : IsRagdoll( pObject );
	if( preserveRequested )
	{
		ReleaseAnimatedCollisionActors( old->second );
		m_physicsAssetCollisionStates.erase( old );
	}

	PhysicsAssetCollisionState state;
	state.edict = pObject->edict();
	state.renderModel = renderModel;
	state.renderBody = pObject->pev->body;
	state.selectionHasGeometry = !hasCollisionSections || PhysicsAssetCollisionSelectionHasGeometry( pObject, *asset );
	state.collisionRequested = requestedCollision;
    state.articulatedOverride = preserveArticulated;
	if( !preserveRequested )
	{
		if( PxActor *actor = ActorFromEntity( pObject ) )
		{
			if( PxRigidActor *rigid = actor->is<PxRigidActor>() )
				state.collisionRequested = CheckCollision( rigid );
		}
	}

	auto inserted = m_physicsAssetCollisionStates.emplace( entIndex, std::move( state ) );
	PhysicsAssetCollisionState &stored = inserted.first->second;
	BuildAnimatedCollisionActors( pObject, stored, *asset );

	// Mark the root .phys shape with the same owner as its bone-bound children
	// so PhysX suppresses chassis/door self-collision while preserving contacts
	// against every other entity. Preserve existing flags such as conveyor.
    // Do not retag the normal PrimeXT root actor for metadata-only v4 assets.
	if( hasCollisionSections ) if( PxActor *actor = ActorFromEntity( pObject ) )
	{
		if( PxRigidActor *rigid = actor->is<PxRigidActor>() )
		{
			PxShape *rootShape = NULL;
			if( rigid->getShapes( &rootShape, 1 ) == 1 && rootShape )
			{
				CollisionFilterData filterData( rootShape->getSimulationFilterData() );
				filterData.SetPhysicsAssetBody( true );
				filterData.SetPhysicsAssetOwner( static_cast<PxU32>( pObject->entindex() ) );
				rootShape->setSimulationFilterData( filterData.ToNativeType() );
			}
		}
	}

	// A v2/v3 bodygroup may intentionally select an empty/disabled root collision
	// variant. Keep the root actor as a placeholder, but only bone-bound child
	// actors remain collision-active in that state.
	if( !stored.selectionHasGeometry )
	{
		PxActor *actor = ActorFromEntity( pObject );
		PxRigidActor *rigid = actor ? actor->is<PxRigidActor>() : NULL;
		PxShape *shape = NULL;
		if( rigid && rigid->getShapes( &shape, 1 ) == 1 && shape )
		{
			shape->setFlag( PxShapeFlag::eSIMULATION_SHAPE, false );
			shape->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, false );
		}
	}
	// A state rebuilt during an active ragdoll must not wake the root collider.
    // Metadata-only assets otherwise keep their existing legacy root flags.
    ApplyPhysicsAssetRootCollision( pObject, stored, false );
	SetAnimatedCollisionEnabled( stored, stored.collisionRequested );
	UpdateEntityAABB( pObject );
}

void CPhysicPhysX::UpdateCollisionBodygroup( CBaseEntity *pEntity )
{
	if( !pEntity || !m_pScene || !pEntity->m_pUserData )
		return;

	const int entIndex = pEntity->entindex();
	auto it = m_physicsAssetCollisionStates.find( entIndex );
	if( it == m_physicsAssetCollisionStates.end() )
	{
		TrackPhysicsAssetCollision( pEntity );
		it = m_physicsAssetCollisionStates.find( entIndex );
		if( it == m_physicsAssetCollisionStates.end() ) return;
	}

	model_t *currentRenderModel = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( currentRenderModel != it->second.renderModel )
	{
		TrackPhysicsAssetCollision( pEntity );
		it = m_physicsAssetCollisionStates.find( entIndex );
		if( it == m_physicsAssetCollisionStates.end() ) return;
	}

	PhysicsAssetCollisionState &state = it->second;
	if( state.renderBody == pEntity->pev->body )
		return;

	PxActor *pActor = ActorFromEntity( pEntity );
	PxRigidActor *pRigidActor = pActor ? pActor->is<PxRigidActor>() : NULL;
    if(!pRigidActor) return;
    // A zero-shape root is a valid placeholder for independently active
    // children. Keep their bodygroup flags and spatial bounds up to date.
    // The absent root cannot accept a replacement mesh until rebuilt.
    if(pRigidActor->getNbShapes()==0) {
        std::shared_ptr<PhysicsAssetDef> childAsset=GetPhysicsAssetDefinition(pEntity);
        if(!childAsset) return;
        state.renderBody=pEntity->pev->body;
        state.selectionHasGeometry=false;
        SetAnimatedCollisionEnabled(state,state.collisionRequested);
        RebuildAnimatedCollisionTraceMesh(pEntity,state,*childAsset);
        UpdateEntityAABB(pEntity);
        pEntity->RelinkEntity(FALSE);
        return;
    }
    if(pRigidActor->getNbShapes()!=1) {
        ALERT(at_warning,"PhysX: can't switch .phys root bodygroup for %s; expected one rigid shape\n",
              pEntity->GetClassname());
        return;
    }

	std::shared_ptr<PhysicsAssetDef> asset = GetPhysicsAssetDefinition( pEntity );
    if( !asset )
    {
        ReleaseAnimatedCollisionActors( it->second );
        m_physicsAssetCollisionStates.erase( it );
        return;
    }
    if( !physicsasset::HasCollisionGeometry( *asset ) )
    {
        // The registry may contain interactive v4 body metadata only.
        // Bodygroup selection does not alter its normal legacy root geometry.
        state.renderBody = pEntity->pev->body;
        return;
    }

	PxShape *pShape = NULL;
	if( pRigidActor->getShapes( &pShape, 1 ) != 1 || !pShape )
		return;

	const bool selectionHasGeometry = PhysicsAssetCollisionSelectionHasGeometry( pEntity, *asset );
	if( !selectionHasGeometry )
	{
		pShape->setFlag( PxShapeFlag::eSIMULATION_SHAPE, false );
		pShape->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, false );
		if( PxRigidDynamic *pDynamic = pRigidActor->is<PxRigidDynamic>() )
			pDynamic->wakeUp();
		state.renderBody = pEntity->pev->body;
		state.selectionHasGeometry = false;
        ApplyPhysicsAssetRootCollision( pEntity, state, false );
		SetAnimatedCollisionEnabled( state, state.collisionRequested );
		RebuildAnimatedCollisionTraceMesh( pEntity, state, *asset );
		UpdateEntityAABB( pEntity );
		pEntity->RelinkEntity( FALSE );
		return;
	}

	PxMeshScale scale( pEntity->GetScale() );
	if( pEntity->m_iActorType == ACTOR_DYNAMIC )
	{
		PxConvexMesh *pMesh = ConvexMeshFromPhysicsAsset( pEntity, *asset );
		if( !pMesh ) return;
		pShape->setGeometry( PxConvexMeshGeometry( pMesh, scale ));
	}
	else
	{
		PxTriangleMesh *pMesh = TriangleMeshFromPhysicsAsset( pEntity, *asset );
		if( !pMesh ) return;
		pShape->setGeometry( PxTriangleMeshGeometry( pMesh, scale ));
	}

    // The current bodygroup may change while a ragdoll is active. Never
    // reenable the root proxy from collisionRequested alone (H review B4).
    // Assign the selected geometry first, then apply authoritative ownership.
    state.selectionHasGeometry = true;
    ApplyPhysicsAssetRootCollision( pEntity, state, true );
	if( PxRigidDynamic *pDynamic = pRigidActor->is<PxRigidDynamic>() )
	{
		float density = pEntity->GetDensity();
		if( density <= 0.0f ) density = k_DefaultDensity;
		PxRigidBodyExt::updateMassAndInertia( *pDynamic, density );
		pDynamic->wakeUp();
	}
	state.renderBody = pEntity->pev->body;
	state.selectionHasGeometry = true;
	SetAnimatedCollisionEnabled( state, state.collisionRequested );
	RebuildAnimatedCollisionTraceMesh( pEntity, state, *asset );
	UpdateEntityAABB( pEntity );
	pEntity->RelinkEntity( FALSE );
}

void CPhysicPhysX::UpdatePhysicsAssetCollisionBodygroups()
{
	for( auto it = m_physicsAssetCollisionStates.begin(); it != m_physicsAssetCollisionStates.end(); )
	{
		edict_t *pEdict = it->second.edict;
		if( !pEdict || pEdict->free )
		{
			ReleaseAnimatedCollisionActors( it->second );
			it = m_physicsAssetCollisionStates.erase( it );
			continue;
		}
		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if( !pEntity || !pEntity->m_pUserData )
		{
			ReleaseAnimatedCollisionActors( it->second );
			it = m_physicsAssetCollisionStates.erase( it );
			continue;
		}
		++it;
		UpdateCollisionBodygroup( pEntity );
	}
}

// Build the visible Studio model at the entity's current server animation pose.
// The normal CMeshDesc::StudioConstructMesh cache intentionally uses sequence 0 /
// frame 0, which is correct for static material lookup but wrong for an animated
// prop whose .phys collision follows bones (for example an open car door).
static bool BuildCurrentStudioRenderMesh( CBaseEntity *pEntity, CMeshDesc &mesh )
{
	if( !pEntity ) return false;
	model_t *model = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( !model || model->type != mod_studio ) return false;
	studiohdr_t *header = (studiohdr_t *)model->cache.data;
	if( !header || header->numbones <= 0 ) return false;

	int triangleCount = 0;
	for( int bodyPartIndex = 0; bodyPartIndex < header->numbodyparts; ++bodyPartIndex )
	{
		mstudiobodyparts_t *bodyPart = (mstudiobodyparts_t *)((byte *)header + header->bodypartindex) + bodyPartIndex;
		if( bodyPart->nummodels <= 0 || bodyPart->base <= 0 ) continue;
		const int modelIndex = ( pEntity->pev->body / bodyPart->base ) % bodyPart->nummodels;
		mstudiomodel_t *subModel = (mstudiomodel_t *)((byte *)header + bodyPart->modelindex) + modelIndex;
		for( int meshIndex = 0; meshIndex < subModel->nummesh; ++meshIndex )
		{
			mstudiomesh_t *studioMesh = (mstudiomesh_t *)((byte *)header + subModel->meshindex) + meshIndex;
			short *commands = (short *)((byte *)header + studioMesh->triindex);
			while( int count = *commands++ )
			{
				count = count < 0 ? -count : count;
				if( count >= 3 ) triangleCount += count - 2;
				commands += count * 4;
			}
		}
	}
	if( triangleCount <= 0 ) return false;

	mesh.SetDebugName( pEntity->GetModel() );
	mesh.SetModel( model, pEntity->pev->body, pEntity->pev->skin );
	mesh.SetGeometryType( clipfile::GeometryType::Original );
	if( !mesh.InitMeshBuild( triangleCount ) ) return false;

	const matrix4x4 entityToWorld( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f );
	std::vector<matrix4x4> boneToWorld( header->numbones );
	for( int bone = 0; bone < header->numbones; ++bone )
	{
		Vector origin, angles;
		GET_BONE_POSITION( pEntity->edict(), bone, origin, angles );
		boneToWorld[bone] = matrix4x4( origin, angles, 1.0f );
	}

	bool addedAny = false;
	for( int bodyPartIndex = 0; bodyPartIndex < header->numbodyparts; ++bodyPartIndex )
	{
		mstudiobodyparts_t *bodyPart = (mstudiobodyparts_t *)((byte *)header + header->bodypartindex) + bodyPartIndex;
		if( bodyPart->nummodels <= 0 || bodyPart->base <= 0 ) continue;
		const int modelIndex = ( pEntity->pev->body / bodyPart->base ) % bodyPart->nummodels;
		mstudiomodel_t *subModel = (mstudiomodel_t *)((byte *)header + bodyPart->modelindex) + modelIndex;
		Vector *studioVertices = (Vector *)((byte *)header + subModel->vertindex);
		byte *vertexBones = (byte *)header + subModel->vertinfoindex;

		for( int meshIndex = 0; meshIndex < subModel->nummesh; ++meshIndex )
		{
			mstudiomesh_t *studioMesh = (mstudiomesh_t *)((byte *)header + subModel->meshindex) + meshIndex;
			short *commands = (short *)((byte *)header + studioMesh->triindex);
			while( int rawCount = *commands++ )
			{
				const bool strip = rawCount > 0;
				const int count = rawCount < 0 ? -rawCount : rawCount;
				std::vector<mvert_t> primitive;
				primitive.reserve( count );
				for( int v = 0; v < count; ++v, commands += 4 )
				{
					mvert_t out = {};
					const int vertexIndex = commands[0];
					if( vertexIndex < 0 || vertexIndex >= subModel->numverts )
						continue;
					const int boneIndex = vertexBones[vertexIndex];
					if( boneIndex < 0 || boneIndex >= header->numbones )
						continue;
					const Vector world = boneToWorld[boneIndex].VectorTransform( studioVertices[vertexIndex] );
					out.point = entityToWorld.VectorITransform( world );
					// Material lookup only requires skinref. TraceMesh's server-side masked
					// texture pixel test currently has no pixel source, so UVs do not affect
					// this effects-only trace.
					out.st[0] = out.st[1] = 0.0f;
					primitive.push_back( out );
				}
				if( primitive.size() < 3 ) continue;

				mvert_t triangle[3];
				triangle[0] = primitive[0];
				triangle[1] = primitive[1];
				triangle[2] = primitive[2];
				addedAny |= mesh.AddMeshTrinagle( triangle, studioMesh->skinref );
				for( size_t v = 3; v < primitive.size(); ++v )
				{
					if( strip )
					{
						const int vertexState = static_cast<int>( v ) + 1;
						if( vertexState & 1 )
						{
							triangle[0] = primitive[v - 2];
							triangle[1] = primitive[v - 1];
						}
						else
						{
							triangle[0] = primitive[v - 1];
							triangle[1] = primitive[v - 2];
						}
						triangle[2] = primitive[v];
					}
					else
					{
						triangle[0] = primitive[0];
						triangle[1] = primitive[v - 1];
						triangle[2] = primitive[v];
					}
					addedAny |= mesh.AddMeshTrinagle( triangle, studioMesh->skinref );
				}
			}
		}
	}

	if( !addedAny || !mesh.FinishMeshBuild() )
	{
		mesh.FreeMeshBuild();
		return false;
	}
	mesh.FreeMeshBuild();
	return true;
}

// Resolve visual impact data for an entity whose .phys asset owns collision.
// PhysicsAsset remains authoritative; this function only projects the
// already-confirmed impact onto the visible render model so material sounds
// and studio decals come from the surface the player actually sees.
bool CPhysicPhysX::ResolveRenderImpact( CBaseEntity *pEntity, const Vector &start, const Vector &end,
	Vector &impactPoint, Vector &impactNormal, std::uint32_t &materialHash )
{
	if( !pEntity )
		return false;

	model_t *renderModel = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( !renderModel || renderModel->type != mod_studio )
		return false;

	// Without .phys collision geometry, PrimeXT's original material-trace behavior
	// remains exact and there is nothing to refine.
	if( !HasPhysicsAssetCollision( pEntity ))
		return false;

	// Use the current Studio pose first. This is required for v3 bone-bound
	// collision: an open door must resolve effects against the open rendered
	// door, not sequence-0/frame-0. Fall back to PrimeXT's cached reference
	// mesh if the live mesh cannot be constructed.
	CMeshDesc currentPoseMesh;
	CMeshDesc *renderMesh = nullptr;
	if( BuildCurrentStudioRenderMesh( pEntity, currentPoseMesh ))
		renderMesh = &currentPoseMesh;
	else
	{
		auto &meshDescFactory = CMeshDescFactory::Instance();
		CMeshDesc &referenceMesh = meshDescFactory.CreateObject( pEntity->pev->modelindex,
			pEntity->pev->body, pEntity->pev->skin, clipfile::GeometryType::Original );
		if( !referenceMesh.GetMesh() )
			referenceMesh.StudioConstructMesh();
		if( !referenceMesh.GetMesh() )
			return false;
		renderMesh = &referenceMesh;
	}

	if( ( end - start ).Length() <= 0.001f )
		return false;

	// The proxy already confirmed that this entity was hit. For effects only,
	// trace the complete original bullet ray against the visible MDL and take
	// its first surface contact. This does not alter the authoritative collision
	// result; it only supplies the visual impact point/normal/material.
	trace_t visualTrace;
	TraceMesh visualTracer;
	visualTracer.SetTraceMesh( renderMesh->GetMesh(), renderMesh->GetHeadNode(),
		pEntity->pev->modelindex, renderMesh->GetBody(), renderMesh->GetSkin() );
	visualTracer.SetMeshOrientation( pEntity->pev->origin, pEntity->pev->angles, pEntity->GetScale() );
	visualTracer.SetupTrace( start, g_vecZero, g_vecZero, end, &visualTrace );
	if( !visualTracer.DoTrace() || ( visualTrace.fraction >= 1.0f && !visualTrace.startsolid ))
		return false;

	impactPoint = visualTrace.endpos;
	impactNormal = visualTrace.plane.normal;
	materialHash = COM_GetMaterialHash( visualTracer.GetLastHitSurface() );
	return true;
}

physx::PxBounds3 CPhysicPhysX::GetIntersectionBounds(const physx::PxBounds3 &a, const physx::PxBounds3 &b)
{
	if (!a.intersects(b)) {
		return PxBounds3::empty();
	}

	PxBounds3 result;
	result.minimum.x = std::max(a.minimum.x, b.minimum.x);
	result.minimum.y = std::max(a.minimum.y, b.minimum.y);
	result.minimum.z = std::max(a.minimum.z, b.minimum.z);
	result.maximum.x = std::min(a.maximum.x, b.maximum.x);
	result.maximum.y = std::min(a.maximum.y, b.maximum.y);
	result.maximum.z = std::min(a.maximum.z, b.maximum.z);
	return result;
}

CBaseEntity *CPhysicPhysX :: EntityFromActor( PxActor *pObject )
{
	if( !pObject || !pObject->userData )
		return NULL;

	return CBaseEntity::Instance( (edict_t *)pObject->userData );
}

bool CPhysicPhysX::CheckCollision(physx::PxRigidActor *pActor)
{
	if( !pActor || pActor->getNbShapes() == 0 )
		return false;

	std::vector<PxShape*> shapes;
	shapes.resize(pActor->getNbShapes());
	pActor->getShapes(shapes.data(), static_cast<PxU32>(shapes.size()));
	for (PxShape *shape : shapes)
	{
		if (shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE) {
			return true;
		}
	}
	return false;
}

void CPhysicPhysX::ToggleCollision(physx::PxRigidActor *pActor, bool enabled)
{
	if( !pActor )
		return;

	// Remember the gameplay-requested collision state separately from whether
	// the selected root/bodygroup variant actually has geometry. Bone-bound v3
	// child actors receive the same gameplay enable/disable request.
	const bool requestedEnabled = enabled;
	CBaseEntity *pEntity = EntityFromActor( pActor );
	if( pEntity )
	{
		auto state = m_physicsAssetCollisionStates.find( pEntity->entindex() );
		if( state != m_physicsAssetCollisionStates.end() && ActorFromEntity( pEntity ) == pActor )
		{
			state->second.collisionRequested = requestedEnabled;
			SetAnimatedCollisionEnabled( state->second, requestedEnabled );
            // Also respect the ownership/suspension gate for *generic* toggles
            // (MoveCharacter, MoveKinematic and EnableCollision).
            const bool pendingRestore = m_ragdollLegacyCollisionState.find(pEntity->entindex()) !=
                m_ragdollLegacyCollisionState.end();
            enabled = physicsasset::ResolveRootCollision(
                requestedEnabled, state->second.selectionHasGeometry,
                state->second.articulatedOverride, pendingRestore, true ) ==
                physicsasset::RootCollisionAction::Enable;
		}
	}

	std::vector<PxShape*> shapes;
	shapes.resize(pActor->getNbShapes());
	pActor->getShapes(shapes.data(), static_cast<PxU32>(shapes.size()));

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: ToggleCollision( actor = %x, state = %s )\n", pActor, enabled ? "true" : "false");
	}

	for( PxShape *shape : shapes )
	{
		const PxShapeFlags flags = shape->getFlags();
		// Simulation and query flags can legitimately diverge (ragdoll/legacy
		// traces, or a previously disabled bodygroup variant). Updating the
		// query flag only when simulation changes can leave an invisible hit.
		if( static_cast<bool>( flags & PxShapeFlag::eSIMULATION_SHAPE ) != enabled )
			shape->setFlag( PxShapeFlag::eSIMULATION_SHAPE, enabled );
		if( static_cast<bool>( flags & PxShapeFlag::eSCENE_QUERY_SHAPE ) != enabled )
			shape->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, enabled );
	}
}

void *CPhysicPhysX :: CreateBodyFromEntity( CBaseEntity *pObject )
{
	PxConvexMesh *pCollision = ConvexMeshFromEntity(pObject);
	if (!pCollision)
		return NULL;

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxMeshScale scale(pObject->GetScale());
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxConvexMeshGeometry(pCollision, scale), *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT(at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	float mat[16];
	float maxVelocity = CVAR_GET_FLOAT("sv_maxvelocity");
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);
	PxTransform pose = PxTransform(PxMat44(mat));

	pActor->setGlobalPose(pose);
	pActor->setName(pObject->GetClassname());
	pActor->setSolverIterationCounts(k_SolverIterationCount);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
	pActor->setLinearVelocity(pObject->GetLocalVelocity());
	pActor->setAngularVelocity(pObject->GetLocalAvelocity());
	pActor->setMaxLinearVelocity(maxVelocity);
	pActor->setMaxAngularVelocity(720.0);
	pActor->userData = pObject->edict();

	float density = pObject->GetDensity();
	if (density <= 0.0f)
		density = k_DefaultDensity;

	PxRigidBodyExt::updateMassAndInertia(*pActor, density);

	m_pScene->addActor(*pActor);
	pObject->m_iActorType = ACTOR_DYNAMIC;
	pObject->m_pUserData = pActor;
	TrackPhysicsAssetCollision( pObject );
	return pActor;
}

/*
=================
CreateBoxFromEntity

used for characters: clients and monsters
=================
*/
void *CPhysicPhysX :: CreateBoxFromEntity( CBaseEntity *pObject )
{
	PxBoxGeometry boxGeometry;
	boxGeometry.halfExtents = pObject->pev->size * k_PaddingFactor;

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, boxGeometry, *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT( at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	Vector origin = (pObject->IsMonster()) ? Vector( 0, 0, pObject->pev->maxs.z / 2.0f ) : g_vecZero;
	origin += pObject->GetAbsOrigin();
	PxTransform pose = PxTransform(origin);

	pActor->setName(pObject->GetClassname());
	pActor->setGlobalPose(pose);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	pActor->userData = pObject->edict();
	m_pScene->addActor(*pActor);

	pObject->m_iActorType = ACTOR_CHARACTER;
	pObject->m_pUserData = pActor;

	return pActor;
}

void *CPhysicPhysX::CreateKinematicBodyFromEntity(CBaseEntity *pObject)
{
	PxTriangleMesh *pCollision = TriangleMeshFromEntity(pObject);
	if (!pCollision)
		return NULL;

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	if (!pActor)
	{
		ALERT(at_error, "failed to create kinematic from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	if (!UTIL_CanRotate(pObject))
	{
		// entity missed origin-brush
		pActor->setRigidDynamicLockFlags(
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_X |
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y |
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z);
	}

	float mat[16];
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);

	PxTransform pose = PxTransform(PxMat44(mat));
	pActor->setName(pObject->GetClassname());
	pActor->setGlobalPose(pose);
	pActor->setSolverIterationCounts(k_SolverIterationCount);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	pActor->userData = pObject->edict();

	PxMeshScale scale(pObject->GetScale());
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(pCollision, scale), *m_pDefaultMaterial);
	m_pScene->addActor(*pActor);
	pObject->m_iActorType = ACTOR_KINEMATIC;
	pObject->m_pUserData = pActor;
	TrackPhysicsAssetCollision( pObject );
	return pActor;
}

void *CPhysicPhysX :: CreateStaticBodyFromEntity( CBaseEntity *pObject )
{
	PxTriangleMesh *pCollision = TriangleMeshFromEntity( pObject );
	if( !pCollision )
		return NULL;

	float mat[16];
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);

	bool conveyorEntity = FBitSet(pObject->pev->flags, FL_CONVEYOR);
	PxTransform pose = PxTransform(PxMat44(mat));
	PxMeshScale scale(pObject->GetScale());
	PxMaterial *material = conveyorEntity ? m_pConveyorMaterial : m_pDefaultMaterial;
	PxRigidStatic *pActor = m_pPhysics->createRigidStatic(pose);
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(pCollision, scale), *material);

	if( !pActor )
	{
		ALERT( at_error, "failed to create static from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	if (conveyorEntity)
	{
		CollisionFilterData filterData;
		filterData.SetConveyorFlag(true);
		pShape->setSimulationFilterData(filterData.ToNativeType());
	}

	pActor->setName( pObject->GetClassname( ));
	pActor->userData = pObject->edict();
	m_pScene->addActor(*pActor);

	pObject->m_iActorType = ACTOR_STATIC;
	pObject->m_pUserData = pActor;
	TrackPhysicsAssetCollision( pObject );

	return pActor;
}

void *CPhysicPhysX::CreateTriggerFromEntity(CBaseEntity *pEntity)
{
	PxBoxGeometry boxGeometry;
	boxGeometry.halfExtents = pEntity->pev->size / 2.0f;
	Vector centerOrigin = pEntity->pev->mins + pEntity->pev->size / 2.0f;
	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(centerOrigin));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, boxGeometry, *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT( at_error, "failed to create trigger actor from entity %s\n", pEntity->GetClassname());
		return NULL;
	}

	pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
	pShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
	pActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
	pActor->setName(pEntity->GetClassname());
	pActor->userData = pEntity->edict();
	m_pScene->addActor(*pActor);

	pEntity->m_iActorType = ACTOR_TRIGGER;
	pEntity->m_pUserData = pActor;

	return pActor;
}

void *CPhysicPhysX :: CreateVehicle( CBaseEntity *pObject, string_t scriptName )
{
#if defined (HAS_PHYSIC_VEHICLE)
	NxBoxShapeDesc	boxShapes[MAXSTUDIOBONES];
	vehicleparams_t	vehicleParams;
	NxVehicleDesc	vehicleDesc;
	int		i, j, index;
	int		wheel_count;
	Vector		wheel_pos;

	if( UTIL_GetModelType( pObject->pev->modelindex ) != mod_studio )
	{
		ALERT( at_error, "CreateVehicle: not a studio model\n" );
		return NULL;
          }

	if( !ParseVehicleScript( STRING( scriptName ), vehicleParams ))
	{
		ALERT( at_error, "CreateVehicle: couldn't load %s\n", STRING( scriptName ));
		return NULL;
    }

	model_t *smodel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;

	if( !phdr || phdr->numbones < 1 )
	{
		ALERT( at_error, "CreateVehicle: bad model header\n" );
		return NULL;
	}

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;
	mstudioattachment_t	*pattachment = (mstudioattachment_t *) ((byte *)phdr + phdr->attachmentindex);
	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for( i = 0; i < phdr->numbones; i++, pbone++, panim++ )
	{
		StudioCalcBoneQuaterion( pbone, panim, q[i] );
		StudioCalcBonePosition( pbone, panim, pos[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for( i = 0; i < phdr->numbones; i++ )
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 )
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix );
	}

	// create body vehicle from hitboxes
	for( i = 0; i < phdr->numhitboxes; i++ )
	{
		mstudiobbox_t	*pbbox = (mstudiobbox_t *)((byte *)phdr + phdr->hitboxindex);
		vec3_t		tmp, p[8], mins, maxs, size, pos;

		ClearBounds( mins , maxs );

		for( j = 0; j < 8; j++ )
		{
			tmp[0] = (j & 1) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
			tmp[1] = (j & 2) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
			tmp[2] = (j & 4) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];
			p[j] = bonetransform[pbbox[i].bone].VectorTransform( tmp );
			AddPointToBounds( p[j], mins, maxs );
		}

		boxShapes[i].dimensions.set( NxVec3( maxs - mins ) * 0.5 );		// half-size
		boxShapes[i].localPose.t.set( NxVec3(( mins + maxs ) * 0.5f ));	// origin
		vehicleDesc.carShapes.pushBack( &boxShapes[i] );
	}

	vehicleDesc.mass = 1200.0f;
	vehicleDesc.digitalSteeringDelta = 0.04f;
	vehicleDesc.steeringMaxAngle = 30.0f;
	vehicleDesc.motorForce = 3500.0f;
	vehicleDesc.centerOfMass.set( -24, 0, -16 );
	vehicleDesc.maxVelocity = 60.0f;
	float scale = 32.0f;

	NxVehicleMotorDesc motorDesc;
	NxVehicleGearDesc gearDesc;
	NxWheelDesc wheelDesc[VEHICLE_MAX_WHEEL_COUNT];
	NxReal wheelRadius = 20.0f;

	gearDesc.setToCorvette();
	motorDesc.setToCorvette();

	vehicleDesc.motorDesc = &motorDesc;
	vehicleDesc.gearDesc = &gearDesc;

	// setup wheels
	for( i = wheel_count = 0; i < vehicleParams.axleCount; i++ )
	{
		axleparams_t *axle = &vehicleParams.axles[i];

		for( j = 0; j < axle->wheelsPerAxle; j++ )
		{
			wheelparams_t *wheel = &axle->wheels[j];
			NxU32 flags = NX_WF_USE_WHEELSHAPE;

			wheelDesc[wheel_count].wheelApproximation = 10;
			wheelDesc[wheel_count].wheelRadius = wheel->radius;
			wheelDesc[wheel_count].wheelWidth = 0.1f * scale;	// FIXME
			wheelDesc[wheel_count].wheelSuspension = axle->suspension.springHeight;
			wheelDesc[wheel_count].springRestitution = axle->suspension.springConstant;
			wheelDesc[wheel_count].springDamping = axle->suspension.springDamping;
			wheelDesc[wheel_count].springBias = axle->suspension.springDampingCompression;
			wheelDesc[wheel_count].maxBrakeForce = axle->suspension.brakeForce;
			wheelDesc[wheel_count].frictionToFront = wheel->frontFriction;
			wheelDesc[wheel_count].frictionToSide = wheel->sideFriction;
			wheelDesc[wheel_count].wheelPoseParamIndex = pObject->LookupPoseParameter( wheel->wheelName );
			wheelDesc[wheel_count].suspensionPoseParamIndex = pObject->LookupPoseParameter( wheel->suspensionName );

			// set wheel flags
			if( axle->steerable )
				SetBits( flags, NX_WF_STEERABLE_INPUT );

			if( axle->driven )
				SetBits( flags, NX_WF_ACCELERATED );

			if( axle->affectBrake )
				SetBits( flags, NX_WF_AFFECTED_BY_HANDBRAKE );

			wheelDesc[wheel_count].wheelFlags = flags;

			// set wheel position
			if(( index = FindAttachmentByName( phdr, wheel->attachmentName )) != -1 )
			{
				wheel_pos = bonetransform[pattachment[index].bone].VectorTransform( pattachment[i].org );
				wheelDesc[wheel_count].position.set( NxVec3( wheel_pos ));
			}
			vehicleDesc.carWheels.pushBack( &wheelDesc[wheel_count] );
			wheel_count++;
		}
	}

	vehicleDesc.steeringSteerPoint.set( 1.8 * scale, 0, 0 );
	vehicleDesc.steeringTurnPoint.set( -1.5 * scale, 0, 0 );

	NxVehicle *pVehicle = NxVehicle :: createVehicle( m_pScene, pObject, &vehicleDesc );

	if( !pVehicle )
	{
		ALERT( at_error, "failed to create vehicle from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	// get steer controller index
	pVehicle->steerPoseParamIndex = pObject->LookupPoseParameter( vehicleParams.steering.steeringName );

	NxActor *pActor = pVehicle->getActor();

	if( !pActor )
	{
		ALERT( at_error, "failed to create vehicle from entity %s\n", pObject->GetClassname( ));
		delete pVehicle;
		return NULL;
	}

	pActor->setName( pObject->GetClassname( ));

	NxMat34 pose;
	float mat[16];
	matrix4x4( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f ).CopyToArray( mat );

	pose.setColumnMajor44( mat );
	pActor->setGlobalPose( pose );
	pActor->setLinearVelocity( pObject->GetLocalVelocity() );
	pActor->setAngularVelocity( pObject->GetLocalAvelocity() );

	return pVehicle;
#else
	return NULL;
#endif
}

void CPhysicPhysX :: UpdateVehicle( CBaseEntity *pObject )
{
#if defined (HAS_PHYSIC_VEHICLE)
	if( !pObject || pObject->m_iActorType != ACTOR_VEHICLE )
		return;

	NxVehicle *pVehicle = (NxVehicle *)pObject->m_pUserData;

	for( NxU32 i = 0; i < pVehicle->getNbWheels(); i++ )
	{
		NxWheel *pWheel = (NxWheel *)pVehicle->getWheel( i );

		pObject->SetPoseParameter( pWheel->wheelPoseParamIndex, -pWheel->getRollAngle( ));
		pObject->SetPoseParameter( pWheel->suspensionPoseParamIndex, pWheel->getSuspensionHeight( ));
	}

	CBaseEntity *pDriver = pObject->GetVehicleDriver();

	if( pDriver != NULL )
	{
		bool left = !!FBitSet( pDriver->pev->button, IN_MOVELEFT );
		bool right = !!FBitSet( pDriver->pev->button, IN_MOVERIGHT );
		bool forward = !!FBitSet( pDriver->pev->button, IN_FORWARD );
		bool backward = !!FBitSet( pDriver->pev->button, IN_BACK );
		bool handBrake = !!FBitSet( pDriver->pev->button, IN_JUMP );

		NxReal steering = 0;
		if( left && !right) steering = -1;
		else if (right && !left) steering = 1;
		NxReal acceleration = 0;
		if (forward && !backward) acceleration = -1;
		else if (backward && !forward) acceleration = 1;

		pVehicle->control( steering, false, acceleration, false, handBrake );
		pObject->SetPoseParameter( pVehicle->steerPoseParamIndex, pVehicle->getSteeringAngle() );
	}

	pVehicle->updateVehicle( gpGlobals->frametime );
#endif
}

bool CPhysicPhysX::UpdateEntityTransform( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return false;

	PxRigidDynamic *pDynamicActor = pActor->is<PxRigidDynamic>();
	if (!pDynamicActor || pDynamicActor->isSleeping())
		return false;

	PxTransform pose = pDynamicActor->getGlobalPose();
	matrix4x4 m( PxMat44(pose).front() );

	Vector angles = m.GetAngles();
	Vector origin = m.GetOrigin();

	// store actor velocities too
	pEntity->SetLocalVelocity( pDynamicActor->getLinearVelocity() );
	pEntity->SetLocalAvelocity( pDynamicActor->getAngularVelocity() );
	Vector vecPrevOrigin = pEntity->GetAbsOrigin();

	pEntity->SetLocalAngles( angles );
	pEntity->SetLocalOrigin( origin );
	pEntity->RelinkEntity( TRUE, &vecPrevOrigin );

	return true;
}

bool CPhysicPhysX :: UpdateActorPos( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return false;

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor)
		return false;

	float mat[16];
	matrix4x4 m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f );
	m.CopyToArray( mat );

	PxTransform pose = PxTransform(PxMat44(mat));
	pRigidActor->setGlobalPose( pose );

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	// Static actors can also be moved through this generic entity interface;
	// do not dereference a null dynamic-body pointer for them.
	if( pRigidDynamic && !(pRigidDynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) )
	{
		pRigidDynamic->setLinearVelocity( pEntity->GetLocalVelocity() );
		pRigidDynamic->setAngularVelocity( pEntity->GetLocalAvelocity() );
	}

	return true;
}

bool CPhysicPhysX :: IsBodySleeping( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return false;

	PxRigidDynamic *pDynamicActor = pActor->is<PxRigidDynamic>();
	if (!pDynamicActor)
		return false;

	return pDynamicActor->isSleeping();
}

void CPhysicPhysX :: UpdateEntityAABB( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if( !pRigidActor ) return;

	ClearBounds( pEntity->pev->absmin, pEntity->pev->absmax );
	bool haveBounds = false;
	auto stateIt = m_physicsAssetCollisionStates.find( pEntity->entindex() );

	// The animated trace mesh is rebuilt directly from current Studio bones and
	// therefore gives immediate broad-phase bounds even before the next PhysX
	// simulation step consumes each child's kinematic target.
	if( stateIt != m_physicsAssetCollisionStates.end() && stateIt->second.animatedTraceMesh && stateIt->second.animatedTraceMesh->GetMesh() )
	{
		mmesh_t *mesh = stateIt->second.animatedTraceMesh->GetMesh();
		const matrix4x4 toWorld( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), pEntity->GetScale() );
		for( int i = 0; i < 8; ++i )
		{
			Vector local(
				(i & 1) ? mesh->maxs.x : mesh->mins.x,
				(i & 2) ? mesh->maxs.y : mesh->mins.y,
				(i & 4) ? mesh->maxs.z : mesh->mins.z );
			const Vector world = toWorld.VectorTransform( local );
			AddPointToBounds( world, pEntity->pev->absmin, pEntity->pev->absmax );
		}
		haveBounds = true;
	}
    // Union the actual active child-actor bounds even when the root collision
    // has no bone-bound section. This keeps a swinging / detached primitive
    // door inside the owner's server spatial-link bounds.
    if( stateIt != m_physicsAssetCollisionStates.end() &&
        !stateIt->second.articulatedOverride )
    {
        for( const PhysicsPartRuntime &part : stateIt->second.parts )
        {
            if( !part.actor ) continue;
            bool active = false;
            for( const PhysicsPartShape &shape : part.shapes )
                if( shape.shape && (shape.shape->getFlags() &
                    (PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE)))
                    { active = true; break; }
            if( !active ) continue;
            const PxBounds3 bounds = part.actor->getWorldBounds();
            if( !bounds.isValid() ) continue;
            AddPointToBounds( bounds.minimum, pEntity->pev->absmin, pEntity->pev->absmax );
            AddPointToBounds( bounds.maximum, pEntity->pev->absmin, pEntity->pev->absmax );
            haveBounds = true;
        }
    }
    // A v4 part-only asset does not replace the old root shape. Do not
    // discard chassis/player collision merely because a hinge mesh exists.
    const std::shared_ptr<PhysicsAssetDef> boundsAsset =
        stateIt != m_physicsAssetCollisionStates.end() ? GetPhysicsAssetDefinition(pEntity) : nullptr;
    const bool includeRoot = stateIt == m_physicsAssetCollisionStates.end() ||
        (boundsAsset && boundsAsset->collisions.empty()) ||
        (stateIt != m_physicsAssetCollisionStates.end() &&
         stateIt->second.selectionHasGeometry && !stateIt->second.animatedTraceMesh);
    if( includeRoot && pRigidActor->getNbShapes() > 0 )
    {
        const PxBounds3 rootBounds = pRigidActor->getWorldBounds();
        if( rootBounds.isValid() )
        {
            AddPointToBounds( rootBounds.minimum, pEntity->pev->absmin, pEntity->pev->absmax );
            AddPointToBounds( rootBounds.maximum, pEntity->pev->absmin, pEntity->pev->absmax );
            haveBounds = true;
        }
    }

	if( !haveBounds )
	{
		pEntity->pev->absmin = pEntity->GetAbsOrigin();
		pEntity->pev->absmax = pEntity->GetAbsOrigin();
	}
	pEntity->pev->mins = pEntity->pev->absmin - pEntity->pev->origin;
	pEntity->pev->maxs = pEntity->pev->absmax - pEntity->pev->origin;
	pEntity->pev->size = pEntity->pev->maxs - pEntity->pev->mins;
}

/*
===============

PHYSICS SAVE\RESTORE SYSTEM

===============
*/

// list of variables that needs to be saved	how it will be saved

/*
// actor description
NxMat34		globalPose;		pev->origin, pev->angles
const NxBodyDesc*	body;			not needs
NxReal		density;			constant (???)
NxU32		flags;			m_iActorFlags;
NxActorGroup	group;			pev->groupinfo (???).
NxU32		contactReportFlags; 	not needs (already set by scene rules)
NxU16		forceFieldMaterial;		not needs (not used)
void*		userData;			not needs (automatically restored)
const char*	name;			not needs (automatically restored)
NxCompartment	*compartment;		not needs (not used)
NxActorDescType	type;			not needs (automatically restored)
// body description
NxMat34		massLocalPose;		not needs (probably automatically recomputed on apply shape)
NxVec3		massSpaceInertia;		not needs (probably automatically recomputed on apply shape)
NxReal		mass;			not needs (probably automatically recomputed on apply shape)
NxVec3		linearVelocity;		pev->velocity
NxVec3		angularVelocity;		pev->avelocity
NxReal		wakeUpCounter;		not needs (use default)
NxReal		linearDamping;		not needs (use default)
NxReal		angularDamping;		not needs (use default)
NxReal		maxAngularVelocity; 	not needs (already set by scene rules)
NxReal		CCDMotionThreshold;		not needs (use default)
NxU32		flags;			m_iBodyFlags;
NxReal		sleepLinearVelocity; 	not needs (already set by scene rules)
NxReal		sleepAngularVelocity; 	not needs (already set by scene rules)
NxU32		solverIterationCount;	not needs (use default)
NxReal		sleepEnergyThreshold;	not needs (use default)
NxReal		sleepDamping;		not needs (use default)
NxReal		contactReportThreshold;	not needs (use default)
*/

/*
===============
SaveBody

collect all the info from generic actor
===============
*/
// I.9.5: preserve the full articulated simulation, not just the old root shadow.
// Snapshot identity is the model path + ordered named .phys body definitions.
bool CPhysicPhysX::CaptureRagdollState(CBaseEntity *entity,
    PhysicsRagdollSaveState &save, PhysicsRagdollBodySaveRecord *bodies,int capacity)
{
    std::memset(&save,0,sizeof(save));
    if(!entity || !bodies || capacity<=0) return false;
    PhysicsAssetInstance *asset=FindPhysicsAsset(entity);
    if(!asset || !asset->MatchesOwnerModel() || !asset->IsRagdoll()) return true;
    save.active=1;
    const char *model=entity->GetModel();
    if(model && std::strlen(model)<sizeof(save.model))
        std::memcpy(save.model,model,std::strlen(model));
    const auto &runtime=asset->Bodies();
    save.count=static_cast<int>(runtime.size());
    save.frozen=asset->IsFrozen()?1:0;
    const int index=ENTINDEX(entity->edict());
    auto original=m_ragdollEntityState.find(index);
    save.full=original!=m_ragdollEntityState.end()?1:0;
    const int solid=save.full?original->second.solid:entity->pev->solid;
    const int movetype=save.full?original->second.movetype:entity->pev->movetype;
    const Vector mins=save.full?original->second.mins:entity->pev->mins;
    const Vector maxs=save.full?original->second.maxs:entity->pev->maxs;
    const Vector angles=save.full?original->second.angles:entity->GetAbsAngles();
    save.oldSolid=solid; save.oldMovetype=movetype;
    save.oldMins[0]=mins.x; save.oldMins[1]=mins.y; save.oldMins[2]=mins.z;
    save.oldMaxs[0]=maxs.x; save.oldMaxs[1]=maxs.y; save.oldMaxs[2]=maxs.z;
    save.oldAngles[0]=angles.x; save.oldAngles[1]=angles.y; save.oldAngles[2]=angles.z;
    const Vector savedOrigin=entity->GetAbsOrigin();
    save.savedEntityOrigin[0]=savedOrigin.x;
    save.savedEntityOrigin[1]=savedOrigin.y;
    save.savedEntityOrigin[2]=savedOrigin.z;
    if(save.full && original->second.hasRootRelation)
    {
        save.hasRootRelation=1;
        original->second.rootToEntity.CopyToArray(save.rootToEntity);
    }
    save.valid=(model && save.model[0] && save.count>0 && save.count<=capacity &&
        asset->SaveArticulatedBodies(bodies,capacity)) ? 1:0;
    if(!save.valid)
        ALERT(at_warning,"Cannot save complete ragdoll for %s (bodies %d, limit %d); restore will use safe animation fallback\n",
            entity->GetClassname(),save.count,capacity);
    return save.valid!=0;
}

bool CPhysicPhysX::RestoreRagdollState(CBaseEntity *entity,
    const PhysicsRagdollSaveState &save,const PhysicsRagdollBodySaveRecord *bodies,int capacity)
{
    if(!entity || !save.active) return true;
    // Saved entvars can contain SOLID_NOT / MOVETYPE_NONE / CF_RAGDOLL.
    // Even if the asset is missing/changed, never leave a physics-less ghost.
    auto fallback=[&]() -> bool {
        if(IsRagdoll(entity)) SetRagdoll(entity,false);
        ClearBits(entity->pev->iuser1,CF_RAGDOLL);
        entity->pev->solid=save.oldSolid;
        entity->pev->movetype=save.oldMovetype;
        UTIL_SetSize(entity->pev,Vector(save.oldMins[0],save.oldMins[1],save.oldMins[2]),
            Vector(save.oldMaxs[0],save.oldMaxs[1],save.oldMaxs[2]));
        entity->SetAbsAngles(Vector(save.oldAngles[0],save.oldAngles[1],save.oldAngles[2]));
        entity->RelinkEntity(TRUE);
        ALERT(at_warning,"Ragdoll restore fell back to ordinary entity for %s\n",entity->GetClassname());
        return false;
    };
    if(!bodies || save.count>kSavedRagdollBodyLimit ||
        !physicsasset::ValidRagdollSnapshotHeader(save.model,sizeof(save.model),
            entity->GetModel(),save.count,capacity,save.valid,save.full,save.frozen))
        return fallback();
    PhysicsAssetInstance *asset=EnsurePhysicsAssetInstance(entity);
    if(!asset || !asset->MatchesOwnerModel() ||
        static_cast<size_t>(save.count)!=asset->Bodies().size() ||
        !m_pPhysics || !m_pScene) return fallback();
    if(!asset->HasActors() && !CreatePhysicsAsset(entity)) return fallback();
    if(!asset->HasActors()) return fallback();

    // Restore pre-ragdoll movement first so ownership capture stores the
    // real hull rather than the saved SOLID_NOT corpse hull.
    entity->pev->solid=save.oldSolid;
    entity->pev->movetype=save.oldMovetype;
    UTIL_SetSize(entity->pev,Vector(save.oldMins[0],save.oldMins[1],save.oldMins[2]),
        Vector(save.oldMaxs[0],save.oldMaxs[1],save.oldMaxs[2]));
    entity->SetAbsAngles(Vector(save.oldAngles[0],save.oldAngles[1],save.oldAngles[2]));
    ClearBits(entity->pev->iuser1,CF_RAGDOLL);
    PxActor *oldActor=ActorFromEntity(entity);
    PxRigidActor *oldRigid=oldActor?oldActor->is<PxRigidActor>():nullptr;
    if(oldRigid) SuspendLegacyCollisionForRagdoll(entity,oldRigid);
    if(!asset->IsRagdoll() && !asset->SetRagdoll(true)) {
        RestoreLegacyCollisionAfterRagdoll(entity);
        return fallback();
    }
    // Preserve savegame landmark translations: body positions are stored in
    // the saved level's world frame; an entity may be offset on load.
    const Vector originNow=entity->GetAbsOrigin();
    const PxVec3 originOffset(originNow.x-save.savedEntityOrigin[0],
        originNow.y-save.savedEntityOrigin[1],originNow.z-save.savedEntityOrigin[2]);
    if(!asset->RestoreArticulatedBodies(bodies,save.count,save.frozen!=0,originOffset)) {
        asset->SetRagdoll(false);
        RestoreLegacyCollisionAfterRagdoll(entity);
        return fallback();
    }
    SetArticulatedCollisionOwnership(entity,true);
    if(save.full)
    {
        SuspendEntityForRagdoll(entity,asset);
        auto relation=m_ragdollEntityState.find(ENTINDEX(entity->edict()));
        if(relation!=m_ragdollEntityState.end() && save.hasRootRelation)
        {
            relation->second.rootToEntity=matrix4x4(save.rootToEntity);
            relation->second.hasRootRelation=true;
        }
    }
    else
        SetBits(entity->pev->iuser1,CF_RAGDOLL);
    const int id=ENTINDEX(entity->edict());
    m_pendingRagdollImpulses.erase(id);
    m_ragdollNetworkState.erase(id);
    asset->CaptureSimulationPose();
    UpdateRagdollPresentation(asset);
    return true;
}

void CPhysicPhysX :: SaveBody( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		ALERT(at_warning, "SaveBody: physic entity %i missed actor!\n", pEntity->m_iActorType);
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor)
	{
		ALERT(at_warning, "SaveBody: physic entity %i missed rigid actor!\n", pEntity->m_iActorType);
		return;
	}

	PxShape *shape;
	PxFilterData filterData;
	PxRigidDynamic *pRigidBody = pRigidActor->is<PxRigidDynamic>();

	if (pRigidActor->getShapes(&shape, 1)) {
		filterData = shape->getSimulationFilterData();
	}

	// filter data get and stored only for one shape, but it can be more than one
	// assumed that all attached shapes have same filter data
	pEntity->m_iFilterData[0] = filterData.word0;
	pEntity->m_iFilterData[1] = filterData.word1;
	pEntity->m_iFilterData[2] = filterData.word2;
	pEntity->m_iFilterData[3] = filterData.word3;

	pEntity->m_iActorFlags = pRigidActor->getActorFlags();
	if (pRigidBody) {
		pEntity->m_iBodyFlags = pRigidBody->getRigidBodyFlags();
		pEntity->m_flBodyMass = pRigidBody->getMass();
		pEntity->m_fFreezed = pRigidBody->isSleeping();
	}
	else {
		pEntity->m_iBodyFlags = 0;
		pEntity->m_flBodyMass = 0.0f;
		pEntity->m_fFreezed = false;
	}

	if (pEntity->m_iActorType == ACTOR_DYNAMIC && !IsRagdoll(pEntity))
	{
		// Never copy the suspended legacy shadow's transform over the
		// authoritative articulated pose while serializing a ragdoll.
		UpdateEntityTransform(pEntity);
	}
}

/*
===============
RestoreBody

re-create shape, apply physic params
===============
*/
void *CPhysicPhysX :: RestoreBody( CBaseEntity *pEntity )
{
	// physics not initialized?
	if (!m_pScene)
		return NULL;

	PxShape *pShape;
	PxRigidActor *pActor;
	PxTransform pose;
	Vector angles = pEntity->GetAbsAngles();
	PxMeshScale scale(pEntity->GetScale());

	if (pEntity->m_iActorType == ACTOR_CHARACTER) {
		angles = g_vecZero;	// no angles for NPC and client
	}

	float mat[16];
	matrix4x4 m(pEntity->GetAbsOrigin(), angles, 1.0f);
	m.CopyToArray(mat);
	pose = PxTransform(PxMat44(mat));

	if (pEntity->m_iActorType == ACTOR_STATIC) {
		pActor = m_pPhysics->createRigidStatic(pose);
	}
	else {
		pActor = m_pPhysics->createRigidDynamic(pose);
	}

	if (!pActor)
	{
		ALERT(at_error, "RestoreBody: unbale to create actor with type (%i)\n", pEntity->m_iActorType);
		return NULL;
	}

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pEntity->m_iActorType == ACTOR_KINEMATIC)
	{
		// set kinematic flag before shape creating, this is required by design
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	}

	switch (pEntity->m_iActorType)
	{
		case ACTOR_DYNAMIC:
		{
			PxConvexMesh *convexMesh = ConvexMeshFromEntity(pEntity);
			if (!convexMesh) {
				ALERT(at_error, "RestoreBody: failed to create convex mesh\n");
				return NULL;
			}

			pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxConvexMeshGeometry(convexMesh, scale), *m_pDefaultMaterial);
			break;
		}
		case ACTOR_CHARACTER:
		{
			PxBoxGeometry box;
			box.halfExtents = pEntity->pev->size * k_PaddingFactor;
			pShape = PxRigidActorExt::createExclusiveShape(*pActor, box, *m_pDefaultMaterial);
			break;
		}
		case ACTOR_KINEMATIC:
		case ACTOR_STATIC:
		{
			PxTriangleMesh *triangleMesh = TriangleMeshFromEntity(pEntity);
			if (!triangleMesh) {
				ALERT(at_error, "RestoreBody: failed to create triangle mesh\n");
				return NULL;
			}

			PxMaterial *pMaterial = m_pDefaultMaterial;
			if (pEntity->pev->flags & FL_CONVEYOR) {
				pMaterial = m_pConveyorMaterial;
			}
			pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(triangleMesh, scale), *pMaterial);
			break;
		}
		default:
		{
			ALERT(at_error, "RestoreBody: invalid actor type %i\n", pEntity->m_iActorType);
			return NULL;
		}
	}

	if (!pShape) {
		ALERT(at_error, "RestoreBody: failed to create shape\n");
		return NULL;
	}

	PxFilterData filterData;
	filterData.word0 = pEntity->m_iFilterData[0];
	filterData.word1 = pEntity->m_iFilterData[1];
	filterData.word2 = pEntity->m_iFilterData[2];
	filterData.word3 = pEntity->m_iFilterData[3];
	pShape->setSimulationFilterData(filterData);

	// fill in actor description
	if (pEntity->m_iActorType != ACTOR_STATIC)
	{
		pRigidBody->setRigidBodyFlags(static_cast<PxRigidBodyFlags>(pEntity->m_iBodyFlags));
		pRigidBody->setMass(pEntity->m_flBodyMass);
		pRigidBody->setSolverIterationCounts(k_SolverIterationCount);

		if (pEntity->m_iActorType != ACTOR_KINEMATIC)
		{
			pRigidBody->setLinearVelocity(pEntity->GetAbsVelocity());
			pRigidBody->setAngularVelocity(pEntity->GetAbsAvelocity());
		}

		float density = pEntity->GetDensity();
		if (density <= 0.0f)
			density = k_DefaultDensity;

		PxRigidBodyExt::updateMassAndInertia(*pRigidBody, density);
		if (pEntity->m_fFreezed && pEntity->m_iActorType == ACTOR_DYNAMIC) {
			pRigidBody->putToSleep();
		}
	}

	pActor->userData = pEntity->edict();
	pActor->setActorFlags(static_cast<PxActorFlags>(pEntity->m_iActorFlags));
	pActor->setName(pEntity->GetClassname());
	pEntity->m_pUserData = pActor;
	m_pScene->addActor(*pActor);
	TrackPhysicsAssetCollision( pEntity );
	return pActor;
}

void CPhysicPhysX :: SetAngles( CBaseEntity *pEntity, const Vector &angles )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console, "PhysX: SetAngles( entity = %x, angles = (%.2f, %.2f, %.2f) )\n",
			pEntity, angles.x, angles.y, angles.z);
	}

	matrix3x3 m(angles);
	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform transform = pRigidDynamic->getGlobalPose();
	transform.q = PxQuat(PxMat33( m ));
	pRigidDynamic->setGlobalPose(transform);
}

void CPhysicPhysX :: SetOrigin( CBaseEntity *pEntity, const Vector &origin )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console, "PhysX: SetOrigin( entity = %x, origin = (%.2f, %.2f, %.2f) )\n",
			pEntity, origin.x, origin.y, origin.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform transform = pRigidDynamic->getGlobalPose();
	transform.p = origin;
	pRigidDynamic->setGlobalPose(transform);
}

void CPhysicPhysX :: SetVelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console, "PhysX: SetVelocity( entity = %x, velocity = (%.2f, %.2f, %.2f) )\n",
			pEntity, velocity.x, velocity.y, velocity.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setLinearVelocity( velocity );
}

void CPhysicPhysX :: SetAvelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console, "PhysX: SetAvelocity( entity = %x, velocity = (%.2f, %.2f, %.2f) )\n",
			pEntity, velocity.x, velocity.y, velocity.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setAngularVelocity( velocity );
}

void CPhysicPhysX :: MoveObject( CBaseEntity *pEntity, const Vector &finalPos )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console, "PhysX: MoveObject( entity = %x, finalPos = (%.2f, %.2f, %.2f) )\n",
			pEntity, finalPos.x, finalPos.y, finalPos.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform pose = pRigidDynamic->getGlobalPose();
	pose.p = finalPos;
	pRigidDynamic->setKinematicTarget(pose);
}

void CPhysicPhysX :: RotateObject( CBaseEntity *pEntity, const Vector &finalAngle )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console, "PhysX: RotateObject( entity = %x, finalAngle = (%.2f, %.2f, %.2f) )\n",
			pEntity, finalAngle.x, finalAngle.y, finalAngle.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform pose = pRigidDynamic->getGlobalPose();
	matrix3x3 m(finalAngle);
	pose.q = PxQuat(PxMat33(m));
	pRigidDynamic->setKinematicTarget(pose);
}

void CPhysicPhysX :: SetLinearMomentum( CBaseEntity *pEntity, const Vector &momentum )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody)
		return;

	PxVec3 targetMomentum( momentum );
	PxVec3 currentMomentum = pRigidBody->getLinearVelocity() * pRigidBody->getMass();
	pRigidBody->addForce( targetMomentum - currentMomentum, PxForceMode::eIMPULSE );
}

void CPhysicPhysX :: AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody)
		return;

	PxRigidBodyExt::addForceAtPos(*pRigidBody, PxVec3(impulse * factor), position, PxForceMode::eIMPULSE);
}

void CPhysicPhysX :: AddForce( CBaseEntity *pEntity, const Vector &force, ForceMode mode )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if( !pRigidBody )
		return;

	switch( mode )
	{
		case ForceMode::Impulse: pRigidBody->addForce( force, PxForceMode::eIMPULSE ); break;
		case ForceMode::VelocityChange: pRigidBody->addForce( force, PxForceMode::eVELOCITY_CHANGE ); break;
		case ForceMode::Acceleration: pRigidBody->addForce( force, PxForceMode::eACCELERATION ); break;
		default: pRigidBody->addForce( force, PxForceMode::eFORCE ); break;
	}
}

void CPhysicPhysX :: AddTorque( CBaseEntity *pEntity, const Vector &torque, ForceMode mode )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if( !pRigidBody )
		return;

	switch( mode )
	{
		case ForceMode::Impulse: pRigidBody->addTorque( torque, PxForceMode::eIMPULSE ); break;
		case ForceMode::VelocityChange: pRigidBody->addTorque( torque, PxForceMode::eVELOCITY_CHANGE ); break;
		case ForceMode::Acceleration: pRigidBody->addTorque( torque, PxForceMode::eACCELERATION ); break;
		default: pRigidBody->addTorque( torque, PxForceMode::eFORCE ); break;
	}
}

void CPhysicPhysX :: SetHoldableTarget( CBaseEntity *pEntity, const Vector &targetOrigin, const Vector4D &targetQuat )
{
	m_holdableController->SetTarget( pEntity, targetOrigin, targetQuat );
}

void CPhysicPhysX :: ClearHoldableTarget( CBaseEntity *pEntity )
{
	m_holdableController->ClearTarget( pEntity );
}

void CPhysicPhysX :: GetTransform( CBaseEntity *pEntity, matrix4x4 &out )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if( !pRigidBody )
		return;

	PxTransform pose = pRigidBody->getGlobalPose();
	matrix4x4 m( PxMat44(pose).front() );
	out = m;
}

int CPhysicPhysX :: FLoadTree( char *szMapName )
{
	if( !szMapName || !*szMapName || !m_pPhysics )
		return 0;

	if( m_fLoaded )
	{
		if( !Q_stricmp( szMapName, m_szMapName ))
		{
			// stay on same map - no reload
			return 1;
		}

		if (m_pSceneActor)
			m_pSceneActor->release();
		m_pSceneActor = NULL;

		if (m_pSceneMesh)
			m_pSceneMesh->release();
		m_pSceneMesh = NULL;

		m_fLoaded = FALSE; // trying to load new collision tree
	}

	// save off mapname
	strcpy(m_szMapName, szMapName);

	char szHullFilename[MAX_PATH];

	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );
	UserStream cacheFileStream(szHullFilename, true);
	m_pSceneMesh = m_pPhysics->createTriangleMesh(cacheFileStream);
	m_fWorldChanged = FALSE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

int CPhysicPhysX :: CheckBINFile( char *szMapName )
{
	if( !szMapName || !*szMapName || !m_pPhysics )
		return FALSE;

	char szBspFilename[MAX_PATH];
	char szHullFilename[MAX_PATH];

	Q_snprintf( szBspFilename, sizeof( szBspFilename ), "maps/%s.bsp", szMapName );
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );

	BOOL retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( szBspFilename, szHullFilename, &iCompare ))
	{
		if( iCompare > 0 )
		{
			// BSP file is newer.
			ALERT ( at_console, ".BIN file will be updated\n\n" );
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

void CPhysicPhysX::CacheNameForModel( model_t *model, fs::Path &hullfile, uint32_t hash, const char *suffix )
{
	if (!model->name || model->name[0] == '\0')
		return;

	std::string pathString = model->name;
    const size_t pos = pathString.find_first_of("/\\");
    if (pos != std::string::npos) {
        pathString.erase(0, pos + 1);
    }

	fs::Path modelPath{ pathString };
	std::string fileName = fmt::format("{}_{:08x}{}", modelPath.stem().string(), hash, suffix);
	hullfile = "cache" / modelPath.parent_path() / fileName;
}

uint32_t CPhysicPhysX::GetHashForModelState( model_t *model, int32_t body, int32_t skin )
{
	uint32_t hash;
	CRC32_Init(&hash);
	CRC32_ProcessBuffer(&hash, &body, sizeof(body));
	CRC32_ProcessBuffer(&hash, &skin, sizeof(skin));
	CRC32_ProcessBuffer(&hash, &model->modelCRC, sizeof(model->modelCRC));
	return CRC32_Final(hash);
}

//-----------------------------------------------------------------------------
// hulls - convex hulls cooked with NxCookingConvexMesh routine and stored into cache\*.hull
// meshes - triangle meshes cooked with NxCookingTriangleMesh routine and stored into cache\*.mesh
//-----------------------------------------------------------------------------
int CPhysicPhysX :: CheckFileTimes( const char *szFile1, const char *szFile2 )
{
	if( !szFile1 || !*szFile1 || !szFile2 || !*szFile2 )
		return FALSE;

	BOOL retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( (char *)szFile1, (char *)szFile2, &iCompare ))
	{
		if( iCompare > 0 )
		{
			// MDL file is newer.
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

bool CPhysicPhysX::DebugEnabled() const
{
	return g_engfuncs.CheckParm("-physxdebug", nullptr) != 0;
}

bool CPhysicPhysX::TracingStateChanges(PxActor *actor) const
{
	if (DebugEnabled() && m_traceStateChanges) {
		return strcmp(actor->getName(), "func_physbox") == 0 || strcmp(actor->getName(), "env_physbox") == 0;
	}
	return false;
}

void CPhysicPhysX::HandleEvents()
{
	auto &touchEventsQueue = m_eventHandler->getTouchEventsQueue();
	while (!touchEventsQueue.empty())
	{
		auto &touchEvent = touchEventsQueue.front();
		edict_t *e1 = reinterpret_cast<edict_t*>(touchEvent.first->userData);
		edict_t *e2 = reinterpret_cast<edict_t*>(touchEvent.second->userData);
		DispatchTouch(e1, e2);
		touchEventsQueue.pop();
	}

	auto &waterContactPairs = m_eventHandler->getWaterContactPairs();
	for (const auto &pair : waterContactPairs)
	{
		edict_t *entity = reinterpret_cast<edict_t*>(pair.objectActor->userData);
		physx::PxRigidDynamic *dynamicActor = pair.objectActor->is<PxRigidDynamic>();
		physx::PxRigidDynamic *triggerActor = pair.waterTriggerActor->is<PxRigidDynamic>();

		if (!entity || !dynamicActor || !triggerActor)
			continue;

		if (!FBitSet(dynamicActor->getRigidBodyFlags(), PxRigidBodyFlag::eKINEMATIC))
		{
			PxBounds3 objectBounds = dynamicActor->getWorldBounds();
			PxBounds3 waterBounds = triggerActor->getWorldBounds();
			PxBounds3 intersectionBounds = GetIntersectionBounds(objectBounds, waterBounds);
			if (!intersectionBounds.isEmpty())
			{
				PxVec3 intersectionSizes = intersectionBounds.getDimensions();
				float displacedVolume = intersectionSizes.x * intersectionSizes.y * intersectionSizes.z;
				PxVec3 byoyancyForce = displacedVolume * k_WaterDensity * -PxVec3(0.f, 0.f, -g_psv_gravity->value);
				dynamicActor->addForce(byoyancyForce);

				// in practice this velocity should be relative to water shape, since water can be moved
				PxVec3 velocityDir = dynamicActor->getLinearVelocity();// -triggerActor->getLinearVelocity();
				float velocityMag = velocityDir.normalizeSafe();
				PxVec3 linearDragForce = k_WaterLinearDragFactor * k_WaterDensity * velocityMag * velocityMag * -velocityDir;
				dynamicActor->addForce(linearDragForce);

				PxVec3 angularDragForce = k_WaterAngularDragFactor * displacedVolume * -dynamicActor->getAngularVelocity();
				dynamicActor->addTorque(angularDragForce);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// assume m_pWorldModel is valid
//-----------------------------------------------------------------------------
int CPhysicPhysX :: ConvertEdgeToIndex( model_t *model, int edge )
{
	int e = model->surfedges[edge];
	return (e > 0) ? model->edges[e].v[0] : model->edges[-e].v[1];
}

int CPhysicPhysX :: BuildCollisionTree( char *szMapName )
{
	if( !m_pPhysics )
		return FALSE;

	// get a world struct
	if(( m_pWorldModel = (model_t *)MODEL_HANDLE( 1 )) == NULL )
	{
		ALERT( at_error, "BuildCollisionTree: unbale to fetch world pointer %s\n", szMapName );
		return FALSE;
	}

	if (m_pSceneActor)
		m_pSceneActor->release();
	m_pSceneActor = NULL;

	if (m_pSceneMesh)
		m_pSceneMesh->release();
	m_pSceneMesh = NULL;

	// save off mapname
	Q_strcpy( m_szMapName, szMapName );

	ALERT( at_console, "Tree Collision out of Date. Rebuilding...\n" );

	// convert world from polygons to tri-list
	int i, numElems = 0, totalElems = 0;
	msurface_t *psurf;

	// compute vertexes count
	for( i = 0; i < m_pWorldModel->nummodelsurfaces; i++ )
	{
		psurf = &m_pWorldModel->surfaces[m_pWorldModel->firstmodelsurface + i];

		if( psurf->flags & ( SURF_DRAWTURB|SURF_DRAWSKY ))
			continue;

		totalElems += (psurf->numedges - 2);
	}

	PxU32 *indices = new PxU32[totalElems * 3];

	for( i = 0; i < m_pWorldModel->nummodelsurfaces; i++ )
	{
		psurf = &m_pWorldModel->surfaces[m_pWorldModel->firstmodelsurface + i];
		int k = psurf->firstedge;

		// don't create collision for water
		if( psurf->flags & ( SURF_DRAWTURB|SURF_DRAWSKY ))
			continue;

		for( int j = 0; j < psurf->numedges - 2; j++ )
		{
			indices[numElems*3+0] = ConvertEdgeToIndex( m_pWorldModel, k );
			indices[numElems*3+1] = ConvertEdgeToIndex( m_pWorldModel, k + j + 2 );
			indices[numElems*3+2] = ConvertEdgeToIndex( m_pWorldModel, k + j + 1 );
			numElems++;
		}
	}

	PX_ASSERT( totalElems == numElems );

	// build physical model
	PxTriangleMeshDesc levelDesc;
	levelDesc.points.count = m_pWorldModel->numvertexes;
	levelDesc.points.data = (const PxVec3*)&(m_pWorldModel->vertexes[0].position);
	levelDesc.points.stride = sizeof(mvertex_t);
	levelDesc.triangles.count = numElems;
	levelDesc.triangles.data = indices;
	levelDesc.triangles.stride = 3 * sizeof(PxU32);
	levelDesc.flags = (PxMeshFlags)0;

	char szHullFilename[MAX_PATH];
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );

	if (m_pCooking)
	{
		UserStream outputFileStream(szHullFilename, false);
		bool status = m_pCooking->cookTriangleMesh(levelDesc, outputFileStream);
	}

	delete [] indices;

	UserStream inputFileStream(szHullFilename, true);
	m_pSceneMesh = m_pPhysics->createTriangleMesh(inputFileStream);
	m_fWorldChanged = TRUE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

void CPhysicPhysX::SetupWorld(void)
{
	if (m_pSceneActor)
		return;	// already loaded

	if (!m_pSceneMesh)
	{
		ALERT(at_error, "*collision tree not ready!\n");
		return;
	}

	// get a world struct
	if ((m_pWorldModel = (model_t *)MODEL_HANDLE(1)) == NULL)
	{
		ALERT(at_error, "SetupWorld: unbale to fetch world pointer %s\n", m_szMapName);
		return;
	}

	PxRigidStatic *pActor = m_pPhysics->createRigidStatic(PxTransform(PxVec3(PxZero), PxQuat(PxIdentity)));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(m_pSceneMesh), *m_pDefaultMaterial);

	pActor->setName(g_pWorld->GetClassname());
	pActor->userData = g_pWorld->edict();

	m_pScene->addActor(*pActor);
	m_pSceneActor = pActor;
	m_worldBounds = pActor->getWorldBounds();
	m_eventHandler->onWorldInit();
	m_fLoaded = true;
}

void CPhysicPhysX :: DebugDraw( void )
{
	if( !m_pPhysics || !m_pScene )
		return;

	const int ragdollDebug = static_cast<int>( ragdoll_debug.value );
	const bool generalDebug = DebugEnabled();
	const bool visualizationEnabled = generalDebug || ragdollDebug > 0;
	// sv_ragdoll_debug can be enabled after scene creation, so do not rely on
	// SetupWorld having turned the global PhysX visualization scale on.
	m_pScene->setVisualizationParameter( PxVisualizationParameter::eSCALE, visualizationEnabled ? 1.0f : 0.0f );
	m_pScene->setVisualizationParameter( PxVisualizationParameter::eCOLLISION_SHAPES, visualizationEnabled ? 1.0f : 0.0f );
	m_pScene->setVisualizationParameter( PxVisualizationParameter::eBODY_AXES, visualizationEnabled ? 1.0f : 0.0f );
	m_pScene->setVisualizationParameter( PxVisualizationParameter::eJOINT_LOCAL_FRAMES, ragdollDebug >= 2 ? 1.0f : 0.0f );
	m_pScene->setVisualizationParameter( PxVisualizationParameter::eJOINT_LIMITS, ragdollDebug >= 2 ? 1.0f : 0.0f );
	// Actors default to visualization enabled, including the entire BSP mesh.
	// Ragdoll-only debugging must clear that flag on world/legacy/prop actors
	// before enabling the selected articulated bodies. Restore all for general debug.
	if( visualizationEnabled )
	{
		const PxActorTypeFlags actorFlags = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
		std::vector<PxActor*> actors( m_pScene->getNbActors( actorFlags ));
		const PxU32 actorCount = m_pScene->getActors( actorFlags, actors.data(), static_cast<PxU32>( actors.size() ));
		for( PxU32 i = 0; i < actorCount; ++i )
			actors[i]->setActorFlag( PxActorFlag::eVISUALIZATION, generalDebug );
	}
	for( const auto &asset : m_physicsAssets )
	{
		const bool visible = generalDebug || ( ragdollDebug > 0 && asset->IsRagdoll() );
		for( const auto &body : asset->Bodies() )
			if( body.actor ) body.actor->setActorFlag( PxActorFlag::eVISUALIZATION, visible );
	}
	m_debugRenderer->RenderData(m_pScene->getRenderBuffer());
}

/*
===============
P_SpeedsMessage
===============
*/
bool CPhysicPhysX :: P_SpeedsMessage( char *out, size_t size )
{
	if( !p_speeds || p_speeds->value <= 0.0f )
		return false;

	if( !out || !size ) return false;
	Q_strncpy( out, p_speeds_msg, size );

	return true;
}

/*
================
SCR_RSpeeds
================
*/
void CPhysicPhysX :: DrawPSpeeds( void )
{
	char	msg[1024];
	int	iScrWidth = CVAR_GET_FLOAT( "width" );

	if( P_SpeedsMessage( msg, sizeof( msg )))
	{
		int	x, y, height;
		char	*p, *start, *end;

		x = iScrWidth - 320;
		y = 128;

		DrawConsoleStringLen( NULL, NULL, &height );
		DrawSetTextColor( 1.0f, 1.0f, 1.0f );

		p = start = msg;
		do
		{
			end = Q_strchr( p, '\n' );
			if( end ) msg[end-start] = '\0';

			DrawConsoleString( x, y, p );
			y += height;

			if( end )
				p = end + 1;
			else
				break;
		} while( 1 );
	}
}

void CPhysicPhysX::ClearWorldAssetCaches()
{
	// Cached meshes hold a PhysX reference of their own; they must be released
	// only after all map actors, shapes and articulated instances are gone.
	// Definitions include null (missing/invalid .phys) lookups, so clearing them
	// here also makes newly authored assets discoverable on the next map.
	physicsasset::ReleaseWorldAssetCaches( m_triangleMeshCache,
		m_physicsAssetConvexMeshCache, m_physicsAssetDefinitions );
}

void CPhysicPhysX :: FreeWorld()
{
    m_performance = PerformanceState{};
	if( !m_pScene )
	{
		// A failed or disabled scene must not preserve stale lookup results.
		ClearWorldAssetCaches();
		return;
	}

	// PhysicsAssetInstance owns its articulated actors and joints. Release those
	// first so the generic scene teardown below never releases the same PxActor
	// and leaves a dangling pointer for FreePhysic()/the instance destructor.
	m_physicsAssets.clear();
    // FreeWorld may be called with live root + bone-bound actors. Release
    // constraints first, then child actors, then let the generic scene pass
    // release only the remaining root/other actors.
    for( auto &entry : m_physicsAssetCollisionStates )
        ReleaseAnimatedCollisionActors( entry.second );
    m_physicsAssetCollisionStates.clear();
	m_ragdollLegacyCollisionState.clear();
	m_ragdollEntityState.clear();
	m_ragdollNetworkState.clear();
	m_pendingRagdollImpulses.clear();

	PxActorTypeFlags actorFlags = (
		PxActorTypeFlag::eRIGID_STATIC |
		PxActorTypeFlag::eRIGID_DYNAMIC
	);

	std::vector<PxActor*> actors;
	actors.assign(m_pScene->getNbActors(actorFlags), nullptr);
	m_pScene->getActors(actorFlags, actors.data(), static_cast<PxU32>(actors.size()));

	// throw all bodies
	for (PxActor *actor : actors)
	{
		CBaseEntity *entity = EntityFromActor( actor );
		if (entity) {
			entity->m_pUserData = nullptr;
		}
		m_pScene->removeActor(*actor);
		actor->release();
	}
	// Scene teardown released bone-bound child actors too; discard their raw
	// runtime pointers before a later map/setup can inspect this state.
	m_physicsAssetCollisionStates.clear();

	m_holdableController->ClearAllTargets();
	m_eventHandler->onWorldShutdown();
	m_pSceneActor = nullptr;

	// IMPORTANT: only release the cached PhysX mesh references after the map
	// actors and their shapes have been removed and destroyed above. This also
	// invalidates both positive and negative .phys definition lookups.
	ClearWorldAssetCaches();
}

void CPhysicPhysX :: TeleportCharacter( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (pRigidBody->getNbShapes() <= 0)
		return;

	PxShape *pShape;
	pRigidBody->getShapes(&pShape, 1); // get only first shape, but it can be several
	Vector vecOffset = (pEntity->IsMonster()) ? Vector( 0, 0, pEntity->pev->maxs.z / 2.0f ) : g_vecZero;

	if (pShape->getGeometryType() == PxGeometryType::eBOX)
	{
		PxBoxGeometry &box = pShape->getGeometry().box();
		PxTransform pose = pRigidBody->getGlobalPose();
		box.halfExtents = pEntity->pev->size * k_PaddingFactor;
		pose.p = (pEntity->GetAbsOrigin() + vecOffset);
		pRigidBody->setGlobalPose(pose);
	}
	else {
		ALERT(at_error, "TeleportCharacter: shape geometry type is not a box\n");
	}
}

void CPhysicPhysX :: TeleportActor( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: TeleportActor( entity = %x )\n", pEntity);
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	matrix4x4 m(pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f);
	PxTransform pose = PxTransform(PxMat44(m));
	pRigidBody->setGlobalPose( pose );
}

void CPhysicPhysX :: MoveCharacter( CBaseEntity *pEntity )
{
	if (!pEntity || pEntity->m_vecOldPosition == pEntity->pev->origin)
		return;

	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return;

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (!pRigidBody)
		return;

	if (TracingStateChanges(pActor))
	{
		ALERT(at_console,
			"PhysX: MoveCharacter( entity = %x, from = (%.2f, %.2f, %.2f), to = (%.2f, %.2f, %.2f) )\n",
			pEntity,
			pEntity->m_vecOldPosition.x,
			pEntity->m_vecOldPosition.y,
			pEntity->m_vecOldPosition.z,
			pEntity->pev->origin.x,
			pEntity->pev->origin.y,
			pEntity->pev->origin.z
		);
	}

	PxShape *pShape;
	pRigidBody->getShapes(&pShape, 1); // get only first shape, but it can be several

	if (pEntity->IsPlayer())
	{
		// if we're in NOCLIP or FLY (ladder climbing) mode - disable collisions
		if (pEntity->pev->movetype != MOVETYPE_WALK)
			ToggleCollision(pRigidBody, false);
		else
			ToggleCollision(pRigidBody, true);
	}

	UpdateCharacterBounds(pEntity, pShape);
	Vector vecOffset = (pEntity->IsMonster()) ? Vector(0, 0, pEntity->pev->maxs.z / 2.0f) : g_vecZero;
	PxTransform pose = pRigidBody->getGlobalPose();
	pose.p = (pEntity->GetAbsOrigin() + vecOffset);
	pRigidBody->setKinematicTarget(pose);
	pEntity->m_vecOldPosition = pEntity->GetAbsOrigin(); // update old position
}

void CPhysicPhysX::UpdateCharacterBounds(CBaseEntity *pEntity, PxShape *pShape)
{
	if (pEntity->pev->size == pEntity->m_vecOldBounds)
		return;

	PxBoxGeometry box;
	box.halfExtents = pEntity->pev->size * k_PaddingFactor;
	pShape->setGeometry(box);
	pEntity->m_vecOldBounds = pEntity->pev->size;
}

void CPhysicPhysX :: MoveKinematic( CBaseEntity *pEntity )
{
	if( !pEntity || ( pEntity->pev->movetype != MOVETYPE_PUSH && pEntity->pev->movetype != MOVETYPE_PUSHSTEP ))
		return;	// probably not a mover

	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (!pRigidBody)
		return;

	if( pEntity->pev->solid == SOLID_NOT || pEntity->pev->solid == SOLID_TRIGGER )
		ToggleCollision(pRigidBody, false);
	else
		ToggleCollision(pRigidBody, true);

	PxTransform pose;
	matrix4x4 m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles( ), 1.0f );

	// complex move for kinematic entities
	pose = PxTransform(PxMat44(m));
	pRigidBody->setKinematicTarget( pose );
}

void CPhysicPhysX :: EnableCollision( CBaseEntity *pEntity, int fEnable )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return;

	// The generic collision toggle also applies to PxRigidStatic actors.
	// Casting them to PxRigidDynamic and dereferencing would crash.
	PxRigidActor *pRigidBody = pActor->is<PxRigidActor>();
	if( !pRigidBody )
		return;

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: EnableCollision( entity = %x, state = %s )\n", pEntity, fEnable ? "true" : "false");
	}

	if (fEnable)
	{
		ToggleCollision(pRigidBody, true);
		pActor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
	}
	else
	{
		ToggleCollision(pRigidBody, false);
		pActor->setActorFlag(PxActorFlag::eVISUALIZATION, false);
	}
	auto state = m_physicsAssetCollisionStates.find( pEntity->entindex() );
	if( state != m_physicsAssetCollisionStates.end() )
	{
		state->second.collisionRequested = fEnable != 0;
		SetAnimatedCollisionEnabled( state->second, fEnable != 0 );
		UpdateEntityAABB( pEntity );
	}
}

void CPhysicPhysX :: MakeKinematic( CBaseEntity *pEntity, int fEnable )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody || pRigidBody->getNbShapes() <= 0)
		return;

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: MakeKinematic( entity = %x, state = %s )\n", pEntity, fEnable ? "true" : "false");
	}

	if (fEnable)
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	else
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
}

void CPhysicPhysX :: SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	PxActor *pActor = ActorFromEntity( pTouch );
	if (!pActor)
	{
		// bad actor?
		tr->allsolid = false;
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	auto assetStateIt = m_physicsAssetCollisionStates.find( pTouch->entindex() );
	bool animatedCollisionActive = false;
	if( assetStateIt != m_physicsAssetCollisionStates.end() )
	{
		for( const PhysicsPartRuntime &part : assetStateIt->second.parts )
        {
            for( const PhysicsPartShape &section : part.shapes )
            {
                if( !section.shape ) continue;
                const PxShapeFlags flags = section.shape->getFlags();
                if(( flags & PxShapeFlag::eSIMULATION_SHAPE ) || ( flags & PxShapeFlag::eSCENE_QUERY_SHAPE ))
                {
                    animatedCollisionActive = true;
                    break;
                }
            }
            if( animatedCollisionActive ) break;
        }
	}
    // An active hinged child still owns movement collision if the root has
    // no shapes or its own collision has been disabled.
    const bool rootHasShapes = pRigidActor && pRigidActor->getNbShapes() > 0;
    const auto sweepOwners = physicsasset::SelectMovementSweepOwners(
        pRigidActor != nullptr, rootHasShapes,
        rootHasShapes && CheckCollision(pRigidActor), animatedCollisionActive);
    if( !sweepOwners.Any() )
    {
        tr->allsolid = false;
        tr->fraction = 1.0f;
        return;
    }

	Vector trace_mins, trace_maxs;
	UTIL_MoveBounds(start, mins, maxs, end, trace_mins, trace_maxs);

	// NOTE: pmove code completely ignore a bounds checking. So we need to do it here
	if (!BoundsIntersect(trace_mins, trace_maxs, pTouch->pev->absmin, pTouch->pev->absmax))
	{
		tr->allsolid = false;
		return;
	}

	DecomposedShape shape;
	model_t *mod = (model_t *)MODEL_HANDLE(pTouch->pev->modelindex);
	const bool materialTrace = mod && mod->type == mod_studio && FBitSet( gpGlobals->trace_flags, FTRACE_MATERIAL_TRACE );
	const bool hasPhysicsAssetCollision = mod && mod->type == mod_studio && HasPhysicsAssetCollision( pTouch );

    trace_t primitiveOnlyResult{};
    bool havePrimitiveOnlyResult = false;
    const std::shared_ptr<PhysicsAssetDef> traceAsset =
        assetStateIt != m_physicsAssetCollisionStates.end() ? GetPhysicsAssetDefinition(pTouch) : nullptr;
    const bool keepLegacyRoot = traceAsset && traceAsset->collisions.empty();
	// v3 bone-bound collision changes with the current Studio pose. Trace the
	// per-entity mesh rebuilt from current bone matrices instead of a cached
	// reference-pose PhysX decomposition. This keeps player/NPC sweeps and
	// bullets aligned with animated doors, hoods, turrets, machinery, etc.
	if( assetStateIt != m_physicsAssetCollisionStates.end() &&
		assetStateIt->second.animatedTraceMesh && assetStateIt->second.animatedTraceMesh->GetMesh() )
	{
		CMeshDesc &animatedMesh = *assetStateIt->second.animatedTraceMesh;
		TraceMesh animatedTrace;
		animatedTrace.SetTraceMesh( animatedMesh.GetMesh(), animatedMesh.GetHeadNode(),
			pTouch->pev->modelindex, pTouch->pev->body, pTouch->pev->skin );
		animatedTrace.SetMeshOrientation( pTouch->pev->origin, pTouch->pev->angles, pTouch->GetScale() );
        trace_t animatedResult{};
        animatedTrace.SetupTrace( start, mins, maxs, end, &animatedResult );
        if( animatedTrace.DoTrace() )
        {
            if( animatedResult.fraction < 1.0f || animatedResult.startsolid )
                animatedResult.ent = pTouch->edict();
            animatedResult.materialHash = 0;
            if( materialTrace && (animatedResult.fraction < 1.0f || animatedResult.startsolid))
            {
                Vector visualPoint, visualNormal;
                uint32_t visualMaterial = 0;
                if( ResolveRenderImpact(pTouch,start,end,visualPoint,visualNormal,visualMaterial))
                    animatedResult.materialHash = visualMaterial;
            }
        }
        if( !keepLegacyRoot )
        {
            *tr = animatedResult;
            return;
        }
        // Part-only .phys adds a hinge, not a replacement chassis collider.
        primitiveOnlyResult = animatedResult;
        havePrimitiveOnlyResult = true;
	}

    // Only the child trace is valid when the root has no active shapes.
    // Avoid decomposing a disabled/shape-less chassis, and retain the hit
    // returned by the live hinged child movement geometry.
    if( !sweepOwners.root )
    {
        if( havePrimitiveOnlyResult ) *tr = primitiveOnlyResult;
        else { tr->allsolid = false; tr->fraction = 1.0f; }
        return;
    }

	auto &meshDescFactory = CMeshDescFactory::Instance();
	clipfile::GeometryType geomType = ShapeTypeToGeomType(shape.GetGeometryType(pRigidActor));
	CMeshDesc &cookedMesh = meshDescFactory.CreateObject(pTouch->pev->modelindex, pTouch->pev->body, pTouch->pev->skin, geomType,
        hasPhysicsAssetCollision ? GetPhysicsAssetDefinition(pTouch)->collisionFingerprint : 0);

	if (!cookedMesh.GetMesh())
	{
		const bool presentInCache = cookedMesh.LoadFromCacheFile();
		if (!presentInCache)
		{
			// update cache or build from scratch
			Vector triangle[3];
			if (!shape.Triangulate(pRigidActor))
			{
				// failed to triangulate, unsupported mesh type, so skip them
                if(havePrimitiveOnlyResult) *tr = primitiveOnlyResult;
                else tr->allsolid = false;
                return;
			}

			cookedMesh.SetDebugName(pTouch->GetModel());
			cookedMesh.InitMeshBuild(shape.GetTrianglesCount());

			// FIXME: store all meshes as local and use capsule instead of bbox
			const auto &indexBuffer = shape.GetIndexBuffer();
			const auto &vertexBuffer = shape.GetVertexBuffer();
			for (size_t i = 0; i < shape.GetTrianglesCount(); i++)
			{
				uint32_t i0 = indexBuffer[3 * i];
				uint32_t i1 = indexBuffer[3 * i + 1];
				uint32_t i2 = indexBuffer[3 * i + 2];
				vec3_t v0 = vertexBuffer[i0];
				vec3_t v1 = vertexBuffer[i1];
				vec3_t v2 = vertexBuffer[i2];
				triangle[0] = v0;
				triangle[1] = v1;
				triangle[2] = v2;
				cookedMesh.AddMeshTrinagle(triangle);
			}
		}
		if (!cookedMesh.FinishMeshBuild())
		{
			ALERT(at_error, "failed to build cooked mesh from %s\n", pTouch->GetModel());
            if(havePrimitiveOnlyResult) *tr = primitiveOnlyResult;
            else tr->allsolid = false;
            return;
		}
		else
		{
			if (!presentInCache) {
				cookedMesh.SaveToCacheFile();
			}
			cookedMesh.FreeMeshBuild();
		}
	}

	mmesh_t *pMesh;
	areanode_t *pHeadNode;
	int32_t meshBody, meshSkin;

	// Preserve original PrimeXT exactly when the model has no .phys collision
	// geometry. With PhysicsAsset collision present, the cooked PhysX shape stays
	// authoritative and the render model is consulted only for effects.
	if( materialTrace && !hasPhysicsAssetCollision )
	{
		CMeshDesc &originalMesh = meshDescFactory.CreateObject(pTouch->pev->modelindex, pTouch->pev->body, pTouch->pev->skin, clipfile::GeometryType::Original);
		if (!originalMesh.GetMesh())
		{
			originalMesh.StudioConstructMesh();
			if (!originalMesh.GetMesh()) {
				ALERT(at_error, "failed to build original mesh from %s\n", pTouch->GetModel());
			}
		}

		meshBody = originalMesh.GetBody();
		meshSkin = originalMesh.GetSkin();
		pMesh = originalMesh.GetMesh();
		pHeadNode = originalMesh.GetHeadNode();
	}
	else
	{
		meshBody = cookedMesh.GetBody();
		meshSkin = cookedMesh.GetSkin();
		pMesh = cookedMesh.GetMesh();
		pHeadNode = cookedMesh.GetHeadNode();
	}

	TraceMesh trm;
	trm.SetTraceMesh(pMesh, pHeadNode, pTouch->pev->modelindex, meshBody, meshSkin);
	trm.SetMeshOrientation(pTouch->pev->origin, pTouch->pev->angles, pTouch->GetScale());
	trm.SetupTrace(start, mins, maxs, end, tr);

	if (trm.DoTrace())
	{
		if( tr->fraction < 1.0f || tr->startsolid )
			tr->ent = pTouch->edict();

		tr->materialHash = COM_GetMaterialHash( trm.GetLastHitSurface() );

		if( materialTrace && hasPhysicsAssetCollision && ( tr->fraction < 1.0f || tr->startsolid ))
		{
			Vector visualPoint, visualNormal;
			uint32_t visualMaterial = 0;
			if( ResolveRenderImpact( pTouch, start, end, visualPoint, visualNormal, visualMaterial ))
				tr->materialHash = visualMaterial;
			else
				tr->materialHash = 0; // .phys collision has no render material authority
		}
	}
    if( havePrimitiveOnlyResult &&
        ((primitiveOnlyResult.startsolid && !tr->startsolid) ||
         primitiveOnlyResult.fraction < tr->fraction))
        *tr = primitiveOnlyResult;
}

void CPhysicPhysX :: SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, TraceResult *tr )
{
	// make trace default
	memset( tr, 0, sizeof( *tr ));
	tr->flFraction = 1.0f;
	tr->vecEndPos = end;

	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor || pRigidActor->getNbShapes() <= 0 || pEntity->pev->solid == SOLID_NOT)
		return; // only dynamic solid objects can be traced

	// test for stuck entity into another
	if (start == end)
	{
		Vector triangle[3], dirs[3];
		PxTransform globalPose = pRigidActor->getGlobalPose();
		DecomposedShape shape;
		if (!shape.Triangulate(pRigidActor)) {
			return; // failed to triangulate
		}

		const auto &indexBuffer = shape.GetIndexBuffer();
		const auto &vertexBuffer = shape.GetVertexBuffer();
		for (size_t i = 0; i < shape.GetTrianglesCount(); i++)
		{
			uint32_t i0 = indexBuffer[3 * i];
			uint32_t i1 = indexBuffer[3 * i + 1];
			uint32_t i2 = indexBuffer[3 * i + 2];
			vec3_t v0 = vertexBuffer[i0];
			vec3_t v1 = vertexBuffer[i1];
			vec3_t v2 = vertexBuffer[i2];

			// transform triangles from local to world space
			triangle[0] = globalPose.transform(v0);
			triangle[1] = globalPose.transform(v1);
			triangle[2] = globalPose.transform(v2);

			for (size_t j = 0; j < 3; j++)
			{
				dirs[j] = globalPose.p - triangle[j];
				triangle[j] += dirs[j] * -2.0f;
				UTIL_TraceLine(triangle[j], triangle[j], ignore_monsters, pEntity->edict(), tr);
				if (tr->fStartSolid)
					return;	// one of points in solid
			}
		}
		return;
	}

	// compute motion
	Vector vecDir = end - start;
	float flLength = vecDir.Length();
	vecDir = vecDir.Normalize();

	// setup trace box
	PxBoxGeometry testBox;
	PxTransform initialPose = pRigidActor->getGlobalPose();
	initialPose.p = pEntity->Center(); // does we really need to do this?
	testBox.halfExtents = pEntity->pev->size * k_PaddingFactor;

	// make a linear sweep through the world
	// we need to disable collision here to avoid touching same actor as we trying to sweep
	PxSweepBuffer sweepResult;
	bool hitOccured = m_pScene->sweep(testBox, initialPose, vecDir, flLength, sweepResult, PxHitFlag::eNORMAL);

	if (!hitOccured || sweepResult.getNbAnyHits() < 1)
		return; // no intersection

	const PxSweepHit &hit = sweepResult.getAnyHit(0);
	if (hit.distance > flLength || !hit.actor)
		return; // hit missed

	// compute fraction
	tr->flFraction = (hit.distance / flLength);
	tr->flFraction = bound( 0.0f, tr->flFraction, 1.0f );
	VectorLerp( start, tr->flFraction, end, tr->vecEndPos );

	CBaseEntity *pHit = EntityFromActor( hit.actor );
	if (pHit) {
		tr->pHit = pHit->edict();
	}

	tr->vecPlaneNormal = hit.normal;
	tr->flPlaneDist = DotProduct( tr->vecEndPos, tr->vecPlaneNormal );
	float flDot = DotProduct( vecDir, tr->vecPlaneNormal );
	float moveDot = Q_round( flDot, 0.1f );

	// FIXME: this is incorrect. Find a better method?
	if(( tr->flFraction < 0.1f ) && ( moveDot < 0.0f ))
		tr->fAllSolid = true;
}


PhysicsAssetInstance *CPhysicPhysX::FindPhysicsAsset( CBaseEntity *pEntity )
{
	for( auto &asset : m_physicsAssets )
		if( asset->Owner() == pEntity ) return asset.get();
	return nullptr;
}

void CPhysicPhysX::SuspendLegacyCollisionForRagdoll( CBaseEntity *pEntity, PxRigidActor *actor )
{
	if( !pEntity || !actor ) return;
	const int entityIndex = ENTINDEX( pEntity->edict() );

	RagdollLegacyCollisionState state;
	state.actor = actor;
    // Remember the starting selection and the gameplay's requested state.
    // If neither changes during ragdoll, restore the exact old shape flags
    // (simulation and scene-query flags may intentionally be different).
    state.renderBodyAtSuspend = pEntity->pev->body;
    auto tracked = m_physicsAssetCollisionStates.find(entityIndex);
    if( tracked != m_physicsAssetCollisionStates.end() && tracked->second.edict == pEntity->edict() )
        state.collisionRequestedAtSuspend = tracked->second.collisionRequested;
    else
        state.collisionRequestedAtSuspend = CheckCollision(actor);
	const PxU32 shapeCount = actor->getNbShapes();
	if( shapeCount > 0 )
	{
		std::vector<PxShape *> shapes( shapeCount );
		actor->getShapes( shapes.data(), shapeCount );
		state.shapes.reserve( shapeCount );
		for( PxShape *shape : shapes )
		{
			LegacyShapeCollisionState shapeState;
			const PxShapeFlags flags = shape->getFlags();
			shapeState.simulation = flags & PxShapeFlag::eSIMULATION_SHAPE;
			shapeState.sceneQuery = flags & PxShapeFlag::eSCENE_QUERY_SHAPE;
			state.shapes.push_back( shapeState );
			shape->setFlag( PxShapeFlag::eSIMULATION_SHAPE, false );
			shape->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, false );
		}
	}
	m_ragdollLegacyCollisionState[entityIndex] = std::move( state );
}

void CPhysicPhysX::RestoreLegacyCollisionAfterRagdoll( CBaseEntity *pEntity )
{
	if( !pEntity ) return;
	const int entityIndex = ENTINDEX( pEntity->edict() );
	auto stored = m_ragdollLegacyCollisionState.find( entityIndex );
	if( stored == m_ragdollLegacyCollisionState.end() ) return;

	PxActor *currentActor = ActorFromEntity( pEntity );
	PxRigidActor *currentRigid = currentActor ? currentActor->is<PxRigidActor>() : nullptr;
	RagdollLegacyCollisionState &state = stored->second;
	if( currentRigid && currentRigid == state.actor )
	{
		const PxU32 shapeCount = currentRigid->getNbShapes();
		std::vector<PxShape *> shapes( shapeCount );
		if( shapeCount > 0 ) currentRigid->getShapes( shapes.data(), shapeCount );
		const size_t restoreCount = std::min( shapes.size(), state.shapes.size() );
		for( size_t i = 0; i < restoreCount; ++i )
		{
			shapes[i]->setFlag( PxShapeFlag::eSIMULATION_SHAPE, state.shapes[i].simulation );
			shapes[i]->setFlag( PxShapeFlag::eSCENE_QUERY_SHAPE, state.shapes[i].sceneQuery );
		}
	}
    const int32_t savedRenderBody = state.renderBodyAtSuspend;
    const bool savedCollisionRequest = state.collisionRequestedAtSuspend;
    const bool rootActorWasReplaced = currentRigid != state.actor;
	m_ragdollLegacyCollisionState.erase( stored );
    // Restore the old shape flags first (important for non-.phys shadows),
    // then make sure today's bodygroup selection and gameplay request still
    // agree with the restored collision. A changed .phys bodygroup must win.
    auto it = m_physicsAssetCollisionStates.find(entityIndex);
    if( it != m_physicsAssetCollisionStates.end() && it->second.edict == pEntity->edict() )
    {
        const auto asset = GetPhysicsAssetDefinition(pEntity);
        const bool managedRoot = asset && physicsasset::HasCollisionGeometry(*asset);
        const bool needsNewCollisionState =
            savedRenderBody != it->second.renderBody ||
            savedCollisionRequest != it->second.collisionRequested ||
            rootActorWasReplaced;
        // If the original actor still exists and nothing changed, preserve
        // its exact pre-ragdoll simulation/query flags. Replaced actors have
        // no valid saved flags, so rebuild their collision from policy.
        ApplyPhysicsAssetRootCollision(pEntity, it->second,
            needsNewCollisionState && (managedRoot || rootActorWasReplaced));
    }
}


void CPhysicPhysX::SuspendEntityForRagdoll( CBaseEntity *pEntity, PhysicsAssetInstance *asset )
{
	if( !pEntity ) return;
	const int entityIndex = ENTINDEX( pEntity->edict() );
	if( m_ragdollEntityState.find( entityIndex ) == m_ragdollEntityState.end() )
	{
		RagdollEntityState state;
		state.solid = pEntity->pev->solid;
		state.movetype = pEntity->pev->movetype;
		state.mins = pEntity->pev->mins;
		state.maxs = pEntity->pev->maxs;
		state.angles = pEntity->GetAbsAngles();
		PxTransform rootPose;
		if( asset && asset->GetRootBonePose( rootPose ) && rootPose.isValid() )
		{
			matrix4x4 rootWorld = MatrixFromPxTransform( rootPose );
			state.rootToEntity = rootWorld.Invert().ConcatTransforms( pEntity->EntityToWorldTransform() );
			state.hasRootRelation = true;
		}
		m_ragdollEntityState.emplace( entityIndex, state );
	}

	// The articulated .phys bodies now own physical contacts. Leaving the old
	// GoldSrc hull active would create an invisible second collider around the
	// corpse and can also let MOVETYPE_STEP move the entity independently of the
	// ragdoll root.
	SetBits( pEntity->pev->iuser1, CF_RAGDOLL );
	pEntity->pev->solid = SOLID_NOT;
	pEntity->pev->movetype = MOVETYPE_NONE;
	pEntity->SetAbsVelocity( g_vecZero );
	pEntity->SetAbsAvelocity( g_vecZero );
	pEntity->pev->groundentity = nullptr;
	ClearBits( pEntity->pev->flags, FL_ONGROUND );
	pEntity->RelinkEntity( TRUE );
}

void CPhysicPhysX::RestoreEntityAfterRagdoll( CBaseEntity *pEntity )
{
	if( !pEntity ) return;
	const int entityIndex = ENTINDEX( pEntity->edict() );
	auto it = m_ragdollEntityState.find( entityIndex );
	if( it == m_ragdollEntityState.end() ) return;

	ClearBits( pEntity->pev->iuser1, CF_RAGDOLL );
	pEntity->pev->solid = it->second.solid;
	pEntity->pev->movetype = it->second.movetype;
	UTIL_SetSize( pEntity->pev, it->second.mins, it->second.maxs );
	pEntity->SetAbsAngles( it->second.angles );
	pEntity->SetAbsVelocity( g_vecZero );
	pEntity->SetAbsAvelocity( g_vecZero );
	pEntity->RelinkEntity( TRUE );
	m_ragdollEntityState.erase( it );
}

void CPhysicPhysX::ClearRagdollPose( CBaseEntity *pEntity )
{
	if( !pEntity ) return;
	ClearBits( pEntity->pev->iuser1, CF_RAGDOLL );

	const int entityIndex = ENTINDEX( pEntity->edict() );
	m_ragdollNetworkState.erase( entityIndex );
	if( !gmsgRagdollPose ) return;

	// Every snapshot carries a global monotonically increasing sequence, so an
	// old pose packet can never resurrect a corpse after this clear (including
	// when an edict index is later reused by another entity).
	++m_ragdollSnapshotSequence;
	if( m_ragdollSnapshotSequence == 0 ) ++m_ragdollSnapshotSequence;

	MESSAGE_BEGIN( MSG_PVS, gmsgRagdollPose, pEntity->GetAbsOrigin() );
		WRITE_ENTITY( entityIndex );
		WRITE_BYTE( ragdollnet::kProtocolVersion );
		WRITE_BYTE( ragdollnet::SnapshotClear );
		WRITE_LONG( static_cast<int32_t>( m_ragdollSnapshotSequence ) );
		WRITE_BYTE( 0 ); // root bone unused for clear
		WRITE_BYTE( 0 ); // sparse non-root bone count
		WRITE_FLOAT( 0.0f ); WRITE_FLOAT( 0.0f ); WRITE_FLOAT( 0.0f );
		WRITE_SHORT( 0 ); WRITE_SHORT( 0 ); WRITE_SHORT( 0 ); WRITE_SHORT( 32767 );
	MESSAGE_END();
}

void CPhysicPhysX::SendRagdollPose( PhysicsAssetInstance *asset )
{
	if( !asset || !asset->IsRagdoll() || !asset->HasSimulationPose() || !gmsgRagdollPose ) return;
	CBaseEntity *owner = asset->Owner();
	if( !owner || owner->pev->modelindex <= 0 ) return;

	studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR( owner->edict() );
	if( !hdr || hdr->numbones <= 0 ) return;

	const int rootBody = asset->RootBodyIndex();
	if( rootBody < 0 || rootBody >= static_cast<int>( asset->Bodies().size() )) return;
	const auto &rootRuntime = asset->Bodies()[rootBody];
	if( !rootRuntime.hasSimulationPose || rootRuntime.boneIndex < 0 || rootRuntime.boneIndex >= hdr->numbones || rootRuntime.boneIndex >= MAXSTUDIOBONES ) return;

	const bool sleeping = asset->IsSleeping();
	const bool frozen = asset->IsFrozen();
	const float now = gpGlobals ? gpGlobals->time : 0.0f;
	const int entityIndex = ENTINDEX( owner->edict() );
	RagdollNetworkState &net = m_ragdollNetworkState[entityIndex];

	// Active ragdolls are sampled at 20 Hz. Once every body sleeps, a full
	// self-contained refresh at 2 Hz is enough to serve clients entering PVS
	// without burning bandwidth forever. Waking/sleeping transitions send now.
	constexpr float kActiveInterval = 1.0f / 20.0f;
	constexpr float kSleepingInterval = 0.5f;
	constexpr float kFrozenInterval = 2.0f;
	const bool timeRewound = net.initialized && now + 1e-6f < net.lastSendTime;
	const bool stateChanged = !net.initialized || net.sleeping != sleeping || net.frozen != frozen || timeRewound;
	if( !stateChanged && now + 1e-6f < net.nextSendTime ) return;

	const PxTransform rootWorld = rootRuntime.simulatedBonePose;
	if( !rootWorld.isValid() ) return;
	const PxTransform worldToRoot = rootWorld.getInverse();

	struct WireBone
	{
		byte index = 0;
		std::int16_t p[3] = { 0, 0, 0 };
		std::int16_t q[4] = { 0, 0, 0, 32767 };
	};
	WireBone wire[MAXSTUDIOBONES];
	size_t wireCount = 0;

	bool quantizationOK = true;
	for( int i = 0; i < static_cast<int>( asset->Bodies().size() ); ++i )
	{
		if( i == rootBody ) continue;
		const auto &body = asset->Bodies()[i];
		if( !body.hasSimulationPose || body.boneIndex < 0 || body.boneIndex >= hdr->numbones || body.boneIndex >= MAXSTUDIOBONES )
			continue;

		PxTransform relative = worldToRoot * body.simulatedBonePose;
		if( !relative.isValid() ) continue;
		relative.q.normalize();

		WireBone out;
		out.index = static_cast<byte>( body.boneIndex );
		out.p[0] = QuantizeRagdollRelativePosition( relative.p.x, quantizationOK );
		out.p[1] = QuantizeRagdollRelativePosition( relative.p.y, quantizationOK );
		out.p[2] = QuantizeRagdollRelativePosition( relative.p.z, quantizationOK );
		out.q[0] = QuantizeRagdollQuaternion( relative.q.x );
		out.q[1] = QuantizeRagdollQuaternion( relative.q.y );
		out.q[2] = QuantizeRagdollQuaternion( relative.q.z );
		out.q[3] = QuantizeRagdollQuaternion( relative.q.w );
		if( wireCount >= MAXSTUDIOBONES - 1 ) return;
		wire[wireCount++] = out;
	}

	if( !quantizationOK )
	{
		ALERT( at_warning, "%s: ragdoll pose exceeds +/-4095.875 units from root; snapshot skipped\n", owner->GetClassname() );
		return;
	}
	PxQuat rootQ = rootWorld.q;
	rootQ.normalize();
	++m_ragdollSnapshotSequence;
	if( m_ragdollSnapshotSequence == 0 ) ++m_ragdollSnapshotSequence;

	MESSAGE_BEGIN( MSG_PVS, gmsgRagdollPose, owner->GetAbsOrigin() );
		WRITE_ENTITY( entityIndex );
		WRITE_BYTE( ragdollnet::kProtocolVersion );
		WRITE_BYTE(( sleeping ? ragdollnet::SnapshotSleeping : 0 ) | ( frozen ? ragdollnet::SnapshotFrozen : 0 ));
		WRITE_LONG( static_cast<int32_t>( m_ragdollSnapshotSequence ) );
		WRITE_BYTE( rootRuntime.boneIndex );
		WRITE_BYTE( static_cast<int>( wireCount ) );
		WRITE_FLOAT( rootWorld.p.x );
		WRITE_FLOAT( rootWorld.p.y );
		WRITE_FLOAT( rootWorld.p.z );
		WRITE_SHORT( QuantizeRagdollQuaternion( rootQ.x ) );
		WRITE_SHORT( QuantizeRagdollQuaternion( rootQ.y ) );
		WRITE_SHORT( QuantizeRagdollQuaternion( rootQ.z ) );
		WRITE_SHORT( QuantizeRagdollQuaternion( rootQ.w ) );
		for( size_t wireIndex = 0; wireIndex < wireCount; ++wireIndex )
		{
			const WireBone &bone = wire[wireIndex];
			WRITE_BYTE( bone.index );
			WRITE_SHORT( bone.p[0] ); WRITE_SHORT( bone.p[1] ); WRITE_SHORT( bone.p[2] );
			WRITE_SHORT( bone.q[0] ); WRITE_SHORT( bone.q[1] ); WRITE_SHORT( bone.q[2] ); WRITE_SHORT( bone.q[3] );
		}
	MESSAGE_END();

	SetBits( owner->pev->iuser1, CF_RAGDOLL );
	net.initialized = true;
	net.sleeping = sleeping;
	net.frozen = frozen;
	net.lastSendTime = now;
	net.nextSendTime = now + ( frozen ? kFrozenInterval : ( sleeping ? kSleepingInterval : kActiveInterval ));
}

void CPhysicPhysX::UpdateRagdollPresentation( PhysicsAssetInstance *asset )
{
	if( !asset || !asset->IsRagdoll() || !asset->HasSimulationPose() ) return;
	CBaseEntity *owner = asset->Owner();
	if( !owner ) return;

	PxTransform rootPose;
	if( !asset->GetRootBonePose( rootPose ) || !rootPose.isValid() ) return;

	// Partial ragdoll / physical-animation mode keeps normal GoldSrc movement
	// ownership. The .phys bodies are an articulated visual/query layer driven
	// around the animation pose, so never drag the entity origin/hull behind a
	// temporarily displaced limb/root. Full weight=1 ragdolls use the D behavior
	// below and make the articulated body authoritative for presentation bounds.
	if( asset->NeedsAnimationControl() )
	{
		SetBits( owner->pev->iuser1, CF_RAGDOLL );
		SendRagdollPose( asset );
		return;
	}

	Vector rootOrigin( rootPose.p.x, rootPose.p.y, rootPose.p.z );
	Vector entityOrigin = rootOrigin;
	Vector entityAngles = owner->GetAbsAngles();

	const int entityIndex = ENTINDEX( owner->edict() );
	auto stateIt = m_ragdollEntityState.find( entityIndex );
	if( stateIt != m_ragdollEntityState.end() && stateIt->second.hasRootRelation )
	{
		matrix4x4 rootWorld = MatrixFromPxTransform( rootPose );
		matrix4x4 entityWorld = rootWorld.ConcatTransforms( stateIt->second.rootToEntity );
		entityWorld.GetOrigin( entityOrigin );
		entityWorld.GetAngles( entityAngles );
	}

	// Keep the network/PVS/entity frame attached to the ragdoll while preserving
	// its authored relationship to the selected physics root. This avoids moving
	// a feet-origin model to its pelvis when ragdoll starts. Ragdoll snapshots
	// carry their own world-space root plus root-relative children, so rendering
	// does not depend on packet ordering of this entity transform.
	owner->SetAbsOrigin( entityOrigin );
	owner->SetAbsAngles( entityAngles );

	PxBounds3 bounds;
	if( asset->GetSimulationBounds( bounds ) )
	{
		// curstate.mins/maxs are interpreted in entity-local coordinates and the
		// client renderer rotates them by entity angles. Convert the PhysX world
		// AABB into this frame first instead of rotating a world AABB twice.
		const matrix4x4 worldToEntity = matrix4x4( entityOrigin, entityAngles, 1.0f ).Invert();
		Vector mins, maxs;
		ClearBounds( mins, maxs );
		for( int x = 0; x < 2; ++x )
		for( int y = 0; y < 2; ++y )
		for( int z = 0; z < 2; ++z )
		{
			const Vector corner(
				x ? bounds.maximum.x : bounds.minimum.x,
				y ? bounds.maximum.y : bounds.minimum.y,
				z ? bounds.maximum.z : bounds.minimum.z );
			AddPointToBounds( worldToEntity.VectorTransform( corner ), mins, maxs );
		}
		ExpandBounds( mins, maxs, 8.0f );
		UTIL_SetSize( owner->pev, mins, maxs );
	}

	const int rootIndex = asset->RootBodyIndex();
	if( rootIndex >= 0 && rootIndex < static_cast<int>(asset->Bodies().size()) )
	{
		const auto &rootBody = asset->Bodies()[rootIndex];
		if( rootBody.actor )
		{
			const PxVec3 velocity = rootBody.actor->getLinearVelocity();
			owner->SetAbsVelocity( Vector( velocity.x, velocity.y, velocity.z ) );
		}
	}

	owner->RelinkEntity( TRUE );
	SendRagdollPose( asset );
}

PhysicsAssetInstance *CPhysicPhysX::EnsurePhysicsAssetInstance( CBaseEntity *pEntity )
{
	if( !pEntity ) return nullptr;
	if( PhysicsAssetInstance *existing = FindPhysicsAsset( pEntity ))
	{
		if( existing->MatchesOwnerModel() )
			return existing;

		// SET_MODEL can legitimately replace a studio model on a live entity. Do
		// not carry bone indices/history/actors from the old .mdl/.phys pair into
		// the new one. Purge callback-held actor pointers before releasing it.
		if( m_eventHandler )
		{
			std::vector<PxActor *> actors;
			existing->CollectActors( actors );
			for( PxActor *actor : actors )
				m_eventHandler->purgeActorReferences( actor );
		}

		if( existing->IsRagdoll() )
		{
			ClearRagdollPose( pEntity );
			RestoreLegacyCollisionAfterRagdoll( pEntity );
			RestoreEntityAfterRagdoll( pEntity );
		}
		else
		{
			m_ragdollLegacyCollisionState.erase( ENTINDEX( pEntity->edict() ));
			m_ragdollEntityState.erase( ENTINDEX( pEntity->edict() ));
		}

		m_pendingRagdollImpulses.erase( ENTINDEX( pEntity->edict() ));
		m_physicsAssets.erase( std::remove_if( m_physicsAssets.begin(), m_physicsAssets.end(),
			[pEntity]( const std::unique_ptr<PhysicsAssetInstance> &asset ) { return asset->Owner() == pEntity; } ), m_physicsAssets.end() );
	}

	auto def = GetPhysicsAssetDefinition( pEntity );
	if( !def || !physicsasset::HasArticulatedBodies( *def )) return nullptr;

	auto instance = std::make_unique<PhysicsAssetInstance>( pEntity, def );
	if( !instance->InitializePoseTracking() )
	{
		ALERT( at_warning, "Failed to initialize physics-asset pose tracking %s for %s\n",
			def->sourcePath.c_str(), pEntity->GetClassname() );
		return nullptr;
	}

	PhysicsAssetInstance *result = instance.get();
	m_physicsAssets.push_back( std::move( instance ));
	return result;
}

std::shared_ptr<PhysicsAssetDef> CPhysicPhysX::GetPhysicsAssetDefinition( CBaseEntity *pEntity )
{
	if( !pEntity || !pEntity->pev->model ) return nullptr;
	const char *modelName = STRING( pEntity->pev->model );
	if( !modelName || !modelName[0] ) return nullptr;

	const std::string key( modelName );
	auto cached = m_physicsAssetDefinitions.find( key );
	if( cached != m_physicsAssetDefinitions.end() )
		return cached->second;

	if( pEntity->pev->modelindex <= 0 )
		return nullptr;
	if( UTIL_GetModelType( pEntity->pev->modelindex ) != mod_studio )
	{
		m_physicsAssetDefinitions.emplace( key, nullptr );
		return nullptr;
	}

	auto def = LoadPhysicsAssetDefinition( modelName );
	if( def )
	{
		studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR( pEntity->edict() );
		if( !hdr ) return nullptr; // transient model-cache state; do not negative-cache

		// Collision-only v2/v3 assets do not require an articulated skeleton. If the
		// asset also has ragdoll bodies, validate every referenced bone as before.
		if( physicsasset::HasArticulatedBodies( *def ))
		{
			if( hdr->numbones <= 0 )
			{
				ALERT( at_warning, "%s: articulated bodies require a studio model with bones\n", def->sourcePath.c_str() );
				def.reset();
			}
			else
			{
				mstudiobone_t *bones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
				for( const auto &body : def->bodies )
				{
					bool found = false;
					for( int i = 0; i < hdr->numbones; ++i )
					{
						if( !Q_stricmp( bones[i].name, body.bone.c_str() )) { found = true; break; }
					}
					if( !found )
					{
						ALERT( at_warning, "%s: body '%s' references missing bone '%s' in %s\n",
							def->sourcePath.c_str(), body.name.c_str(), body.bone.c_str(), modelName );
						def.reset();
						break;
					}
				}
			}
		}

		// Named v2/v3 collision sections deliberately bind to render bodygroups, not
		// to a second Studio model. Reject typos/out-of-range variants once here.
		if( def && physicsasset::HasCollisionGeometry( *def ))
		{
			for( const auto &collision : def->collisions )
			{
				if( !collision.bone.empty() )
				{
					bool foundBone = false;
					mstudiobone_t *bones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
					for( int i = 0; i < hdr->numbones; ++i )
						if( !Q_stricmp( bones[i].name, collision.bone.c_str() )) { foundBone = true; break; }
					if( !foundBone )
					{
						ALERT( at_warning, "%s: collision '%s' references missing bone '%s' in %s\n",
							def->sourcePath.c_str(), collision.name.c_str(), collision.bone.c_str(), modelName );
						def.reset();
						break;
					}
				}
				if( collision.bodyGroup.empty() ) continue;
				mstudiobodyparts_t *part = FindBodyPartByName( hdr, collision.bodyGroup.c_str() );
				if( !part || collision.variant < 0 || collision.variant >= part->nummodels )
				{
					ALERT( at_warning, "%s: collision '%s' references invalid bodygroup %s variant %d in %s\n",
						def->sourcePath.c_str(), collision.name.c_str(), collision.bodyGroup.c_str(), collision.variant, modelName );
					def.reset();
					break;
				}
			}
		}
	}

	m_physicsAssetDefinitions.emplace( key, def );
	return def;
}

bool CPhysicPhysX::CreatePhysicsAsset( CBaseEntity *pEntity )
{
	if( !pEntity || !m_pPhysics || !m_pScene ) return false;

	PhysicsAssetInstance *instance = EnsurePhysicsAssetInstance( pEntity );
	if( !instance ) return false;
	if( instance->HasActors() ) return true;

	if( !instance->CreateActors( m_pPhysics, m_pScene, m_pDefaultMaterial ))
	{
		ALERT( at_warning, "Failed to create physics asset %s for %s\n",
			instance->Definition().sourcePath.c_str(), pEntity->GetClassname() );
		return false;
	}

	ALERT( at_aiconsole, "Instantiated physics asset %s for %s\n",
		instance->Definition().sourcePath.c_str(), pEntity->GetClassname() );
	return true;
}

bool CPhysicPhysX::HasPhysicsAsset( CBaseEntity *pEntity )
{
	if( PhysicsAssetInstance *existing = FindPhysicsAsset( pEntity ))
	{
		if( existing->MatchesOwnerModel() ) return true;
		return EnsurePhysicsAssetInstance( pEntity ) != nullptr;
	}
	std::shared_ptr<PhysicsAssetDef> def = GetPhysicsAssetDefinition( pEntity );
	return def && physicsasset::HasArticulatedBodies( *def );
}

void CPhysicPhysX::UpdatePhysicsAssetPose( CBaseEntity *pEntity )
{
	if( !pEntity || !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE ) return;

	// Animation-driven prop collision must advance even for collision-only .phys
	// assets that have no ragdoll BodyDef and therefore no PhysicsAssetInstance.
	auto collisionState = m_physicsAssetCollisionStates.find( pEntity->entindex() );
	if( collisionState != m_physicsAssetCollisionStates.end() )
	{
		std::shared_ptr<PhysicsAssetDef> collisionAsset = GetPhysicsAssetDefinition( pEntity );
		if( collisionAsset )
		{
			for( PhysicsPartRuntime &part : collisionState->second.parts )
                if( part.actor && (part.owner == physicsasset::PartOwner::AnimatedProxy ||
                    part.mode == PhysicsPartMode::Driven) &&
                    !collisionState->second.articulatedOverride && part.boneIndex >= 0 )
                    SampleInteractivePart( pEntity, part );
			SetAnimatedCollisionEnabled( collisionState->second, collisionState->second.collisionRequested );
			RebuildAnimatedCollisionTraceMesh( pEntity, collisionState->second, *collisionAsset );
			UpdateEntityAABB( pEntity );
		}
	}

	PhysicsAssetInstance *asset = EnsurePhysicsAssetInstance( pEntity );
	if( !asset ) return;
	if( asset->IsRagdoll() && !asset->NeedsAnimationControl() ) return;
	asset->UpdateAnimationPose( gpGlobals ? gpGlobals->time : 0.0f );
}

bool CPhysicPhysX::IsRagdoll( CBaseEntity *pEntity )
{
	PhysicsAssetInstance *asset = FindPhysicsAsset( pEntity );
	return asset && asset->MatchesOwnerModel() && asset->IsRagdoll();
}

bool CPhysicPhysX::SetRagdollBodyPhysicsWeight( CBaseEntity *pEntity, const char *bodyName, float physicsWeight, bool includeChildren )
{
	if( !pEntity || !std::isfinite( physicsWeight )) return false;

	PhysicsAssetInstance *asset = EnsurePhysicsAssetInstance( pEntity );
	if( !asset || !asset->MatchesOwnerModel() ) return false;
	if( !asset->HasActors() )
	{
		if( !CreatePhysicsAsset( pEntity )) return false;
		asset = FindPhysicsAsset( pEntity );
		if( !asset || !asset->HasActors() ) return false;
	}

	const bool newlyActivated = !asset->IsRagdoll();
	if( newlyActivated )
	{
		// A driven/partial ragdoll keeps the GoldSrc entity hull and movetype, but
		// its old PhysX shadow still has to be disabled or it would collide with
		// the new articulated bodies. SetRagdoll(true) itself does not alter the
		// entity frame; that is done only for a full free ragdoll below.
		PxActor *legacyActor = ActorFromEntity( pEntity );
		PxRigidActor *legacyRigid = legacyActor ? legacyActor->is<PxRigidActor>() : nullptr;
		if( legacyRigid ) SuspendLegacyCollisionForRagdoll( pEntity, legacyRigid );
		if( !asset->SetRagdoll( true ))
		{
			RestoreLegacyCollisionAfterRagdoll( pEntity );
			return false;
		}
	}

	if( !asset->SetBodyPhysicsWeight( bodyName, physicsWeight, includeChildren ))
	{
		if( newlyActivated )
		{
			asset->SetRagdoll( false );
			RestoreLegacyCollisionAfterRagdoll( pEntity );
		}
		return false;
	}

    // Partially driven ragdolls also take ownership from animated proxies;
    // this cannot be done before the SetBodyPhysicsWeight success path.
    SetArticulatedCollisionOwnership( pEntity, true );

	const int entityIndex = ENTINDEX( pEntity->edict() );
	if( asset->NeedsAnimationControl() )
	{
		// If this was a full ragdoll, hand normal movement back to the entity at
		// its current world location, then let the animation targets pull/anchor
		// selected physics bodies. Preserve current placement across the restore.
		if( m_ragdollEntityState.find( entityIndex ) != m_ragdollEntityState.end() )
		{
			const Vector currentOrigin = pEntity->GetAbsOrigin();
			const Vector currentAngles = pEntity->GetAbsAngles();
			RestoreEntityAfterRagdoll( pEntity );
			pEntity->SetAbsOrigin( currentOrigin );
			pEntity->SetAbsAngles( currentAngles );
			pEntity->RelinkEntity( TRUE );
		}
		SetBits( pEntity->pev->iuser1, CF_RAGDOLL );
		asset->UpdateAnimationPose( gpGlobals ? gpGlobals->time : 0.0f );
	}
	else
	{
		// All bodies returned to weight 1: physical ownership is now a regular
		// free ragdoll. Suspend the entity hull exactly as Milestone D does.
		asset->CaptureSimulationPose();
		if( m_ragdollEntityState.find( entityIndex ) == m_ragdollEntityState.end() )
			SuspendEntityForRagdoll( pEntity, asset );
	}

	asset->CaptureSimulationPose();
	UpdateRagdollPresentation( asset );
	return true;
}

bool CPhysicPhysX::GetRagdollWorldBounds( CBaseEntity *pEntity, Vector &mins, Vector &maxs )
{
    PhysicsAssetInstance *asset = pEntity ? FindPhysicsAsset(pEntity) : nullptr;
    if( !asset || !asset->MatchesOwnerModel() || !asset->IsRagdoll() ||
        asset->NeedsAnimationControl() ) return false;

    PxBounds3 bounds;
    if( !asset->GetSimulationBounds(bounds) || !bounds.isValid() ) return false;
    // Include the same slack as the local visual bounds, plus Xash's one-unit
    // broadphase expansion performed for ordinary entities.
    const float margin = 9.0f;
    mins = Vector(bounds.minimum.x - margin, bounds.minimum.y - margin, bounds.minimum.z - margin);
    maxs = Vector(bounds.maximum.x + margin, bounds.maximum.y + margin, bounds.maximum.z + margin);
    return true;
}

bool CPhysicPhysX::IsRagdollFrozen( CBaseEntity *pEntity )
{
	PhysicsAssetInstance *asset = FindPhysicsAsset( pEntity );
	return asset && asset->MatchesOwnerModel() && asset->IsRagdoll() && asset->IsFrozen();
}

bool CPhysicPhysX::BuildRagdollDecalAttachment( CBaseEntity *pEntity, const Vector &worldPosition, const Vector &worldNormal, int preferredHitGroup, int &boneIndex, Vector &localPosition, Vector &localNormal )
{
	PhysicsAssetInstance *asset = FindPhysicsAsset( pEntity );
	if( !asset || !asset->MatchesOwnerModel() || !asset->IsRagdoll() ) return false;
	PxVec3 localPos, localN;
	if( !asset->BuildDecalAttachment( PxVec3( worldPosition.x, worldPosition.y, worldPosition.z ),
		PxVec3( worldNormal.x, worldNormal.y, worldNormal.z ), preferredHitGroup, boneIndex, localPos, localN )) return false;
	localPosition = Vector( localPos.x, localPos.y, localPos.z );
	localNormal = Vector( localN.x, localN.y, localN.z );
	return true;
}

void CPhysicPhysX::TransferPendingRagdollImpulses( CBaseEntity *pEntity, PhysicsAssetInstance *asset )
{
	auto pendingIt = m_pendingRagdollImpulses.find( ENTINDEX( pEntity->edict() ));
	if( pendingIt != m_pendingRagdollImpulses.end() )
	{
		const float now = gpGlobals ? gpGlobals->time : 0.0f;
		const auto &bodies = asset->Bodies();
		const auto &defs = asset->Definition().bodies;
		for( const PendingRagdollImpulse &pending : pendingIt->second )
		{
			if( pending.edict != pEntity->edict() || now - pending.time > 0.25f || pending.time > now + 0.001f ) continue;
			int bestBody = -1;
			float bestDistSq = FLT_MAX;
			for( size_t i = 0; i < bodies.size() && i < defs.size(); ++i )
			{
				if( !bodies[i].actor ) continue;
				if( pending.hitGroup != 0 && defs[i].hitGroup != pending.hitGroup ) continue;
				const PxVec3 d = bodies[i].actor->getGlobalPose().p - PxVec3( pending.position.x, pending.position.y, pending.position.z );
				const float distSq = d.magnitudeSquared();
				if( distSq < bestDistSq ) { bestDistSq = distSq; bestBody = static_cast<int>( i ); }
			}
			if( bestBody < 0 )
			{
				// Older .phys files may have no hitgroup tags. Fall back to the
				// physically closest body instead of dumping every killing shot
				// into the pelvis/root.
				for( size_t i = 0; i < bodies.size(); ++i )
				{
					if( !bodies[i].actor ) continue;
					const PxVec3 d = bodies[i].actor->getGlobalPose().p - PxVec3( pending.position.x, pending.position.y, pending.position.z );
					const float distSq = d.magnitudeSquared();
					if( distSq < bestDistSq ) { bestDistSq = distSq; bestBody = static_cast<int>( i ); }
				}
			}
			if( bestBody < 0 ) bestBody = asset->RootBodyIndex();
			asset->AddImpulseToBody( bestBody, PxVec3( pending.impulse.x, pending.impulse.y, pending.impulse.z ), PxVec3( pending.position.x, pending.position.y, pending.position.z ));
		}
		m_pendingRagdollImpulses.erase( pendingIt );
	}
}

void CPhysicPhysX::SetRagdoll( CBaseEntity *pEntity, bool enabled )
{
	if( !pEntity ) return;

	PhysicsAssetInstance *asset = EnsurePhysicsAssetInstance( pEntity );
	if( !asset ) return;
	if( asset->IsRagdoll() == enabled )
	{
		if( enabled && asset->NeedsAnimationControl() )
		{
			// Explicit EnableRagdoll(true) promotes a partial/physical-animation
			// instance to a fully free ragdoll instead of being a no-op.
			if( !SetRagdollBodyPhysicsWeight( pEntity, nullptr, 1.0f, true )) return;
		}
		if( enabled )
		{
			// BecomeDead may have restored MOVETYPE_TOSS even for a free
			// ragdoll. Reassert hull suspension without replacing its saved
			// entity frame/root relation or touching actors and joints.
			SuspendEntityForRagdoll( pEntity, asset );
			TransferPendingRagdollImpulses( pEntity, asset );
		}
		return;
	}

	if( enabled && !asset->HasActors() )
	{
		if( !CreatePhysicsAsset( pEntity )) return;
		asset = FindPhysicsAsset( pEntity );
		if( !asset || !asset->HasActors() ) return;
	}

	PxActor *legacyActor = ActorFromEntity( pEntity );
	PxRigidActor *legacyRigid = legacyActor ? legacyActor->is<PxRigidActor>() : nullptr;

	if( enabled )
	{
		// PrimeXT characters may already own a kinematic PhysX shadow box. If it
		// remains active inside the articulated ragdoll it will fight the limbs.
		if( legacyRigid )
			SuspendLegacyCollisionForRagdoll( pEntity, legacyRigid );

		if( !asset->SetRagdoll( true ))
		{
			// Restore the exact legacy shape flags if activation failed atomically.
			RestoreLegacyCollisionAfterRagdoll( pEntity );
			return;
		}
		SetArticulatedCollisionOwnership( pEntity, true );

		// Transfer very recent bullet/melee impulses that were recorded while the
		// character was still animation-driven. This preserves killing-shot
		// momentum even though the authoritative hit initially came from Studio
		// hitboxes before the .phys actors became dynamic.
		TransferPendingRagdollImpulses( pEntity, asset );

		// Capture the exact just-activated body pose before deriving the
		// root-bone -> entity relationship. Without this ordering the presentation
		// root is unavailable on the first frame and feet-origin models can jump to
		// the pelvis when the legacy entity hull is suspended.
		asset->CaptureSimulationPose();
		SuspendEntityForRagdoll( pEntity, asset );
		UpdateRagdollPresentation( asset );
	}
	else
	{
		if( !asset->SetRagdoll( false )) return;
        SetArticulatedCollisionOwnership( pEntity, false );
		ClearRagdollPose( pEntity );
		RestoreLegacyCollisionAfterRagdoll( pEntity );
		RestoreEntityAfterRagdoll( pEntity );

		// SetRagdoll(false) starts a fresh animation-history epoch. Restore the
		// entity frame first, then replace that first sample with the pose in the
		// restored frame so a later reactivation cannot derive velocity from a
		// transient ragdoll-oriented entity transform.
		asset->UpdateAnimationPose( gpGlobals ? gpGlobals->time : 0.0f );
		asset->FollowAnimation();
	}
}

IPhysicLayer::CollisionRepresentation CPhysicPhysX::ResolveCollisionRepresentation( CBaseEntity *pEntity, IPhysicLayer::CollisionQueryPurpose purpose )
{
	if( !pEntity ) return CollisionRepresentation::Legacy;
	if( IsRagdoll( pEntity ))
		return CollisionRepresentation::PhysicsAsset;

	model_t *model = pEntity->pev->modelindex > 0 ? (model_t *)MODEL_HANDLE( pEntity->pev->modelindex ) : nullptr;
	const bool studio = model && model->type == mod_studio;
	const bool character = FBitSet( pEntity->pev->flags, FL_MONSTER | FL_CLIENT );
	const bool hasAssetCollision = studio && HasPhysicsAssetCollision( pEntity );

	switch( purpose )
	{
	case CollisionQueryPurpose::BulletDamage:
		if( character ) return CollisionRepresentation::Hitboxes;
		if( hasAssetCollision ) return CollisionRepresentation::PhysicsAsset;
		return CollisionRepresentation::Legacy;
	case CollisionQueryPurpose::PrecisionQuery:
		if( hasAssetCollision ) return CollisionRepresentation::PhysicsAsset;
		if( character ) return CollisionRepresentation::Hitboxes;
		return CollisionRepresentation::Legacy;
	case CollisionQueryPurpose::PhysicsSimulation:
	case CollisionQueryPurpose::Movement:
	default:
		if( hasAssetCollision ) return CollisionRepresentation::PhysicsAsset;
		return CollisionRepresentation::Legacy;
	}
}

// Supplementary query is required for primitive-only v4 doors and doors
// protruding beyond their chassis' old GoldSrc/root collision hull. The
// legacy/world nearest hit is ALWAYS the upper distance bound.
bool CPhysicPhysX::TraceBulletPhysicsParts( const Vector &start, const Vector &end,
    edict_t *ignore, TraceResult *ioTrace )
{
    if( !ioTrace || !m_pScene ) return false;
    const Vector delta = end - start;
    const float rayLength = delta.Length();
    if( !std::isfinite(rayLength) || rayLength <= 0.0001f ||
        !std::isfinite(ioTrace->flFraction) ) return false;
    const PxVec3 origin(start.x, start.y, start.z);
    const PxVec3 direction(delta.x / rayLength, delta.y / rayLength, delta.z / rayLength);
    const PxHitFlags flags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL;
    float closest = rayLength * std::clamp(ioTrace->flFraction, 0.0f, 1.0f);
    PxRaycastHit chosenHit;
    CBaseEntity *chosenEntity = nullptr;
    int chosenHitGroup = 0;
    for( auto &entry : m_physicsAssetCollisionStates )
    {
        PhysicsAssetCollisionState &state = entry.second;
        if( !state.edict || state.edict->free || state.edict == ignore ||
            !state.collisionRequested || state.articulatedOverride ) continue;
        CBaseEntity *entity = CBaseEntity::Instance(state.edict);
        if( !entity || entity->edict() != state.edict || entity->entindex() != entry.first ) continue;
        auto asset = GetPhysicsAssetDefinition(entity);
        for( const PhysicsPartRuntime &part : state.parts )
        {
            if( !part.actor ) continue;
            const PxTransform partPose = part.owner == physicsasset::PartOwner::AnimatedProxy && part.sampledPose
                ? part.latestAnimatedPose : part.actor->getGlobalPose();
            for( const PhysicsPartShape &section : part.shapes )
            {
                if( !section.shape || !(section.shape->getFlags() & PxShapeFlag::eSCENE_QUERY_SHAPE) ) continue;
                const PxTransform pose = partPose * section.shape->getLocalPose();
                if( !pose.isValid() ) continue;
                PxRaycastHit hit;
                const PxGeometryHolder geom = section.shape->getGeometry();
                if( PxGeometryQuery::raycast(origin, direction, geom.any(), pose,
                        closest, flags, 1, &hit) == 0 ) continue;
                // Tie-break favors existing trace (including walls or a root
                // collider); two independent physics representations never
                // cause a duplicate hit at effectively equal distances.
                if( !std::isfinite(hit.distance) || hit.distance < 0.0f ||
                    !physicsasset::PhysicsPartRayHitWins(hit.distance, closest) || !hit.position.isFinite() ||
                    !hit.normal.isFinite() ) continue;
                closest = hit.distance;
                chosenHit = hit;
                chosenEntity = entity;
                chosenHitGroup = (asset && part.bodyIndex >= 0 &&
                    static_cast<size_t>(part.bodyIndex) < asset->bodies.size()) ?
                    asset->bodies[part.bodyIndex].hitGroup : 0;
            }
        }
    }
    if( !chosenEntity ) return false;
    const int inOpen = ioTrace->fInOpen;
    const int inWater = ioTrace->fInWater;
    memset(ioTrace, 0, sizeof(*ioTrace));
    ioTrace->fInOpen = inOpen;
    ioTrace->fInWater = inWater;
    ioTrace->flFraction = closest / rayLength;
    ioTrace->vecEndPos = Vector(chosenHit.position.x, chosenHit.position.y, chosenHit.position.z);
    ioTrace->vecPlaneNormal = Vector(chosenHit.normal.x, chosenHit.normal.y, chosenHit.normal.z);
    ioTrace->flPlaneDist = DotProduct(ioTrace->vecEndPos, ioTrace->vecPlaneNormal);
    ioTrace->pHit = chosenEntity->edict();
    ioTrace->iHitgroup = chosenHitGroup;
    ioTrace->fStartSolid = closest <= 0.001f;
    return true;
}

bool CPhysicPhysX::TraceBulletRagdolls( const Vector &start, const Vector &end, edict_t *ignore, TraceResult *ioTrace, int *bodyIndex )
{
	if( bodyIndex ) *bodyIndex = -1;
	if( !ioTrace || !m_pScene ) return false;

	Vector delta = end - start;
	const float length = delta.Length();
	if( length <= 0.0001f || !std::isfinite( length )) return false;
	const PxVec3 origin( start.x, start.y, start.z );
	const PxVec3 direction( delta.x / length, delta.y / length, delta.z / length );
	float closestDistance = length * std::clamp( ioTrace->flFraction, 0.0f, 1.0f );
	bool found = false;
	PhysicsAssetRayHit bestHit;
	CBaseEntity *bestEntity = nullptr;

	Vector traceMins, traceMaxs;
	UTIL_MoveBounds( start, g_vecZero, g_vecZero, end, traceMins, traceMaxs );

	for( const auto &assetPtr : m_physicsAssets )
	{
		PhysicsAssetInstance *asset = assetPtr.get();
		if( !asset || !asset->IsRagdoll() || !asset->HasActors() ) continue;
		CBaseEntity *owner = asset->Owner();
		if( !owner || owner->edict() == ignore || owner->edict()->free ) continue;
		if( ResolveCollisionRepresentation( owner, CollisionQueryPurpose::BulletDamage ) != CollisionRepresentation::PhysicsAsset ) continue;
        // B5/B6: cull with current PhysX body bounds, not with the animated
        // entity hull (partial ragdolls retain that hull) or rotated local
        // mins/maxs. If the snapshot is unavailable, do not cull at all.
        PxBounds3 bodyBounds;
        if( asset->GetSimulationBounds(bodyBounds) && bodyBounds.isValid() )
        {
            const Vector physMins(bodyBounds.minimum.x, bodyBounds.minimum.y, bodyBounds.minimum.z);
            const Vector physMaxs(bodyBounds.maximum.x, bodyBounds.maximum.y, bodyBounds.maximum.z);
            const float traceLow[3] = {traceMins.x, traceMins.y, traceMins.z};
            const float traceHigh[3] = {traceMaxs.x, traceMaxs.y, traceMaxs.z};
            const float physicsLow[3] = {physMins.x, physMins.y, physMins.z};
            const float physicsHigh[3] = {physMaxs.x, physMaxs.y, physMaxs.z};
            if( !ragdollquery::OverlapWorldBounds(traceLow, traceHigh, physicsLow, physicsHigh) ) continue;
        }

		PhysicsAssetRayHit hit;
		if( !asset->Raycast( origin, direction, closestDistance, hit )) continue;
		// Keep the legacy/world hit when distances are effectively tied. This
		// prevents a limb resting against a wall from stealing the wall hit.
		if( hit.distance + 0.001f >= closestDistance ) continue;
		closestDistance = hit.distance;
		bestHit = hit;
		bestEntity = owner;
		found = true;
	}

	if( !found || !bestEntity ) return false;

	const int inOpen = ioTrace->fInOpen;
	const int inWater = ioTrace->fInWater;
	memset( ioTrace, 0, sizeof( *ioTrace ));
	ioTrace->fInOpen = inOpen;
	ioTrace->fInWater = inWater;
	ioTrace->flFraction = closestDistance / length;
	ioTrace->vecEndPos = Vector( bestHit.position.x, bestHit.position.y, bestHit.position.z );
	ioTrace->vecPlaneNormal = Vector( bestHit.normal.x, bestHit.normal.y, bestHit.normal.z );
	ioTrace->flPlaneDist = DotProduct( ioTrace->vecEndPos, ioTrace->vecPlaneNormal );
	ioTrace->pHit = bestEntity->edict();
	ioTrace->iHitgroup = bestHit.hitGroup;
	ioTrace->fStartSolid = closestDistance <= 0.001f;
	if( bodyIndex ) *bodyIndex = bestHit.bodyIndex;
	return true;
}

// J.9: target-only penetration exit, using the very same PhysX shape as
// the entry. This never raycasts the entire scene or another character part.
bool CPhysicPhysX::TraceBallisticPhysicsExit(CBaseEntity *target, int bodyIndex,
    const Vector &entry, const Vector &forward, float depth, Vector &exit, Vector &normal)
{
    if(!target || !m_pScene || !std::isfinite(depth) || depth<=1.0f || depth>96.0f)
        return false;
    const PxVec3 point(entry.x,entry.y,entry.z);
    const PxVec3 direction(forward.x,forward.y,forward.z);
    if(!point.isFinite() || !direction.isFinite() ||
       std::abs(direction.magnitudeSquared()-1.0f)>0.01f) return false;
    if(IsRagdoll(target)) {
        if(bodyIndex<0) return false;
        for(const auto &asset: m_physicsAssets) {
            if(!asset || asset->Owner()!=target || !asset->IsRagdoll()) continue;
            PxVec3 out, n;
            if(!asset->RaycastBodyExit(bodyIndex,point,direction,depth,out,n)) return false;
            exit=Vector(out.x,out.y,out.z);
            normal=Vector(n.x,n.y,n.z);
            return true;
        }
        return false;
    }
    // Primitive-only .phys assets and child actors may be active even when
    // the root has no mesh sections or a disabled bodygroup selection.
    auto it=m_physicsAssetCollisionStates.find(target->entindex());
    const bool haveState=it!=m_physicsAssetCollisionStates.end() &&
        it->second.edict==target->edict();
    if(haveState && (!it->second.collisionRequested || it->second.articulatedOverride))
        return false;
    const PxHitFlags flags=PxHitFlag::ePOSITION | PxHitFlag::eNORMAL;
    struct EntryShape { PxShape *shape; PxTransform pose; };
    std::vector<EntryShape> candidates;
    // Select the entry shape independently of its reverse exit. Otherwise
    // a failed reverse ray can cause another nearby shape to be substituted.
    std::vector<modular::BallisticEntryCandidate> entries;
    auto inspect=[&](PxShape *shape,const PxTransform &pose) {
        if(!shape || !pose.isValid() ||
           !(shape->getFlags() & PxShapeFlag::eSCENE_QUERY_SHAPE)) return;
        if(std::any_of(candidates.begin(),candidates.end(),[shape](const EntryShape &candidate) {
                return candidate.shape==shape;
            })) return;
        const PxGeometryHolder geometry=shape->getGeometry();
        PxRaycastHit first{};
        if(!PxGeometryQuery::raycast(point-direction*0.5f,direction,
                geometry.any(),pose,1.0f,flags,1,&first) ||
           !first.position.isFinite() || !first.normal.isFinite() ||
           first.normal.dot(direction)>-0.15f) return;
        const float offset=(first.position-point).magnitude();
        if(offset>0.75f) return;
        const int index=int(candidates.size());
        candidates.push_back({shape,pose});
        entries.push_back({index,offset});
    };
    if(haveState) {
        for(const PhysicsPartRuntime &part:it->second.parts) {
            if(!part.actor) continue;
            const PxTransform partPose=part.owner==physicsasset::PartOwner::AnimatedProxy && part.sampledPose ?
                part.latestAnimatedPose : part.actor->getGlobalPose();
            for(const PhysicsPartShape &section:part.shapes)
                if(section.shape) inspect(section.shape,partPose*section.shape->getLocalPose());
        }
    }
    // Root sections are an optional, independent source of collision.
    if(PxActor *actor=ActorFromEntity(target)) {
        if(PxRigidActor *root=actor->is<PxRigidActor>()) {
            const PxU32 count=root->getNbShapes();
            if(count>128) return false; // Do not certify an incomplete root query.
            PxShape *shapes[128]{};
            const PxU32 total=root->getShapes(shapes,count);
            if(total!=count) return false;
            for(PxU32 i=0;i<total;++i)
                if(shapes[i]) inspect(shapes[i],root->getGlobalPose()*shapes[i]->getLocalPose());
        }
    }
    const auto selection=modular::SelectBallisticEntry(entries.data(),entries.size());
    if(!modular::ValidBallisticEntrySelection(selection)) return false;
    const EntryShape &picked=candidates[size_t(selection.index)];
    PxRaycastHit back{};
    const PxGeometryHolder geometry=picked.shape->getGeometry();
    if(!PxGeometryQuery::raycast(point+direction*depth,-direction,
            geometry.any(),picked.pose,depth-0.5f,flags,1,&back) ||
        !back.position.isFinite() || !back.normal.isFinite() ||
        back.normal.dot(direction)<0.15f) return false;
    const float along=(back.position-point).dot(direction);
    if(!std::isfinite(along) || along<=0.5f || along>=depth-0.5f ||
       (back.position-point-direction*along).magnitude()>0.75f) return false;
    exit=Vector(back.position.x,back.position.y,back.position.z);
    normal=Vector(back.normal.x,back.normal.y,back.normal.z);
    return true;
}

// Milestone I.5: select ONLY a live collision shape owned by the entity
// which the ordinary server bullet trace has already hit. This prevents
// independent prop physics queries from shooting through the world/other props.
// The chosen shape's actor is the SAME I.2/I.3 actor used by animation and hinge
// simulation; there is no second bullet-only door body.
bool CPhysicPhysX::ApplyPhysicsPartBulletImpact( CBaseEntity *entity, const Vector &start,
    const Vector &end, const Vector &legacyContact, const Vector &impulse, float damage )
{
    if( !entity || !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE ||
        !std::isfinite(damage) || damage < 0.0f ) return false;
    if( !std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(start.z) ||
        !std::isfinite(end.x) || !std::isfinite(end.y) || !std::isfinite(end.z) ||
        !std::isfinite(legacyContact.x) || !std::isfinite(legacyContact.y) || !std::isfinite(legacyContact.z) ||
        !std::isfinite(impulse.x) || !std::isfinite(impulse.y) || !std::isfinite(impulse.z) ) return false;
    auto stateIt = m_physicsAssetCollisionStates.find(entity->entindex());
    if( stateIt == m_physicsAssetCollisionStates.end() ||
        stateIt->second.edict != entity->edict() || stateIt->second.articulatedOverride ||
        !stateIt->second.collisionRequested ) return false;

    const Vector delta = end - start;
    const float rayLength = delta.Length();
    if( !std::isfinite(rayLength) || rayLength < 0.001f ) return false;
    // Contact tolerance covers mesh/convex skin discrepancy, not a second
    // unrestricted ray. The original hit must already be on THIS entity.
    const float maxDistance = std::min(rayLength, (legacyContact - start).Length() + 0.75f);
    const PxVec3 origin(start.x, start.y, start.z);
    const PxVec3 direction(delta.x / rayLength, delta.y / rayLength, delta.z / rayLength);
    const PxHitFlags flags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL;
    PhysicsPartRuntime *bestPart = nullptr;
    PxRaycastHit bestHit;
    float closest = maxDistance;
    for( PhysicsPartRuntime &part : stateIt->second.parts )
    {
        if( !part.actor ) continue;
        const PxTransform bodyPose = part.owner == physicsasset::PartOwner::AnimatedProxy && part.sampledPose
            ? part.latestAnimatedPose : part.actor->getGlobalPose();
        for( const PhysicsPartShape &section : part.shapes )
        {
            if( !section.shape || !(section.shape->getFlags() & PxShapeFlag::eSCENE_QUERY_SHAPE) ) continue;
            const PxGeometryHolder geometry = section.shape->getGeometry();
            const PxTransform shapePose = bodyPose * section.shape->getLocalPose();
            if( !shapePose.isValid() ) continue;
            PxRaycastHit hit;
            if( PxGeometryQuery::raycast(origin, direction, geometry.any(), shapePose,
                    closest, flags, 1, &hit) == 0 ) continue;
            if( !std::isfinite(hit.distance) || hit.distance < 0.0f ||
                hit.distance > closest || !hit.position.isFinite() ) continue;
            if( bestPart && hit.distance + 0.0001f >= closest ) continue;
            closest = hit.distance;
            bestPart = &part;
            bestHit = hit;
        }
    }
    if( !bestPart ) return false;

    // Damage is per named part and is counted once for this bullet. The
    // authored thresholds can release a motor or detach the hinge. Without
    // thresholds, Animated parts keep their legacy non-reactive behavior.
    if( !ApplyPhysicsPartDamageEvent(entity, *bestPart, damage, impulse.Length()) )
        return true;
    if( bestPart->detached || bestPart->owner == physicsasset::PartOwner::ArticulatedBody ||
        (bestPart->hasHinge && bestPart->dynamicCompatible &&
         bestPart->initialMode != physicsasset::PropDriveMode::Animated &&
         SetPhysicsPartMode(entity, bestPart->name.c_str(),
             bestPart->initialMode == physicsasset::PropDriveMode::Driven ?
                 PhysicsPartMode::Driven : PhysicsPartMode::Free)) )
    {
        if( bestPart->owner == physicsasset::PartOwner::ArticulatedBody && impulse.Length() > 0.0f )
        {
            PxRigidBodyExt::addForceAtPos(*bestPart->actor,
                PxVec3(impulse.x, impulse.y, impulse.z), bestHit.position, PxForceMode::eIMPULSE);
            bestPart->actor->wakeUp();
        }
    }
    return true; // one bullet -> at most one matching physics part / impulse
}

void CPhysicPhysX::AddPhysicsPartRadialImpulse( CBaseEntity *entity,
    const Vector &origin, float radius, float impulse, float damage )
{
    if( !entity || !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE ||
        !std::isfinite(radius) || radius <= 0.001f || !std::isfinite(impulse) || impulse < 0.0f ||
        !std::isfinite(damage) || damage < 0.0f || (damage == 0.0f && impulse == 0.0f) ||
        !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z) ) return;
    auto it = m_physicsAssetCollisionStates.find(entity->entindex());
    if( it == m_physicsAssetCollisionStates.end() || it->second.edict != entity->edict() ||
        it->second.articulatedOverride || !it->second.collisionRequested ) return;
    struct AffectedPart
    {
        PhysicsPartRuntime *part;
        PxVec3 center;
        PxVec3 direction;
        float weight;
    };
    std::vector<AffectedPart> affected;
    const PxVec3 blast(origin.x, origin.y, origin.z);
    float totalWeight = 0.0f;
    for( PhysicsPartRuntime &part : it->second.parts )
    {
        if( !part.actor || ((!part.hasHinge || !part.dynamicCompatible) && !part.detached) ) continue;
        if( part.owner != physicsasset::PartOwner::ArticulatedBody &&
            part.initialMode == physicsasset::PropDriveMode::Animated &&
            part.releaseDamage == 0.0f && part.breakDamage == 0.0f && part.breakImpulse == 0.0f ) continue;
        bool active = false;
        for( const PhysicsPartShape &section : part.shapes )
            if( section.shape && (section.shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE) )
                { active = true; break; }
        if( !active ) continue;
        const PxTransform pose = part.owner == physicsasset::PartOwner::AnimatedProxy && part.sampledPose
            ? part.latestAnimatedPose : part.actor->getGlobalPose();
        const PxVec3 direction = pose.p - blast;
        const float distance = direction.magnitude();
        if( !std::isfinite(distance) || distance >= radius ) continue;
        const float weight = physicsasset::PhysicsPartBlastWeight(distance, radius);
        if( weight <= 0.0f ) continue;
        affected.push_back({ &part, pose.p,
            distance > 0.001f ? direction / distance : PxVec3(0.0f, 0.0f, 1.0f), weight });
        totalWeight += weight;
    }
    if( totalWeight <= 0.0f ) return;
    // One blast event carries one impulse budget. Multiple parts share it,
    // rather than multiplying the explosion's energy by the door count.
    for( const AffectedPart &hit : affected )
    {
        const float deliveredImpulse = impulse * hit.weight / totalWeight;
        // Damage is the explosion's local radial falloff, NOT scaled by how
        // many doors are on the entity. The impulse budget remains shared.
        if( !ApplyPhysicsPartDamageEvent(entity, *hit.part, damage * hit.weight, deliveredImpulse) )
            continue;
        if( hit.part->owner != physicsasset::PartOwner::ArticulatedBody )
        {
            if( hit.part->initialMode == physicsasset::PropDriveMode::Animated ||
                !SetPhysicsPartMode(entity, hit.part->name.c_str(),
                    hit.part->initialMode == physicsasset::PropDriveMode::Driven ?
                        PhysicsPartMode::Driven : PhysicsPartMode::Free) ) continue;
        }
        if( deliveredImpulse <= 0.0f ) continue;
        const PxVec3 force = hit.direction * deliveredImpulse;
        PxRigidBodyExt::addForceAtPos(*hit.part->actor, force, hit.center, PxForceMode::eIMPULSE);
        hit.part->actor->wakeUp();
    }
}

bool CPhysicPhysX::AddPhysicsAssetImpulse( CBaseEntity *pEntity, int bodyIndex, const Vector &impulse, const Vector &position )
{
	PhysicsAssetInstance *asset = FindPhysicsAsset( pEntity );
	if( !asset || !asset->IsRagdoll() ) return false;
	return asset->AddImpulseToBody( bodyIndex, PxVec3( impulse.x, impulse.y, impulse.z ), PxVec3( position.x, position.y, position.z ));
}

void CPhysicPhysX::QueuePhysicsAssetImpulse( CBaseEntity *pEntity, int hitGroup, const Vector &impulse, const Vector &position )
{
	if( !pEntity || !HasPhysicsAsset( pEntity )) return;
	PhysicsAssetInstance *asset = FindPhysicsAsset( pEntity );
	if( asset && asset->IsRagdoll() && !asset->NeedsAnimationControl() ) return;
	if( !std::isfinite( impulse.x ) || !std::isfinite( impulse.y ) || !std::isfinite( impulse.z ) ||
		!std::isfinite( position.x ) || !std::isfinite( position.y ) || !std::isfinite( position.z )) return;

	const int entityIndex = ENTINDEX( pEntity->edict() );
	const float now = gpGlobals ? gpGlobals->time : 0.0f;
	auto &queue = m_pendingRagdollImpulses[entityIndex];
	queue.erase( std::remove_if( queue.begin(), queue.end(), [now, pEntity]( const PendingRagdollImpulse &pending )
	{
		return pending.edict != pEntity->edict() || now - pending.time > 0.25f || pending.time > now + 0.001f;
	}), queue.end() );
	if( queue.size() >= 16 ) queue.erase( queue.begin() );
	queue.push_back( { pEntity->edict(), impulse, position, std::clamp( hitGroup, 0, 255 ), now } );
}

void CPhysicPhysX::AddPhysicsAssetRadialImpulse( CBaseEntity *pEntity, const Vector &origin, float radius, float impulse )
{
	PhysicsAssetInstance *asset = FindPhysicsAsset( pEntity );
	if( !asset || !asset->IsRagdoll() ) return;
	asset->AddRadialImpulse( PxVec3( origin.x, origin.y, origin.z ), radius, impulse );
}


#endif // USE_PHYSICS_ENGINE
