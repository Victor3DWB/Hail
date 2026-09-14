#include "Game_PCH.h"

#include "CapabilityBaseClass.h"
#include "ScriptManager.h"
#include "EntityManager.h"

#include "angelscript.h"
#include "Angelscript/TypeRegistry.h"
#include "Angelscript/Handler.h"
#include "Angelscript/Runner.h"

#include "CommonComponents.h"

namespace Hail
{
    // must be global to access internal script objects
    ScriptManager* g_pScriptManager = nullptr;
    Hail::Transform2D* LocalGetEntityTransform(EntityHandle entityId)
    {
        H_ASSERT(entityId != MAX_UINT);
        return g_pScriptManager->GetEntityTransform(entityId);
    }
}


bool Hail::ScriptManager::Init(InitParams initParams)
{
    g_pScriptManager = this;
    bool bRegistryResult = RegisterGameScriptClasses(initParams.m_pHandler->GetTypeRegistry());
    H_DEBUGMESSAGE("Initializing game script manager");

    m_pRunner = new AngelScript::Runner();
    initParams.m_pHandler->SetActiveScriptRunner(m_pRunner);
    AngelScript::Runner::InitParams runnerInitParams;
    runnerInitParams.m_pCapabilitySystem = initParams.m_pCapabilitySystem;
    runnerInitParams.m_pScriptEngine = initParams.m_pHandler->GetScriptEngine();
    runnerInitParams.m_pTypeRegistry = initParams.m_pHandler->GetTypeRegistry();
    m_pRunner->Init(runnerInitParams);
    m_pRunner->AddMandatoryScriptPath("CapabilityBase", Capability::GetCapabilityAngelScriptBaseClassRelativePath());

    m_pComponentManager = initParams.m_pComponentManager;
    m_pEntityManager = initParams.m_pEntityManager;
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

    // Create functions to get components :D 



    return true;
}

Hail::Transform2D* Hail::ScriptManager::GetEntityTransform(EntityHandle entityId)
{
    H_ASSERT(entityId != MAX_UINT);

    Entity& entity = m_pEntityManager->GetEntityFromIndex(entityId);
    return entity.GetTransform();
}