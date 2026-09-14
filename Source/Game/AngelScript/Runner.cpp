#include "Game_PCH.h"
#include "Runner.h"

#include "TypeRegistry.h"
#include "Scriptbuilder.h"
#include "Debugger.h"

#include "GameSystems/CapabilitySystem.h"

using namespace Hail;

#include <sstream>


void Hail::AngelScript::Runner::Init(const InitParams& initParams)
{
	m_pScriptEngine = initParams.m_pScriptEngine;
	m_pTypeRegistry = initParams.m_pTypeRegistry;
	m_pCapabilitySystem = initParams.m_pCapabilitySystem;
	
	//TODO: Disable on non debug
	m_pDebuggerServer = new DebuggerServer(m_pTypeRegistry->GetDebuggerRegistry());
}

void Hail::AngelScript::Runner::ImportAndBuildScript(const FilePath& filePath, String64 scriptName)
{
	for (int i = 0; i < m_scripts.Size(); i++)
	{
		if (m_scripts[i].m_filePath == filePath)
			return;
	}

	Script newScript(filePath, false);

	CreateScript(scriptName, newScript);
	m_scripts.Add(newScript);
}

void Hail::AngelScript::Runner::RunScript(String64 scriptName)
{
	H_ASSERT(m_pScriptEngine, "Must have a script engine on the script runner.");

	int scriptIndex = -1;

	for (int i = 0; i < m_scripts.Size(); i++)
	{
		if (m_scripts[i].m_name == scriptName)
			scriptIndex = i;
	}

	if (scriptIndex < 0)
	{
		//H_ERROR(StringL::Format("No script loaded called %s", scriptName));
		return;
	}

	Script& script = m_scripts[scriptIndex];

	if (script.loadStatus == eScriptLoadStatus::FailedToLoad)
	{
		return;
	}

	if (m_pDebuggerServer)
	{
		m_pDebuggerServer->SetScriptToDebug(&script);
	}


	asIScriptModule* mod = m_pScriptEngine->GetModule(script.m_name.Data());
	asIScriptFunction* func = mod->GetFunctionByDecl("void main()");
	if (func == 0)
	{
		// The function couldn't be found. Instruct the script writer
		// to include the expected function in the script.
		H_ERROR("The script must have the function 'void main()'. Please add it and try again.");
		return;
	}

	script.m_pScriptContext->Prepare(func);

#ifdef DEBUG
	// Tell the context to invoke the debugger's line callback
	//script.m_pScriptContext->SetLineCallback(asMETHOD(CDebugger, LineCallback), m_pDebugger, asCALL_THISCALL);
	// Allow the user to initialize the debugging before moving on
	//m_pDebugger->TakeCommands(script.m_pScriptContext);
	// Execute the script normally. If a breakpoint is reached the 
	// debugger will take over the control loop.
	int r = script.m_pScriptContext->Execute();
#else
	int r = script.m_pScriptContext->Execute();
#endif

	if (r != asEXECUTION_FINISHED)
	{
		// The execution didn't complete as expected. Determine what happened.
		if (r == asEXECUTION_EXCEPTION)
		{
			// An exception occurred, let the script writer know what happened so it can be corrected.
			H_ERROR(StringL::Format("An exception '%s' occurred. Please correct the code and try again.", script.m_pScriptContext->GetExceptionString()));
		}
	}
}

void Hail::AngelScript::Runner::UpdateScriptReloadWatcher()
{
	m_bScriptsHaveReloaded = false;
	// Reloading logic below
	for (int i = 0; i < m_scripts.Size(); i++)
	{
		Script& script = m_scripts[i];
		m_bScriptsHaveReloaded |= CheckForScriptReload(script);
	}
}

void Hail::AngelScript::Runner::UpdateCapabilityScriptReloadWatcher()
{
	for (int i = 0; i < m_capabilityScripts.Size(); i++)
	{
		Script& script = m_capabilityScripts[i];
		bool bCapabilityIsReloaded = CheckForScriptReload(script);
		if (bCapabilityIsReloaded)
		{
			m_pCapabilitySystem->CapabilityHasBeenReloaded(script.m_name);
			m_bScriptsHaveReloaded = true;
		}
	}
}

void Hail::AngelScript::Runner::UpdateScriptDebugger()
{
	bool bAreScriptsReloading = m_bScriptsHaveReloaded;
	if (m_pDebuggerServer)
	{
		// Check for new connections and messages on the server if debugging
		m_pDebuggerServer->Update(bAreScriptsReloading);
	}
}

void Hail::AngelScript::Runner::Cleanup()
{
	for (int i = 0; i < m_scripts.Size(); i++)
	{
		if (m_scripts[i].m_pScriptContext != nullptr)
		{
			m_scripts[i].m_pScriptContext->Release();
		}
		SAFEDELETE(m_scripts[i].m_pDebugger);
	}
	m_scripts.RemoveAll();
	SAFEDELETE(m_pDebuggerServer);

#ifdef DEBUG
	//m_pDebugger->SetEngine(nullptr);
	//SAFEDELETE(m_pDebugger)
#endif
}

