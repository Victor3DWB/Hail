#pragma once
#include "Utility\FilePath.hpp"
#include "Containers\GrowingArray\GrowingArray.h"
#include "DebuggerTypes.h"
#include "Script.h"

class asIScriptEngine;
class asIScriptContext;

namespace Hail
{
	class FilePath;
	namespace AngelScript
	{
		class ScriptDebugger;
		class DebuggerServer;
		class TypeRegistry;

		class Runner
		{
		public:

			void ImportAndBuildScript(const FilePath& filePath, String64 scriptName);

			void Initialize(asIScriptEngine* pScriptEngine, TypeRegistry* pTypeRegistry);

			void RunScript(String64 scriptName);
			// Will iterate over the loaded scripts and reload the ones that are out of date.
			void Update();

			void Cleanup();

			void AddMandatoryScriptPath(const char* pSectionName, const char* pMandatoryRelativeScriptPath);

			DebuggerServer* GetDebuggerServer() { return m_pDebuggerServer; }

		private:
			// Will return true if the creation is succesfull 
			bool CreateScript(String64 scriptName, Script& scriptToFill);
			bool CreateScriptModule(String64 scriptName, const FilePath& pathToScript);
			bool ReloadScript(Script& scriptToReload);

			asIScriptEngine* m_pScriptEngine;
			DebuggerServer* m_pDebuggerServer;
			TypeRegistry* m_pTypeRegistry;
			GrowingArray<Script> m_scripts;

			struct MandatoryScriptInclude
			{
				String64 m_name;
				const char* m_pRelativePath;
			};

			GrowingArray<MandatoryScriptInclude> m_mandatoryScriptIncludes;
		};
	}
}