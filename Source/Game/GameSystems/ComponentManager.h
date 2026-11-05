#pragma once
#include "Containers/GrowingArray/GrowingArray.h"
#include "CommonComponents.h"
#include "ComponentArray.h"

class asIScriptEngine;

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

		// TODO: Create return angelscriptComponentFunctions
		AngelscriptComponent* ComponentManager::GetAngelscriptComponent(EntityHandle entity, uint32 componentTypeId);
		AngelscriptComponent* ComponentManager::GetOrCreateAngelscriptComponent(EntityHandle entity, uint32 componentTypeId, asIScriptEngine* pScriptEngine);
		bool HasAngelscriptComponent(EntityHandle entity, uint32 componentTypeId);
		
		uint32 RegisterAngelscriptComponentTypeInfo(asITypeInfo* pTypeInfo);

	private:

		Hail::ComponentArray<PhysicsComponent> m_physicsComponents;
		Hail::ComponentArray<AngelscriptCapabilityComponent> m_capabilityComponents;

		struct AngelscriptComponentType
		{
			uint32 m_id;
			asITypeInfo* m_pScriptType;
			String64 m_name;
		};
		AngelscriptComponentType* GetAngelscriptComponentType(uint32 componentTypeId);

		struct EntityAngelscriptComponents
		{
			EntityHandle m_owningEntity;
			VectorOnStack<AngelscriptComponent, 8> m_components;
		};
		GrowingArray<EntityAngelscriptComponents> m_entitiesAngelscriptComponents;

		GrowingArray<AngelscriptComponentType> m_registeredAngelscriptComponentTypes;
		uint32 m_angelscriptComponentTypeCounter = 0;

	};

}