void Hail::AngelScript::Runner::AddMandatoryScriptPath(const char* pSectionName, const char* pMandatoryRelativeScriptPath)
{
	MandatoryScriptInclude& scriptSection = m_mandatoryScriptIncludes.Add();
	scriptSection.m_name = pSectionName;
	scriptSection.m_pRelativePath = pMandatoryRelativeScriptPath;

	if (m_pDebuggerServer)
	{
		m_pDebuggerServer->AddMandatoryIncludePath(pMandatoryRelativeScriptPath);
	}
}

Capability* Hail::AngelScript::Runner::CreateCapabilityFromScriptFile(const CapabilityCreationRequest& creationRequest)
{
	H_ASSERT(!creationRequest.m_scriptFilePath.IsEmpty(), "Scriptpath must be valid");

	asIScriptContext* pScriptContext = nullptr;

	Script* pCapabilityScript = nullptr;
	for (uint32 i = 0; i < m_capabilityScripts.Size(); i++)
	{
		if (m_capabilityScripts[i].m_name == creationRequest.m_capabilityName)
		{
			pCapabilityScript = &m_capabilityScripts[i];
			pScriptContext = m_capabilityScripts[i].m_pScriptContext;
			break;
		}
	}
	if (!pCapabilityScript)
	{
		Script capabilityScript(creationRequest.m_scriptFilePath, true);

		if (!CreateScript(creationRequest.m_capabilityName, capabilityScript))
		{
			return nullptr;
		}
		pScriptContext = capabilityScript.m_pScriptContext;
		m_capabilityScripts.Add(capabilityScript);
		pCapabilityScript = &m_capabilityScripts.GetLast();
	}

	asIScriptModule* pScriptModule = m_pScriptEngine->GetModule(creationRequest.m_capabilityName.Data());

	asITypeInfo* type = pScriptModule->GetTypeInfoByDecl(creationRequest.m_capabilityName.Data());

	StringL factoryDeclaration = StringL::Format("%s @%s()", creationRequest.m_capabilityName.Data(), creationRequest.m_capabilityName.Data());
	// Get the factory function from the object type
	asIScriptFunction* factory = type->GetFactoryByDecl(factoryDeclaration.Data());

	// Prepare the context to call the factory function
	pScriptContext->Prepare(factory);

	// Execute the call
	int result = pScriptContext->Execute();

	if (result != asEXECUTION_FINISHED)
	{
		// Handle error
		return nullptr;
	}

	// Get the object that was created
	asIScriptObject* obj = *(asIScriptObject**)pScriptContext->GetAddressOfReturnValue();
	if (obj)
	{
		obj->AddRef();
	}

	void* userData = obj->GetUserData((uint32)ERegisteredAngelScriptClasses::Capability);
	Capability* pCapability = reinterpret_cast<Capability*>(userData);

	return pCapability;
}

const FilePath& Hail::AngelScript::Runner::GetScriptFilePathForCapability(const String64& capabilityName)
{
	for (uint32 i = 0; i < m_capabilityScripts.Size(); i++)
	{
		if (m_capabilityScripts[i].m_name == capabilityName)
		{
			return m_capabilityScripts[i].m_filePath;
		}
	}
	return {};
}

bool Hail::AngelScript::Runner::CheckForScriptReload(Script& scriptToCheckForReload)
{
	//Adding a delay to the reloading, as there can be a frame or two where the filesystem is still saving the script. Leading to a load error.
	if (!scriptToCheckForReload.m_bIsDirty)
	{
		if (scriptToCheckForReload.m_lastWriteTime != scriptToCheckForReload.m_filePath.GetCurrentLastWriteFileTime())
		{
			scriptToCheckForReload.m_bIsDirty = true;
			scriptToCheckForReload.m_reloadDelay = 0;
		}
	}

	if (scriptToCheckForReload.m_bIsDirty && scriptToCheckForReload.m_lastWriteTime != scriptToCheckForReload.m_filePath.GetCurrentLastWriteFileTime())
	{
		if (scriptToCheckForReload.m_reloadDelay > 16)
		{
			if (ReloadScript(scriptToCheckForReload))
			{
				scriptToCheckForReload.m_lastWriteTime = scriptToCheckForReload.m_filePath.GetCurrentLastWriteFileTime();
				scriptToCheckForReload.m_bIsDirty = false;
				return true;
			}
			scriptToCheckForReload.m_reloadDelay = 0;
		}
		else
		{
			scriptToCheckForReload.m_reloadDelay++;
		}
	}
	return false;
}

