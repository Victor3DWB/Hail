#include "Game_PCH.h"
#include "ComponentManager.h"

#include "angelscript.h"
#include "Angelscript/TypeRegistry.h"

namespace Hail
{
	void ComponentManager::Init(EntityManager* pEntityManager)
	{
		m_physicsComponents.Init(128, pEntityManager);
		m_capabilityComponents.Init(64, pEntityManager);
	}

	bool ComponentManager::DoesEntityHaveComponent(EntityHandle entityHandle, EComponentType componentToCheckFor)
	{
		switch (componentToCheckFor)
		{
		case Hail::EComponentType::PhysicsComponent:
			return m_physicsComponents.HasComponent(entityHandle);
			break;
		case Hail::EComponentType::AnngelScriptCapabilityComponent:
			return m_capabilityComponents.HasComponent(entityHandle);
			break;
		default:
			break;
		}
		return false;
	}

	void ComponentManager::AddComponentToEntity(EntityHandle entityHandle, EComponentType componentToAdd)
	{
		switch (componentToAdd)
		{
		case Hail::EComponentType::PhysicsComponent:
			m_physicsComponents.AddComponent(entityHandle);
			break;
		case Hail::EComponentType::AnngelScriptCapabilityComponent:
			m_capabilityComponents.AddComponent(entityHandle);
			break;
		default:
			break;
		}
	}

	PhysicsComponent* ComponentManager::GetPhysicsComponent(EntityHandle entityHandle)
	{
		return m_physicsComponents.GetComponentFromEntity(entityHandle);
	}

	AngelscriptCapabilityComponent* ComponentManager::GetAngelscriptCapabilityComponent(EntityHandle entityHandle)
	{
		return m_capabilityComponents.GetComponentFromEntity(entityHandle);
	}

	AngelscriptComponent* ComponentManager::GetAngelscriptComponent(EntityHandle entity, uint32 typeId)
	{
		AngelscriptComponentType* pRegisteredTypeId = GetAngelscriptComponentType(typeId);

		if (!pRegisteredTypeId)
		{
			H_ASSERT("Type is not registered to the component manager");
			return nullptr;
		}

		for (uint32 i = 0; i < m_entitiesAngelscriptComponents.Size(); i++)
		{
			EntityAngelscriptComponents& entityAngelscriptComponents = m_entitiesAngelscriptComponents[i];
			if (entityAngelscriptComponents.m_owningEntity == entity)
			{
				for (uint32 iComponent = 0; iComponent < entityAngelscriptComponents.m_components.Size(); iComponent++)
				{
					if (entityAngelscriptComponents.m_components[iComponent].m_typeId == pRegisteredTypeId->m_id)
					{
						return &entityAngelscriptComponents.m_components[iComponent];
					}
				}
				break;
			}
		}
		return nullptr;
	}

	AngelscriptComponent* ComponentManager::GetOrCreateAngelscriptComponent(EntityHandle entity, uint32 typeId, asIScriptEngine* pScriptEngine)
	{
		H_ASSERT(pScriptEngine);

		AngelscriptComponentType* pRegisteredTypeId = GetAngelscriptComponentType(typeId);

		if (!pRegisteredTypeId)
		{
			H_ASSERT("Type is not registered to the component manager");
			return nullptr;
		}

		int32 registeredEntityId = -1;

		for (uint32 i = 0; i < m_entitiesAngelscriptComponents.Size(); i++)
		{
			EntityAngelscriptComponents& entityAngelscriptComponents = m_entitiesAngelscriptComponents[i];
			if (entityAngelscriptComponents.m_owningEntity == entity)
			{
				registeredEntityId = i;
				for (uint32 iComponent = 0; iComponent < entityAngelscriptComponents.m_components.Size(); iComponent++)
				{
					if (entityAngelscriptComponents.m_components[iComponent].m_typeId == pRegisteredTypeId->m_id)
					{
						return &entityAngelscriptComponents.m_components[iComponent];
					}
				}
				break;
			}
		}

		asIScriptObject* pObject = static_cast<asIScriptObject*>(pScriptEngine->CreateScriptObject(pRegisteredTypeId->m_pScriptType));

		if (registeredEntityId == -1)
		{
			registeredEntityId = m_entitiesAngelscriptComponents.Size();
			EntityAngelscriptComponents& entityAngelscriptComponents = m_entitiesAngelscriptComponents.Add();
			entityAngelscriptComponents.m_owningEntity = entity;
		}

		EntityAngelscriptComponents& entityAngelscriptComponents = m_entitiesAngelscriptComponents[registeredEntityId];

		AngelscriptComponent& angelscriptComponent = entityAngelscriptComponents.m_components.Add();
		angelscriptComponent.m_pObject = pObject;
		angelscriptComponent.m_typeId = pRegisteredTypeId->m_id;
		return &angelscriptComponent;
	}

	bool ComponentManager::HasAngelscriptComponent(EntityHandle entity, uint32 componentTypeId)
	{
		for (uint32 i = 0; i < m_entitiesAngelscriptComponents.Size(); i++)
		{
			EntityAngelscriptComponents& entityAngelscriptComponents = m_entitiesAngelscriptComponents[i];
			if (entityAngelscriptComponents.m_owningEntity == entity)
			{
				for (uint32 iComponent = 0; iComponent < entityAngelscriptComponents.m_components.Size(); iComponent++)
				{
					if (entityAngelscriptComponents.m_components[iComponent].m_typeId == componentTypeId)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	uint32 ComponentManager::RegisterAngelscriptComponentTypeInfo(asITypeInfo* pTypeInfo)
	{
		for (uint32 i = 0; i < m_registeredAngelscriptComponentTypes.Size(); i++)
		{
			H_ASSERT(m_registeredAngelscriptComponentTypes[i].m_pScriptType != pTypeInfo, "Type already registered");
		}
		AngelscriptComponentType& registeredAngelscriptComponentType = m_registeredAngelscriptComponentTypes.Add();
		registeredAngelscriptComponentType.m_id = m_angelscriptComponentTypeCounter++;
		registeredAngelscriptComponentType.m_pScriptType = pTypeInfo;
		registeredAngelscriptComponentType.m_name = pTypeInfo->GetName();
		return registeredAngelscriptComponentType.m_id;
	}

	ComponentManager::AngelscriptComponentType* ComponentManager::GetAngelscriptComponentType(uint32 componentTypeId)
	{
		for (uint32 i = 0; i < m_registeredAngelscriptComponentTypes.Size(); i++)
		{
			if (m_registeredAngelscriptComponentTypes[i].m_id == componentTypeId)
			{
				return &m_registeredAngelscriptComponentTypes[i];
			}
		}
		return nullptr;
	}

}