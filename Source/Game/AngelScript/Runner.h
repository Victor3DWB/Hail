#pragma once
#include "Utility\FilePath.hpp"
#include "Containers\GrowingArray\GrowingArray.h"
#include "DebuggerTypes.h"
#include "Script.h"

class asIScriptContext;
class asIScriptEngine;
class asIScriptModule;

class CScriptBuilder;

namespace Hail
{
	class FilePath;
	class Capability;
	class CapabilitySystem;
	class ComponentManager;
	struct CapabilityCreationRequest;
	struct AngelscriptCapabilityComponent;

	namespace AngelScript
	{
		class ScriptDebugger;
		class DebuggerServer;
		class TypeRegistry;

		class Runner
		{
		public:

			void ImportAndBuildScript(const FilePath& filePath, String64 scriptName);

			struct InitParams
			{
				asIScriptEngine* m_pScriptEngine;
				TypeRegistry* m_pTypeRegistry;
				CapabilitySystem* m_pCapabilitySystem;
				ComponentManager* m_pComponentManager;
			};

			void Init(const InitParams& initParams);

			void RunScript(String64 scriptName);
			// Will iterate over the loaded scripts and reload the ones that are out of date.
			void UpdateScriptReloadWatcher();
			// Checks all capability scripts for reloads, then updates the affected capability through the CapabilitySystem
			void UpdateCapabilityScriptReloadWatcher();

			void UpdateScriptDebugger();

			void Cleanup();

			void AddMandatoryScriptPath(const char* pSectionName, const char* pMandatoryRelativeScriptPath);
			bool SetScriptDefinedComponents(const FilePath& componentFilePath);

			DebuggerServer* GetDebuggerServer() { return m_pDebuggerServer; }

			Capability* CreateCapabilityFromScriptFile(const CapabilityCreationRequest& creationRequest);
			const FilePath& GetScriptFilePathForCapability(const String64& capabilityName);

		private:
			// Returns true if script is reloaded
			bool CheckForScriptReload(Script& scriptToCheckForReload);

			// Will return true if the creation is succesfull 
			bool CreateScript(String64 scriptName, Script& scriptToFill);
			// Creates an ordinary scriptfile module with a main function
			bool CreateScriptFileModule(String64 scriptName, const FilePath& pathToScript, bool bReloadScriptModule);
			bool CreateCapabilityModule(String64 capabilityName, const FilePath& pathToScript, bool bReloadCapabilityModule);
			bool CreateScriptModuleInternal(String64 scriptName, const FilePath& pathToScript);
			bool ReloadScript(Script& scriptToReload);

			void GenerateScriptModuleComponentFunctions(uint32 componentIdNumber, const char* pComponentName, CScriptBuilder& scriptBuilder);

			asIScriptEngine* m_pScriptEngine;
			DebuggerServer* m_pDebuggerServer;
			TypeRegistry* m_pTypeRegistry;
			CapabilitySystem* m_pCapabilitySystem;
			ComponentManager* m_pComponentManager;
			GrowingArray<Script> m_scripts;

			bool m_bScriptsHaveReloaded;

			struct MandatoryScriptInclude
			{
				String64 m_name;
				const char* m_pRelativePath;
			};

			GrowingArray<MandatoryScriptInclude> m_mandatoryScriptIncludes;
			struct ScriptComponentInformation
			{
				String64 m_name;
				uint32 m_id;
			};
			GrowingArray<ScriptComponentInformation>m_scriptComponentsToAdd;

			GrowingArray<Script> m_capabilityScripts;

			asIScriptModule* m_pScriptComponentsModule;
		};
	}
}