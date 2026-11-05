#include "Game_PCH.h"
#include "CapabilitySystem.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"

namespace Hail
{
	void CapabilitySystem::Init(const CapabilitySystemInitParams& initParameters)
	{
		m_pEntityManager = initParameters.m_pEntityManager;
		m_pComponentManager = initParameters.m_pComponentManager;
		m_pScriptManager = initParameters.m_pScriptManager;
		m_bCapabilityHasBeenReloaded = false;
	}

	// TODO: Figure out update order
	// Probably this: 
	// ScriptManager for reloads and debugger:
	// Then capabilitySystem for applying reloads
	// Then update tick world


	void CapabilitySystem::Update(float deltaTime)
	{
		if (m_bCapabilityHasBeenReloaded)
		{
			// Check each registered Capability type
			bool bFoundCapabilityForReload = false;
			for (uint32 iCapabilityData = 0; iCapabilityData < m_registeredCapabilities.Size(); iCapabilityData++)
			{
				CapabilityData& capabilityData = m_registeredCapabilities[iCapabilityData];
				const String64& reloadedCapabilityName = capabilityData.m_name;

				// For each capability
				for (uint32 iCapability = 0; iCapability < capabilityData.m_pCapabilities.Size(); iCapability++)
				{
					Capability* pCapability = capabilityData.m_pCapabilities[iCapability];
					const EntityHandle owningEntityHandle = pCapability->m_owningEntityId;
					AngelscriptCapabilityComponent* pCapabilityComponent = m_pComponentManager->GetAngelscriptCapabilityComponent(owningEntityHandle);
					H_ASSERT(pCapabilityComponent);
					// Find each component that has registered the capability and check it for out of date
					for (uint32 i = 0; i < pCapabilityComponent->m_capabilities.Size(); i++)
					{
						AngelscriptCapabilityComponent::AngelscriptCapabilityData& componentCapabilityData = pCapabilityComponent->m_capabilities[i];
						if (StringCompare(reloadedCapabilityName, componentCapabilityData.m_capabilityName) && componentCapabilityData.m_loadVersion != capabilityData.m_loadVersion)
						{
							m_CapabilityTickWorld.UnRegisterCapability(pCapability);
							pCapability->OnOwnerDestroyed();
							pCapability->Release();
							H_ASSERT(pCapability->GetRefCount() == 0);
							SAFEDELETE(pCapability);

							// Reload this capability
							CapabilityCreationRequest creationRequest;
							creationRequest.m_scriptFilePath = m_pScriptManager->GetScriptFilePathForCapability(reloadedCapabilityName);
							creationRequest.m_capabilityName = reloadedCapabilityName;
							Capability* pReloadedCapability = m_pScriptManager->CreateCapability(creationRequest);

							componentCapabilityData.m_capabilityId = pReloadedCapability->GetId();
							componentCapabilityData.m_loadVersion = capabilityData.m_loadVersion;

							pCapability = pReloadedCapability;
							pCapability->SetOwningEntity(owningEntityHandle);
							capabilityData.m_pCapabilities[iCapability] = pCapability;

							m_CapabilityTickWorld.RegisterCapability(*pCapability);
							bFoundCapabilityForReload = true;
							break;
						}
					}
				}
			}
			H_ASSERT(bFoundCapabilityForReload, "This should not happen, so sanity checking for logic errors");
			m_bCapabilityHasBeenReloaded = false;
		}

		m_CapabilityTickWorld.Tick(deltaTime);
	}

	void CapabilitySystem::RegisterCapabilityToEntity(const EntityHandle& owningEntity, Capability* pCapability)
	{
		if (!m_pComponentManager->DoesEntityHaveComponent(owningEntity, EComponentType::AnngelScriptCapabilityComponent))
		{
			m_pComponentManager->AddComponentToEntity(owningEntity, EComponentType::AnngelScriptCapabilityComponent);
		}

		const char* pCapabilityName = pCapability->GetCapabilityName();

		AngelscriptCapabilityComponent* pCapabilityComponent = m_pComponentManager->GetAngelscriptCapabilityComponent(owningEntity);
		H_ASSERT(pCapabilityComponent);
 		for (uint32 i = 0; i < pCapabilityComponent->m_capabilities.Size(); i++)
		{
			// Check if it already is registered:
			if (StringCompare(pCapabilityName, pCapabilityComponent->m_capabilities[i].m_capabilityName))
			{
				H_ASSERT(false, "Capability already registered on this component");
				// Maybe delete capability here? 
				return;
			}
		}

		pCapability->SetOwningEntity(owningEntity);

		CapabilityData* pRegisteredCapabilityData = nullptr;
		for (uint32 iCapability = 0; iCapability < m_registeredCapabilities.Size(); iCapability++)
		{
			if (StringCompare(m_registeredCapabilities[iCapability].m_name, pCapabilityName))
			{
				pRegisteredCapabilityData = &m_registeredCapabilities[iCapability];
				break;
			}
		}

		if (!pRegisteredCapabilityData)
		{
			CapabilityData& newCapabilityData = m_registeredCapabilities.Add();
			newCapabilityData.m_name = pCapabilityName;
			newCapabilityData.m_loadVersion = 0;
			pRegisteredCapabilityData = &newCapabilityData;
		}

		pRegisteredCapabilityData->m_pCapabilities.Add(pCapability);

		AngelscriptCapabilityComponent::AngelscriptCapabilityData& capabilityData = pCapabilityComponent->m_capabilities.Add();
		capabilityData.m_capabilityName = pCapabilityName;
		capabilityData.m_loadVersion = pRegisteredCapabilityData->m_loadVersion;
		capabilityData.m_capabilityId = pCapability->GetId();

		m_CapabilityTickWorld.RegisterCapability(*pCapability);
	}

