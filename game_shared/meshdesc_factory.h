/*
meshdesc_factory.h - class which responsible for creating and caching collision meshes
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

#pragma once
#include "meshdesc.h"
#include "clipfile.h"
#include <stdint.h>
#include <list>
#include <unordered_map>

class CMeshDescFactory
{
public:
	static CMeshDescFactory& Instance();
	void ClearCache();
	CMeshDesc& CreateObject(int32_t modelIndex, int32_t body, int32_t skin, clipfile::GeometryType geomType, uint64_t fingerprint = 0);

private:
	CMeshDescFactory();
	CMeshDescFactory(const CMeshDescFactory&) = delete;
	CMeshDescFactory& operator=(CMeshDescFactory&) = delete;

    struct Key
    {
        std::string name;
        int32_t body, skin;
        clipfile::GeometryType geometry;
        uint64_t fingerprint;
        bool operator==(const Key &other) const
        {
            return name == other.name && body == other.body && skin == other.skin &&
                geometry == other.geometry && fingerprint == other.fingerprint;
        }
    };
    struct KeyHash
    {
        size_t operator()(const Key &key) const
        {
            size_t hash = std::hash<std::string>{}(key.name);
            const auto mix = [&](size_t value) { hash ^= value + size_t(0x9e3779b9u) + (hash << 6) + (hash >> 2); };
            mix(std::hash<int32_t>{}(key.body));
            mix(std::hash<int32_t>{}(key.skin));
            mix(std::hash<int>{}(static_cast<int>(key.geometry)));
            mix(std::hash<uint64_t>{}(key.fingerprint));
            return hash;
        }
    };

	std::list<CMeshDesc> m_meshDescList;
    // The owning list keeps every embedded area-node and facet pointer stable.
    std::unordered_map<Key, CMeshDesc *, KeyHash> m_meshDescIndex;
};
