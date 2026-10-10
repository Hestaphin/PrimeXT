/*
collision_filter_data.cpp - part of PhysX physics engine implementation
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

#include "collision_filter_data.h"

using namespace physx;

enum CollisionFlags : PxU32
{
    ConveyorActor = (1u << 0),
    PhysicsAssetBody = (1u << 1),
};

CollisionFilterData::CollisionFilterData() :
    m_conveyorFlag(false),
    m_physicsAssetBody(false),
    m_physicsAssetOwner(0)
{
}

CollisionFilterData::CollisionFilterData(const physx::PxFilterData &data)
{
    m_conveyorFlag = (data.word0 & CollisionFlags::ConveyorActor) != 0;
    m_physicsAssetBody = (data.word0 & CollisionFlags::PhysicsAssetBody) != 0;
    m_physicsAssetOwner = data.word1;
}

bool CollisionFilterData::HasConveyorFlag() const
{
    return m_conveyorFlag;
}

void CollisionFilterData::SetConveyorFlag(bool state)
{
    m_conveyorFlag = state;
}

bool CollisionFilterData::IsPhysicsAssetBody() const
{
    return m_physicsAssetBody;
}

void CollisionFilterData::SetPhysicsAssetBody(bool state)
{
    m_physicsAssetBody = state;
}

PxU32 CollisionFilterData::GetPhysicsAssetOwner() const
{
    return m_physicsAssetOwner;
}

void CollisionFilterData::SetPhysicsAssetOwner(PxU32 owner)
{
    m_physicsAssetOwner = owner;
}

PxFilterData CollisionFilterData::ToNativeType() const
{
    PxFilterData filterData(0, 0, 0, 0);
    filterData.word0 |= m_conveyorFlag ? CollisionFlags::ConveyorActor : 0;
    filterData.word0 |= m_physicsAssetBody ? CollisionFlags::PhysicsAssetBody : 0;
    filterData.word1 = m_physicsAssetOwner;
    return filterData;
}
