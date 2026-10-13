/*
meshdesc_factory.cpp - class which responsible for creating and caching collision meshes
Copyright (C) 2023 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "meshdesc_factory.h"
#include "const.h"
#include "com_model.h"

#ifdef CLIENT_DLL
#include "utils.h"
#include "render_api.h"
#else
#include "edict.h"
#include "physcallback.h"
#include "enginecallback.h"
#endif

CMeshDescFactory::CMeshDescFactory()
{
    // Meshes contain pointers into their own embedded area nodes. Keep stable
    // list ownership and index it separately instead of relocating the objects.
    m_meshDescList = {};
}

CMeshDescFactory& CMeshDescFactory::Instance()
{
    static CMeshDescFactory instance;
    return instance;
}

void CMeshDescFactory::ClearCache()
{
    m_meshDescIndex.clear();
    m_meshDescList.clear();
}

CMeshDesc &CMeshDescFactory::CreateObject(int32_t modelIndex, int32_t body, int32_t skin, clipfile::GeometryType geomType, uint64_t fingerprint)
{
    model_t *mod = static_cast<model_t*>(MODEL_HANDLE(modelIndex));
	if (!mod) {
        HOST_ERROR("CMeshDescFactory: invalid model index in CreateObject");
	}

    auto initializeMeshDesc = [&](CMeshDesc &desc) {
        desc.SetDebugName(mod->name);
	    desc.SetModel(mod, body, skin);
	    desc.SetGeometryType(geomType);
        desc.SetSourceFingerprint(fingerprint);
    };

    const Key key{mod->name, body, skin, geomType, fingerprint};
    const auto found = m_meshDescIndex.find(key);
    if(found != m_meshDescIndex.end())
    {
        initializeMeshDesc(*found->second);
        return *found->second;
    }

    auto &meshDesc = m_meshDescList.emplace_back();
    initializeMeshDesc(meshDesc);
    m_meshDescIndex.emplace(key, &meshDesc);
    return meshDesc;
}
