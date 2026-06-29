#include "Game_PCH.h"

#include "CapabilityBaseClass.h"
#include "ScriptManager.h"

#include "angelscript.h"
#include "Engine/Angelscript/TypeRegistry.h"
#include "Engine/Angelscript/Handler.h"
#include "Engine/Angelscript/Runner.h"

bool Hail::ScriptManager::Init(AngelScript::Handler* pHandler, AngelScript::TypeRegistry* pTypeRegistry)
{
    bool bRegistryResult = RegisterGameScriptClasses(pTypeRegistry);
    H_DEBUGMESSAGE("Initializing game script manager");


    m_pRunner = new AngelScript::Runner();
    pHandler->SetActiveScriptRunner(m_pRunner);
    m_pRunner->Initialize(pHandler->GetScriptEngine(), pTypeRegistry);
    m_pRunner->AddMandatoryScriptPath("CapabilityBase", Capability::GetCapabilityAngelScriptBaseClassRelativePath());

    StringLW firstScriptPath = FilePath::GetAngelscriptDirectory().Data();
    firstScriptPath += L"FirstScript.as";
    m_pRunner->ImportAndBuildScript(firstScriptPath.Data(), "FirstScript");

    return bRegistryResult;
}

void Hail::ScriptManager::Cleanup()
{
    m_pRunner->Cleanup();
    SAFEDELETE(m_pRunner);
}

void Hail::ScriptManager::Update()
{
    m_pRunner->RunScript("FirstScript");
    m_pRunner->Update();
    // TODO: Hooka in alla capability grejer här :D 
}

bool Hail::ScriptManager::RegisterGameScriptClasses(AngelScript::TypeRegistry* pTypeRegistry)
{
    RegisterCapability(pTypeRegistry);
    return true;
}