bool Hail::AngelScript::Runner::CreateScript(String64 scriptName, Script& scriptToFill)
{
	H_ASSERT(!scriptToFill.m_filePath.IsEmpty(), "Script must have been created with a filepath");

	scriptToFill.m_lastWriteTime = scriptToFill.m_filePath.GetCurrentLastWriteFileTime();
	if (!scriptToFill.m_filePath.IsValid())
	{
		H_ERROR("Invalid script path");
		scriptToFill.loadStatus = eScriptLoadStatus::FailedToLoad;
		return false;
	}

	H_ASSERT(m_pScriptEngine, "Must have a script engine on the importer.");

	// Reloading script
	bool bReloadScriptModule = false;
	if (scriptToFill.m_pScriptContext || scriptToFill.m_pDebugger)
	{
		bReloadScriptModule = scriptToFill.m_bIsCapabilityScript ? CreateCapabilityModule(scriptName, scriptToFill.m_filePath, true) : CreateScriptFileModule(scriptName, scriptToFill.m_filePath, true);
		if (bReloadScriptModule)
		{
			m_pScriptEngine->DiscardModule("ReloadScript");
			if (scriptToFill.m_pScriptContext)
			{
				scriptToFill.m_pScriptContext->Release();
			}
			scriptToFill.m_pScriptContext = nullptr;
		}
		else
		{
			scriptToFill.loadStatus = eScriptLoadStatus::FailedToReload;
			return false;
		}
	}

	if (scriptToFill.m_bIsCapabilityScript) 
	{
		// TODO: Maybe destroy all capabilities here
		//for (uint32 i = 0; i < m_capabilityScripts.Size(); i++)
		//{
		//	if (m_capabilityScripts[i].m_name == scriptName && bReloadScriptModule)
		//	{
		//		break;
		//	}
		//}

		if (!CreateCapabilityModule(scriptName, scriptToFill.m_filePath, false)) 
		{
			scriptToFill.loadStatus = eScriptLoadStatus::FailedToLoad;
			return false;
		}
	}
	else
	{
		if (!CreateScriptFileModule(scriptName, scriptToFill.m_filePath, false))
		{
			scriptToFill.loadStatus = eScriptLoadStatus::FailedToLoad;
			return false;
		}
	}

	scriptToFill.m_fileNames.RemoveAll();
	//TODO add all script fileNames
	scriptToFill.m_fileNames.Add(scriptToFill.m_filePath.Object().Name().ToCharString());

	// Create our context, prepare it, and then execute
	asIScriptContext* pCtx = m_pScriptEngine->CreateContext();
	scriptToFill.m_pScriptContext = pCtx;
	scriptToFill.m_name = scriptName;
	scriptToFill.loadStatus = eScriptLoadStatus::NoError;
	scriptToFill.m_reloadDelay = 0;
	scriptToFill.m_bIsDirty = false;
	// Set some setting or way to toggle the debugger off.
	if (scriptToFill.m_pDebugger)
	{
		scriptToFill.m_pDebugger->SetContext(pCtx);
		if (scriptToFill.m_pDebugger->GetIsDebugging())
		{
			scriptToFill.m_pDebugger->ClearLineCallback();
		}
	}
	else
	{
		scriptToFill.m_pDebugger = new ScriptDebugger(pCtx, m_pDebuggerServer, m_pTypeRegistry);
	}

	if (!scriptToFill.m_bIsCapabilityScript)
	{
		// Call the init function on the script if it exists
		asIScriptModule* pScriptModule = m_pScriptEngine->GetModule(scriptName.Data());
		asIScriptFunction* pInitFunc = pScriptModule->GetFunctionByDecl("void Init()");
		if (pInitFunc)
		{
			// TODO: figure out how to get the script to be debuggable from the server. Maybe trigger reloads later.
			//if (m_pDebuggerServer)
			//{
			//	scriptToFill.m_pDebugger->SetLineCallback();

			//	m_pDebuggerServer->SetScriptToDebug(&scriptToFill);
			//}
			scriptToFill.m_pScriptContext->Prepare(pInitFunc);
			int r = scriptToFill.m_pScriptContext->Execute();
			if (r != asEXECUTION_FINISHED)
			{
				// The execution didn't complete as expected. Determine what happened.
				if (r == asEXECUTION_EXCEPTION)
				{
					// An exception occurred, let the script writer know what happened so it can be corrected.
					H_ERROR(StringL::Format("An exception '%s' occurred. Please correct the code and try again.", scriptToFill.m_pScriptContext->GetExceptionString()));
				}
			}
		}
	}

	return true;
}

