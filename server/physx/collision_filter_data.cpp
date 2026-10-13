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
#include "physics_asset_contact_policy.h"

using namespace physx;

enum CollisionFlags : PxU32
{
    ConveyorActor = (1u << 0),
    PhysicsAssetBody = (1u << 1),
};

CollisionFilterData::CollisionFilterData():m_native(0,0,0,0) {}
CollisionFilterData::CollisionFilterData(const PxFilterData &data):m_native(data) {}
bool CollisionFilterData::HasConveyorFlag() const {return (m_native.word0&ConveyorActor)!=0;}
void CollisionFilterData::SetConveyorFlag(bool on) {if(on) m_native.word0|=ConveyorActor;else m_native.word0&=~PxU32(ConveyorActor);}
bool CollisionFilterData::IsPhysicsAssetBody() const {return (m_native.word0&PhysicsAssetBody)!=0;}
void CollisionFilterData::SetPhysicsAssetBody(bool on)
{if(on) m_native.word0|=PhysicsAssetBody;else {m_native.word0&=~PxU32(PhysicsAssetBody|physicsasset::kAssetPolicyFlag);m_native.word2=m_native.word3=0;}}
PxU32 CollisionFilterData::GetPhysicsAssetOwner() const {return m_native.word1;}
void CollisionFilterData::SetPhysicsAssetOwner(PxU32 owner)
{if(owner!=m_native.word1) {m_native.word0&=~physicsasset::kAssetPolicyFlag;m_native.word2=m_native.word3=0;}m_native.word1=owner;}
PxFilterData CollisionFilterData::ToNativeType() const {return m_native;}
