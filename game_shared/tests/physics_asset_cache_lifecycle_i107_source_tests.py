#!/usr/bin/env python3
"""I.10.7 map teardown source-order checks (does not execute PhysX)."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
cpp = (root / 'server/physx/physx_impl.cpp').read_text()
hpp = (root / 'server/physx/physx_impl.h').read_text()
server = (root / 'server/client.cpp').read_text()
cache = (root / 'game_shared/physics_asset_cache_lifecycle.h').read_text()

assert '#include "physics_asset_cache_lifecycle.h"' in cpp
assert 'void ClearWorldAssetCaches();' in hpp
assert 'm_physicsAssetDefinitions.emplace( key, def );' in cpp  # positive/negative cache
assert 'if( cached != m_physicsAssetDefinitions.end() )' in cpp
assert 'void ServerDeactivate( void )' in server
assert 'WorldPhysic->FreeWorld();' in server[server.index('void ServerDeactivate( void )'): server.index('void ServerActivate(')]

world = cpp[cpp.index('void CPhysicPhysX :: FreeWorld()'):cpp.index('void CPhysicPhysX :: TeleportCharacter')]
assert world.index('m_physicsAssets.clear();') < world.index('ReleaseAnimatedCollisionActors(')
assert world.index('ReleaseAnimatedCollisionActors(') < world.index('actor->release();')
assert world.index('actor->release();') < world.rindex('ClearWorldAssetCaches();')
assert 'ClearWorldAssetCaches();\n\t\treturn;' in world  # null-scene path
assert world.count('ClearWorldAssetCaches();') == 2

physic = cpp[cpp.index('void CPhysicPhysX :: FreePhysic( void )'):cpp.index('void CPhysicPhysX :: Update(')]
assert physic.index('m_physicsAssets.clear();') < physic.index('m_pScene->release()')
assert physic.index('m_pScene->release()') < physic.index('ClearWorldAssetCaches();')
assert physic.index('ClearWorldAssetCaches();') < physic.index('m_pPhysics->release()')
assert 'm_triangleMeshCache.clear()' not in physic  # shared cleanup, no double-free

helper = cpp[cpp.index('void CPhysicPhysX::ClearWorldAssetCaches()'):cpp.index('void CPhysicPhysX :: FreeWorld()')]
assert 'physicsasset::ReleaseWorldAssetCaches( m_triangleMeshCache,' in helper
assert 'm_physicsAssetConvexMeshCache, m_physicsAssetDefinitions' in helper
assert 'entry.second->release();' in cache
# All .phys-cooked mesh identities include asset content, not only file/section name.
assert cpp.count('asset.collisionFingerprint,') >= 4
assert 'definitions.clear();' in cache
print('I.10.7 map-scoped caches, actor release order, and both shutdown paths PASS')
