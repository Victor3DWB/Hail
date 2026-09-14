#pragma once
#include "Containers/GrowingArray/GrowingArray.h"
#include "Entity.h"
#include "CapabilityTickManager.h"

namespace Hail
{
	class EntityManager;
	class ComponentManager;
	class Capability;
	class ScriptManager;

	class CapabilitySystem
	{
	public:

		struct CapabilitySystemInitParams
		{
			EntityManager* m_pEntityManager;
			ComponentManager* m_pComponentManager;
			ScriptManager* m_pScriptManager;
		};

		void Init(const CapabilitySystemInitParams& initParameters);

		void Update(float deltaTime);

		void RegisterCapabilityToEntity(const Entity& owningEntity, Capability* pCapability);
		void UnregisterCapabilityFromEntity(const Entity& owningEntity, Capability* pCapability);

		void CapabilityHasBeenReloaded(const String64& reloadedCapability);
		uint32 GetCapabilityLoadVersion(const String64& capabilityName);

	private:

		EntityManager* m_pEntityManager;
		ComponentManager* m_pComponentManager;
		ScriptManager* m_pScriptManager;

		bool m_bCapabilityHasBeenReloaded;

		struct CapabilityData
		{
			String64 m_name;
			uint32 m_loadVersion;
			GrowingArray<Capability*> m_pCapabilities;
		};
		GrowingArray<CapabilityData> m_registeredCapabilities;

		CapabilityTickWorld m_CapabilityTickWorld;
	};

}