	void CapabilitySystem::UnregisterCapabilityFromEntity(const EntityHandle& owningEntity, const char* pCapabilityName)
	{
		if (!m_pComponentManager->DoesEntityHaveComponent(owningEntity, EComponentType::AnngelScriptCapabilityComponent))
		{
			return;
		}

		CapabilityData* pRegisteredCapabilityData = nullptr;

		for (uint32 iCapability = 0; iCapability < m_registeredCapabilities.Size(); iCapability++)
		{
			if (StringCompare(m_registeredCapabilities[iCapability].m_name, pCapabilityName))
			{
				pRegisteredCapabilityData = &m_registeredCapabilities[iCapability];
				break;
			}
		}

		if (!pRegisteredCapabilityData)
		{
			return;
		}

		AngelscriptCapabilityComponent* pCapabilityComponent = m_pComponentManager->GetAngelscriptCapabilityComponent(owningEntity);
		for (uint32 i = 0; i < pCapabilityComponent->m_capabilities.Size(); i++)
		{
			if (StringCompare(pCapabilityName, pCapabilityComponent->m_capabilities[i].m_capabilityName))
			{
				AngelscriptCapabilityComponent::AngelscriptCapabilityData& componentData = pCapabilityComponent->m_capabilities[i];
				
				for (uint32 iRegisteredCapability = 0; iRegisteredCapability < pRegisteredCapabilityData->m_pCapabilities.Size(); iRegisteredCapability++)
				{
					Capability* pCapability = pRegisteredCapabilityData->m_pCapabilities[iRegisteredCapability];
					if (pCapability->GetId() == componentData.m_capabilityId)
					{
						m_CapabilityTickWorld.UnRegisterCapability(pCapability);
						pRegisteredCapabilityData->m_pCapabilities.RemoveCyclicAtIndex(iRegisteredCapability);
						pCapabilityComponent->m_capabilities.RemoveCyclicAtIndex(i);
						pCapability->OnOwnerDestroyed();
						pCapability->Release();
						H_ASSERT(pCapability->GetRefCount() == 0);
						SAFEDELETE(pCapability);
						return;
					}
					H_ASSERT(StringCompare(pCapability->GetCapabilityName(), pCapabilityName) == false, "Capability is registered to this entity, but has the incorrect id");
				}
				break;
			}
		}
	}

	void CapabilitySystem::CapabilityHasBeenReloaded(const String64& reloadedCapability)
	{
		CapabilityData* pRegisteredCapabilityData = nullptr;
		for (uint32 iCapability = 0; iCapability < m_registeredCapabilities.Size(); iCapability++)
		{
			if (StringCompare(m_registeredCapabilities[iCapability].m_name, reloadedCapability))
			{
				pRegisteredCapabilityData = &m_registeredCapabilities[iCapability];
				break;
			}
		}
		H_ASSERT(pRegisteredCapabilityData, "Unregistered capability has been reloaded");
		pRegisteredCapabilityData->m_loadVersion++;
		m_bCapabilityHasBeenReloaded = true;
	}

	uint32 CapabilitySystem::GetCapabilityLoadVersion(const String64& capabilityName)
	{
		CapabilityData* pRegisteredCapabilityData = nullptr;
		for (uint32 iCapability = 0; iCapability < m_registeredCapabilities.Size(); iCapability++)
		{
			if (StringCompare(m_registeredCapabilities[iCapability].m_name, capabilityName))
			{
				pRegisteredCapabilityData = &m_registeredCapabilities[iCapability];
				break;
			}
		}
		H_ASSERT(pRegisteredCapabilityData, "Unregistered capability has been reloaded");
		return pRegisteredCapabilityData->m_loadVersion;
	}
}
//void CapabilitySheet::ApplyOn(ComponentOwner& Owner)
//{
//    
//}
