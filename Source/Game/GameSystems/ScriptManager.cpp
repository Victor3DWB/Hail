#include "Game_PCH.h"

#include "CapabilityBaseClass.h"
#include "CapabilitySystem.h"
#include "ComponentManager.h"
#include "EntityManager.h"
#include "ScriptManager.h"

#include "angelscript.h"
#include "Angelscript/TypeRegistry.h"
#include "Angelscript/Handler.h"
#include "Angelscript/Runner.h"

#include "CommonComponents.h"

namespace Hail
{
    // must be global to access internal script objects
    ScriptManager* g_pScriptManager = nullptr;
    CapabilitySystem* g_pCapabilitySystem = nullptr;
    Hail::Transform2D* LocalGetEntityTransform(EntityHandle entityId)
    {
        H_ASSERT(entityId != MAX_UINT);
        return g_pScriptManager->GetEntityTransform(entityId);
    }


    asIScriptObject* LocalGetAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
    {
        return g_pScriptManager->GetAngelscriptComponent(entityHandle, componentTypeId);
    }

    asIScriptObject* LocalGetOrCreateAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
    {
        asIScriptObject* pObj = g_pScriptManager->GetOrCreateAngelscriptComponent(entityHandle, componentTypeId);
        return pObj;
    }

    bool LocalHasAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
    {
        return g_pScriptManager->HasAngelscriptComponent(entityHandle, componentTypeId);
    }

    bool LocalCreateCapabilityForEntity(uint32 entityHandle, std::string& capabilityName, std::string& capabilityPath)
    {
        CapabilityCreationRequest capabilityRequest;
        capabilityRequest.m_capabilityName = capabilityName;
        capabilityRequest.m_scriptFilePath = FilePath::GetAngelscriptDirectory().Data();
        capabilityRequest.m_scriptFilePath += FileObject(capabilityPath.c_str(), "as", capabilityRequest.m_scriptFilePath);
        Capability* pCapability = g_pScriptManager->CreateCapability(capabilityRequest);
        if (pCapability)
        {
            g_pCapabilitySystem->RegisterCapabilityToEntity(entityHandle, pCapability);
            return true;
        }
        return false;
    }

    void LocalRemoveCapabilityFromEntity(uint32 entityHandle, std::string& capabilityName)
    {
        g_pCapabilitySystem->UnregisterCapabilityFromEntity(entityHandle, capabilityName.c_str());
    }
}


bool Hail::ScriptManager::Init(InitParams initParams)
{
    m_pComponentManager = initParams.m_pComponentManager;
    m_pEntityManager = initParams.m_pEntityManager;
    m_pScriptEngine = initParams.m_pHandler->GetScriptEngine();

    g_pScriptManager = this;
    g_pCapabilitySystem = initParams.m_pCapabilitySystem;

    initParams.m_pHandler->GetScriptEngine()->RegisterInterface("IComponent");
    bool bRegistryResult = RegisterGameScriptClasses(initParams.m_pHandler->GetTypeRegistry());
    H_DEBUGMESSAGE("Initializing game script manager");

    m_pRunner = new AngelScript::Runner();
    initParams.m_pHandler->SetActiveScriptRunner(m_pRunner);

    AngelScript::Runner::InitParams runnerInitParams;
    runnerInitParams.m_pCapabilitySystem = initParams.m_pCapabilitySystem;
    runnerInitParams.m_pScriptEngine = initParams.m_pHandler->GetScriptEngine();
    runnerInitParams.m_pTypeRegistry = initParams.m_pHandler->GetTypeRegistry();
    runnerInitParams.m_pComponentManager = initParams.m_pComponentManager;

    m_pRunner->Init(runnerInitParams);
    m_pRunner->AddMandatoryScriptPath("CapabilityBase", Capability::GetCapabilityAngelScriptBaseClassRelativePath());
    m_pRunner->AddMandatoryScriptPath("ComponentBase", "EngineScripts/ComponentBase.as");

    StringLW componentScriptPath = FilePath::GetAngelscriptDirectory().Data();
    componentScriptPath += L"Components.as";
    bRegistryResult &= m_pRunner->SetScriptDefinedComponents(FilePath(componentScriptPath));
    m_pRunner->AddMandatoryScriptPath("AngelscriptDefinedComponents", "Components.as");

    //StringLW firstScriptPath = FilePath::GetAngelscriptDirectory().Data();
    //firstScriptPath += L"FirstScript.as";
    //m_pRunner->ImportAndBuildScript(firstScriptPath.Data(), "FirstScript");

    return bRegistryResult;
}

