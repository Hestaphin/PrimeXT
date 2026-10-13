/*
physics_asset_cache_lifecycle.h - PrimeXT world-owned PhysX cache teardown
Copyright (C) 2026 PrimeXT contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 3 or later.
*/
#pragma once

namespace physicsasset
{
// Entries are references owned by the physics cache. A PxShape keeps its own
// mesh reference while alive. Call only after releasing map actors/shapes.
template<typename MeshCache>
void ReleaseOwnedMeshCache( MeshCache &cache )
{
	for( auto &entry : cache )
	{
		if( entry.second ) entry.second->release();
	}
	cache.clear();
}

// Definitions include negative lookups (nullptr). Clearing them per map is
// necessary to notice new or edited .phys files without restarting the DLL.
template<typename TriangleCache, typename ConvexCache, typename DefinitionCache>
void ReleaseWorldAssetCaches( TriangleCache &triangles, ConvexCache &convexes,
	DefinitionCache &definitions )
{
	ReleaseOwnedMeshCache( triangles );
	ReleaseOwnedMeshCache( convexes );
	definitions.clear();
}
} // namespace physicsasset
