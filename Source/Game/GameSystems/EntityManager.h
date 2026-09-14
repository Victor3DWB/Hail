#pragma once
#include "Types.h"
#include "Transforms.h"

#include "Containers/GrowingArray/GrowingArray.h"
#include "Entity.h"

namespace Hail
{


	class EntityManager
	{
	public:

		EntityManager();

		EntityHandle CreateEntity();
		void ReturnEntity(Entity entity);

		uint32 GetEntityCapacity();
		Entity& GetEntityFromIndex(EntityHandle entityIndex);

	private:

		uint32 m_firstAvailableEntity;
		GrowingArray<Entity> m_entities;
	};
}