bool Hail::AngelScript::Runner::CreateScriptFileModule(String64 scriptName, const FilePath& pathToScript, bool bReloadScriptModule)
{
	const char* pTemporaryScriptModuleName = "ReloadScript";
	const char* pModuleName = bReloadScriptModule ? pTemporaryScriptModuleName : scriptName.Data();

	if (CreateScriptModuleInternal(pModuleName, pathToScript))
	{
		asIScriptModule* pScriptModule = m_pScriptEngine->GetModule(pModuleName);

		asIScriptFunction* func = pScriptModule->GetFunctionByDecl("void main()");
		if (func == 0)
		{
			if (bReloadScriptModule)
			{
				m_pScriptEngine->DiscardModule(pTemporaryScriptModuleName);
			}
			else
			{
				m_pScriptEngine->DiscardModule(scriptName.Data());
			}
			// The function couldn't be found. Instruct the script writer to include the expected function in the script.
			H_ERROR("The script must have the function 'void main()'. Please add it and try again.");
			return false;
		}
		if (bReloadScriptModule)
		{
			m_pScriptEngine->DiscardModule(pTemporaryScriptModuleName);
		}
		return true;
	}
	return false;
}

bool Hail::AngelScript::Runner::CreateCapabilityModule(String64 capabilityName, const FilePath& pathToScript, bool bReloadCapabilityModule)
{
	const char* pTemporaryCapabilityModuleName = "ReloadingCapabilityModule";
	const char* pModuleName = bReloadCapabilityModule ? pTemporaryCapabilityModuleName : capabilityName.Data();

	if (CreateScriptModuleInternal(pModuleName, pathToScript))
	{
		asIScriptModule* pScriptModule = m_pScriptEngine->GetModule(pModuleName);

		asITypeInfo* pTypeInfo = pScriptModule->GetTypeInfoByDecl(capabilityName.Data());

		if (!pTypeInfo)
		{
			if (bReloadCapabilityModule)
			{
				m_pScriptEngine->DiscardModule(pTemporaryCapabilityModuleName);
			}
			else
			{
				m_pScriptEngine->DiscardModule(capabilityName.Data());
			}
			H_ERROR(StringL::Format("The Capability %s could not be found.", capabilityName.Data()));
			return false;
		}
		if (bReloadCapabilityModule)
		{
			m_pScriptEngine->DiscardModule(pTemporaryCapabilityModuleName);
		}
		return true;
	}
	return false;
}

bool Hail::AngelScript::Runner::CreateScriptModuleInternal(String64 scriptName, const FilePath& pathToScript)
{
	CScriptBuilder builder;
	int r = builder.StartNewModule(m_pScriptEngine, scriptName.Data());
	if (r < 0)
	{
		// If the code fails here it is usually because there
		// is no more memory to allocate the module
		H_ERROR("Unrecoverable error while starting a new module.");
		return false;
	}
	char filePathAsChar[1024];
	for (uint32 iInclude = 0; iInclude < m_mandatoryScriptIncludes.Size(); iInclude++)
	{
		memset(filePathAsChar, 0, 1024);
		const MandatoryScriptInclude& mandatoryInclude = m_mandatoryScriptIncludes[iInclude];
		const FilePath scriptsBaseIncludePath = FilePath::GetAngelscriptDirectory() + StringLW(mandatoryInclude.m_pRelativePath).Data();
		FromWCharToConstChar(scriptsBaseIncludePath.Data(), filePathAsChar, 1024);
		r = builder.AddSectionFromFile(filePathAsChar);
		if (r < 0)
		{
			H_ERROR(StringL::Format("Failed to load mandatory include: %s", mandatoryInclude.m_name));
			return false;
		}
	}
	memset(filePathAsChar, 0, 1024);
	FromWCharToConstChar(pathToScript.Data(), filePathAsChar, 1024);
	r = builder.AddSectionFromFile(filePathAsChar);
	if (r < 0)
	{
		// The builder wasn't able to load the file. Maybe the file has been removed, or the wrong name was given, or some preprocessing commands are incorrectly written.
		return false;
	}

	r = builder.BuildModule();
	if (r < 0)
	{
		// An error occurred. Instruct the script writer to fix the compilation errors that were listed in the output stream.
		return false;
	}

	return true;
}

bool Hail::AngelScript::Runner::ReloadScript(Script& scriptToReload)
{
	if (CreateScript(scriptToReload.m_name, scriptToReload))
	{
		H_DEBUGMESSAGE(StringL::Format("Reload succesful for script %s", scriptToReload.m_name));
	}

	if (scriptToReload.loadStatus == eScriptLoadStatus::FailedToReload)
	{
		scriptToReload.loadStatus = eScriptLoadStatus::FailedToReload;
		scriptToReload.m_reloadDelay = 0;
		scriptToReload.m_lastWriteTime = scriptToReload.m_filePath.GetCurrentLastWriteFileTime();
		H_ERROR(StringL::Format("Failed to reload script %s", scriptToReload.m_name));
	}

	return scriptToReload.loadStatus == eScriptLoadStatus::NoError;
}

