#pragma once

#include "Containers\GrowingArray\GrowingArray.h"
#include "EntityManager.h"
#include "Transforms.h"

namespace Hail
{

	template <class ComponentType>
	class ComponentArray
	{
	public:

		void Init(uint32 size, EntityManager* pEntityManager);
		ComponentType& AddComponent(EntityHandle entityHandle);
		ComponentType* GetOrAddComponent(EntityHandle entityHandle);
		bool HasComponent(EntityHandle entityHandle) const;
		void RemoveComponent(EntityHandle entityHandle);
		Entity& GetEntityFromComponentIndex(uint32 index);
		ComponentType* GetComponentFromEntity(EntityHandle entityHandle);
		ComponentType* GetComponentFromIndex(uint32 componentIndex);
		uint32 GetSize() { return m_components.Size(); }

	private:

		EntityManager* m_pEntityManager;
		GrowingArray<ComponentType> m_components;
		GrowingArray<uint32> m_entityIdToComponentMap;
		GrowingArray<uint32> m_componentToEntityIdMap;
	};


	template<class ComponentType>
	inline void ComponentArray<ComponentType>::Init(uint32 size, EntityManager* pEntityManager)
	{
		m_pEntityManager = pEntityManager;
		m_components.Resize(size);

		const uint32 entityCapcacity = pEntityManager->GetEntityCapacity();
		m_entityIdToComponentMap.AddN(MAX_UINT, entityCapcacity);
		m_componentToEntityIdMap.AddN(MAX_UINT, entityCapcacity);
	}

	template <class ComponentType>
	inline ComponentType& ComponentArray<ComponentType>::AddComponent(EntityHandle entityHandle)
	{
		H_ASSERT(entityHandle != MAX_UINT);
		H_ASSERT(HasComponent(entityHandle) == false);

		const uint32 componentIndex = m_components.Size();
		m_components.Add(ComponentType());
		m_entityIdToComponentMap[entityHandle] = componentIndex;
		m_componentToEntityIdMap[componentIndex] = entityHandle;

		return m_components[componentIndex];
	}

	template <class ComponentType>
	inline ComponentType* ComponentArray<ComponentType>::GetOrAddComponent(EntityHandle entityHandle)
	{
		H_ASSERT(entityHandle != MAX_UINT);
		if (const uint32 existingComponentIndex = m_entityIdToComponentMap[entityHandle] != MAX_UINT)
		{
			return &m_components[existingComponentIndex];
		}

		const uint32 componentIndex = m_components.Size();
		m_components.Add(ComponentType());
		m_entityIdToComponentMap[entityHandle] = componentIndex;
		m_componentToEntityIdMap[componentIndex] = entityHandle;

		return &m_components[componentIndex];
	}

	template<class ComponentType>
	inline bool ComponentArray<ComponentType>::HasComponent(EntityHandle entityHandle) const
	{
		H_ASSERT(entityHandle != MAX_UINT);
		return m_entityIdToComponentMap[entityHandle] != MAX_UINT;
	}

	template <class ComponentType>
	inline void ComponentArray<ComponentType>::RemoveComponent(EntityHandle entityHandle)
	{
		H_ASSERT(entityHandle != MAX_UINT);

		const uint32 componentIndex = m_entityIdToComponentMap[entityHandle];

		H_ASSERT(componentIndex != MAX_UINT, "Removing component that has not been created by target entity.");

		const uint32 lastComponentIndex = m_components.Size() - 1;

		m_components.RemoveCyclicAtIndex(componentIndex);
		m_entityIdToComponentMap[m_componentToEntityIdMap[lastComponentIndex]] = componentIndex;
		m_componentToEntityIdMap[componentIndex] = m_componentToEntityIdMap[lastComponentIndex];

		m_componentToEntityIdMap[lastComponentIndex] = MAX_UINT;
		m_entityIdToComponentMap[componentIndex] = MAX_UINT;
	}

	template<class ComponentType>
	inline Entity& ComponentArray<ComponentType>::GetEntityFromComponentIndex(uint32 index)
	{
		H_ASSERT(index < m_pEntityManager->GetEntityCapacity());
		return m_pEntityManager->GetEntityFromIndex(m_componentToEntityIdMap[index]);
	}

	template<class ComponentType>
	inline ComponentType* ComponentArray<ComponentType>::GetComponentFromEntity(EntityHandle entityHandle)
	{
		H_ASSERT(entityHandle != MAX_UINT);

		const uint32 componentIndex = m_entityIdToComponentMap[entityHandle];
		H_ASSERT(componentIndex != MAX_UINT, "Trying to fetch component that has not been created by target entity.");

		return &m_components[componentIndex];
	}

	template<class ComponentType>
	inline ComponentType* ComponentArray<ComponentType>::GetComponentFromIndex(uint32 componentIndex)
	{
		H_ASSERT(componentIndex < GetSize());

		return &m_components[componentIndex];
	}

}



