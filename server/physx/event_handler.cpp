/*
event_handler.h - class for handling PhysX simulation events
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
#include "event_handler.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "collision_filter_data.h"
#include <algorithm>

using namespace physx;

void EventHandler::onWorldInit()
{
	m_impl = std::make_unique<Impl>();
}

void EventHandler::onWorldShutdown()
{
	m_impl.reset();
}

EventHandler::TouchEventsQueue& EventHandler::getTouchEventsQueue()
{
	return m_impl->touchEventsQueue;
}

std::vector<EventHandler::WaterContactPair>& EventHandler::getWaterContactPairs()
{
	return m_impl->waterContactPairs;
}

void EventHandler::purgeActorReferences(PxActor *actor)
{
	if (!m_impl || !actor)
		return;

	// Event queues intentionally store raw PhysX actor pointers. Remove every
	// reference before the actor is released so HandleEvents() can never
	// dereference a stale actor on the following server frame.
	auto waterIt = std::remove_if(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(),
		[actor](const WaterContactPair &p) {
			return p.waterTriggerActor == actor || p.objectActor == actor;
		});
	m_impl->waterContactPairs.erase(waterIt, m_impl->waterContactPairs.end());

	TouchEventsQueue kept;
	while (!m_impl->touchEventsQueue.empty())
	{
		auto event = m_impl->touchEventsQueue.front();
		m_impl->touchEventsQueue.pop();
		if (event.first != actor && event.second != actor)
			kept.push(event);
	}
	m_impl->touchEventsQueue.swap(kept);
}


// Documentation:
// SDK state should not be modified from within the callbacks. In particular objects should not
// be created or destroyed. If state modification is needed then the changes should be stored to a buffer
// and performed after the simulation step.
void EventHandler::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
	for (PxU32 i = 0; i < count; i++)
	{
		const PxTriggerPair &pair = pairs[i];
		if (pair.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
		{
			// A removed-shape event is exactly when cached water pairs must be
			// purged. Ignoring it leaves dangling PxActor pointers that are later
			// dereferenced by buoyancy processing. Compare identities only; do not
			// dereference actors whose shapes have been removed.
			auto it = std::remove_if(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), [&pair](const auto &p) {
				return p.waterTriggerActor == pair.triggerActor || p.objectActor == pair.otherActor;
			});
			m_impl->waterContactPairs.erase(it, m_impl->waterContactPairs.end());
			continue;
		}

		edict_t *triggerEntity = reinterpret_cast<edict_t*>(pair.triggerActor->userData);
		if (triggerEntity && FStrEq(STRING(triggerEntity->v.classname), "func_water")) 
		{
			WaterContactPair contactPair = { pair.triggerActor, pair.otherActor };
			if (pair.status == PxPairFlag::eNOTIFY_TOUCH_FOUND) 
			{
				auto searchIter = std::find(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), contactPair);
				if (searchIter == std::end(m_impl->waterContactPairs)) {
					m_impl->waterContactPairs.push_back(contactPair);
				}
			}
			else if (pair.status == PxPairFlag::eNOTIFY_TOUCH_LOST) 
			{
 				auto it = std::remove_if(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), [&contactPair](const auto &p) {
					return p == contactPair;
				});
				m_impl->waterContactPairs.erase(it, m_impl->waterContactPairs.end());
			}
		}
	}
}

void EventHandler::onContact(const PxContactPairHeader &pairHeader, const PxContactPair *pairs, PxU32 nbPairs)
{
	// When an actor was removed, pairHeader.actors[] may no longer be safe to
	// dereference. Shape-level removed flags are not sufficient because the
	// header has its own actor-lifetime flags.
	if (pairHeader.flags & (PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
		return;

	for (PxU32 i = 0; i < nbPairs; i++)
	{
		const PxContactPair &pair = pairs[i];
		if (pair.flags & (PxContactPairFlag::eREMOVED_SHAPE_0 | PxContactPairFlag::eREMOVED_SHAPE_1))
			continue;
		if (!FBitSet(pair.events, PxPairFlag::eNOTIFY_TOUCH_PERSISTS)) {
			continue;
		}

		// Articulated .phys contacts are solver contacts, not legacy entity Touch()
		// events. Queuing one Touch per limb would spam gameplay callbacks and can
		// re-enter code that assumes one PhysX actor per edict. Dedicated ragdoll
		// contact events can be exposed separately later.
		if(pair.shapes[0] && pair.shapes[1])
		{
			CollisionFilterData shape0(pair.shapes[0]->getSimulationFilterData());
			CollisionFilterData shape1(pair.shapes[1]->getSimulationFilterData());
			if(shape0.IsPhysicsAssetBody() || shape1.IsPhysicsAssetBody())
				continue;
		}

		PxActor *a1 = pairHeader.actors[0];
		PxActor *a2 = pairHeader.actors[1];
		edict_t *e1 = (edict_t*)a1->userData;
		edict_t *e2 = (edict_t*)a2->userData;

		if (!e1 || !e2)
			continue;

		if (e1 && e1->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			m_impl->touchEventsQueue.push({ a1, a2 });
		}

		if (e2 && e2->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			m_impl->touchEventsQueue.push({ a2, a1 });
		}
	}
}
