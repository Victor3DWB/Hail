#pragma once
#include "Containers/GrowingArray/GrowingArray.h"
#include "CommonComponents.h"
#include "ComponentArray.h"

namespace Hail
{
	enum class EComponentType
	{
		PhysicsComponent,
		AnngelScriptCapabilityComponent
	};

	class EntityManager;

	class ComponentManager
	{
	public:
		void Init(EntityManager* pEntityManager);

		bool DoesEntityHaveComponent(EntityHandle entityHandle, EComponentType componentToCheckFor);
		void AddComponentToEntity(EntityHandle entityHandle, EComponentType componentToAdd);

		PhysicsComponent* GetPhysicsComponent(EntityHandle entityHandle);
		AngelscriptCapabilityComponent* GetAngelscriptCapabilityComponent(EntityHandle entityHandle);
		Hail::ComponentArray<PhysicsComponent>& GetPhysicsComponents() { return m_physicsComponents; }
	private:
		Hail::ComponentArray<PhysicsComponent> m_physicsComponents;
		Hail::ComponentArray<AngelscriptCapabilityComponent> m_capabilityComponents;
	};

}