void Hail::ScriptManager::Cleanup()
{
    m_pRunner->Cleanup();
    SAFEDELETE(m_pRunner);
}

void Hail::ScriptManager::Update(float deltaTime)
{
    //m_pRunner->RunScript("FirstScript");
    m_pRunner->UpdateScriptReloadWatcher();
    m_pRunner->UpdateCapabilityScriptReloadWatcher();

    m_pRunner->UpdateScriptDebugger();
}

Hail::Capability* Hail::ScriptManager::CreateCapability(CapabilityCreationRequest capabilityCreationRequest)
{
    Capability* pCapability = m_pRunner->CreateCapabilityFromScriptFile(capabilityCreationRequest);

    if (!pCapability)
    {
        H_ERROR(StringL::Format("Failed to create capability: %s", capabilityCreationRequest.m_capabilityName));
        return false;
    }

    return pCapability;
}

const Hail::FilePath& Hail::ScriptManager::GetScriptFilePathForCapability(const String64& capabilityName)
{
    return m_pRunner->GetScriptFilePathForCapability(capabilityName);
}

bool Hail::ScriptManager::RegisterGameScriptClasses(AngelScript::TypeRegistry* pTypeRegistry)
{
    Capability::RegisterAngelscriptFunctions(pTypeRegistry);

    bool bResult;
    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetEntityTransform", "Transform2D", false, true }, { {"entityHandle", "uint" } }, asFUNCTION(LocalGetEntityTransform), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");

    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetAngelscriptComponent", "IComponent", false, true }, { {"entityHandle", "uint" }, {"componentTypeId", "uint" } }, asFUNCTION(LocalGetAngelscriptComponent), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetOrCreateAngelscriptComponent", "IComponent", false, true }, { {"entityHandle", "uint" }, {"componentTypeId", "uint" } }, asFUNCTION(LocalGetOrCreateAngelscriptComponent), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    bResult = pTypeRegistry->RegisterGlobalMethod({ "HasAngelscriptComponent", "bool" }, { {"entityHandle", "uint" }, {"componentTypeId", "uint" } }, asFUNCTION(LocalHasAngelscriptComponent), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");


    bResult = pTypeRegistry->RegisterGlobalMethod({ "CreateCapabilityForEntity", "bool" }, { {"entityHandle", "uint" }, {"capabilityName", "string" }, {"capabilityPath", "string" } }, asFUNCTION(LocalCreateCapabilityForEntity), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    bResult = pTypeRegistry->RegisterGlobalMethod({ "RemoveCapabilityFromEntity", "void" }, { {"entityHandle", "uint" }, {"capabilityName", "string" } }, asFUNCTION(LocalRemoveCapabilityFromEntity), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    return true;
}

Hail::Transform2D* Hail::ScriptManager::GetEntityTransform(EntityHandle entityId)
{
    H_ASSERT(entityId != MAX_UINT);

    Entity& entity = m_pEntityManager->GetEntityFromIndex(entityId);
    return entity.GetTransform();
}

asIScriptObject* Hail::ScriptManager::GetAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
{
    AngelscriptComponent* pAngelScriptComponent = m_pComponentManager->GetAngelscriptComponent(entityHandle, componentTypeId);
    if (pAngelScriptComponent)
    {
        pAngelScriptComponent->m_pObject->AddRef();
        return pAngelScriptComponent->m_pObject;
    }
    return nullptr;
}

asIScriptObject* Hail::ScriptManager::GetOrCreateAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
{
    AngelscriptComponent* pAngelScriptComponent = m_pComponentManager->GetOrCreateAngelscriptComponent(entityHandle, componentTypeId, m_pScriptEngine);
    if (pAngelScriptComponent)
    {
        pAngelScriptComponent->m_pObject->AddRef();
        return pAngelScriptComponent->m_pObject;
    }
    return nullptr;
}

bool Hail::ScriptManager::HasAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
{
    return m_pComponentManager->HasAngelscriptComponent(entityHandle, componentTypeId);
}