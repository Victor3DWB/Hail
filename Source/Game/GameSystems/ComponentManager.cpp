#include "Game_PCH.h"
#include "ComponentManager.h"

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

}