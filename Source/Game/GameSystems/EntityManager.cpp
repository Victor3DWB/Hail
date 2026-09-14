#include "Game_PCH.h"

#include "EntityManager.h"

namespace Hail
{
	EntityManager::EntityManager()
	{
		m_firstAvailableEntity = 0;
		const uint32 startNumberOfEntities = 128;
		m_entities.Resize(startNumberOfEntities);
		for (size_t i = 0; i < startNumberOfEntities; i++)
		{
			Entity& entity = m_entities.Add();
			entity.m_next = i + 1;
			entity.m_id = MAX_UINT;
		}
	}

	EntityHandle EntityManager::CreateEntity()
	{
		H_ASSERT(m_firstAvailableEntity <= m_entities.Size());

		Entity& returnEntity = m_entities[m_firstAvailableEntity];
		returnEntity.m_id = m_firstAvailableEntity;
		m_firstAvailableEntity = returnEntity.m_next;
		return returnEntity.m_id;
	}

	void EntityManager::ReturnEntity(Entity entity)
	{
		H_ASSERT(m_entities[entity.m_id].m_id != MAX_UINT, "Returning an already returned entity");
		m_entities[entity.m_id].m_next = m_firstAvailableEntity;
		m_entities[entity.m_id].m_id = MAX_UINT;
		m_firstAvailableEntity = entity.m_id;
	}

	uint32 EntityManager::GetEntityCapacity()
	{
		return m_entities.Size();
	}

	Entity& EntityManager::GetEntityFromIndex(uint32 entityIndex)
	{
		H_ASSERT(m_entities[entityIndex].IsValid());
		return m_entities[entityIndex];
	}
}