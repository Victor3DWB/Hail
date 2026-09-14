#pragma once
#include "Types.h"

#include "Containers\GrowingArray\GrowingArray.h"
#include "Utility\FilePath.hpp"

class asIScriptContext;

namespace Hail
{
	namespace AngelScript
	{
		class ScriptDebugger;
		class DebuggerServer;

		enum class eScriptLoadStatus
		{
			NoError,
			FailedToLoad,
			FailedToReload
		};

		enum class eScriptExecutionStatus
		{
			Normal,
			StoppedExecution,
			PausedExecution,
			StepIn,
			StepOver,
			StepOut
		};

		class Script
		{
		public:
			Script()
				: m_lastWriteTime(0)
				, m_pScriptContext(nullptr)
				, loadStatus(eScriptLoadStatus::FailedToLoad)
				, m_reloadDelay(0)
				, m_bIsDirty(false)
				, m_pDebugger(nullptr)
				, m_bIsCapabilityScript(false)
			{}
			Script(const FilePath& filePath, bool bIsCapabilityScript)
				: m_lastWriteTime(0)
				, m_pScriptContext(nullptr)
				, loadStatus(eScriptLoadStatus::FailedToLoad)
				, m_reloadDelay(0)
				, m_bIsDirty(false)
				, m_pDebugger(nullptr)
				, m_filePath(filePath)
				, m_bIsCapabilityScript(bIsCapabilityScript)
			{}

			Script& operator= (const Script& other)
			{
				m_filePath = other.m_filePath;
				m_lastWriteTime = other.m_lastWriteTime;
				m_pScriptContext = other.m_pScriptContext;
				m_name = other.m_name;
				m_nameBeforeReload = other.m_nameBeforeReload;
				loadStatus = other.loadStatus;
				m_reloadDelay = other.m_reloadDelay;
				m_bIsDirty = other.m_bIsDirty;
				m_bIsCapabilityScript = other.m_bIsCapabilityScript;
				m_pDebugger = other.m_pDebugger;
				m_fileNames = other.m_fileNames;
				return *this;
			}

			FilePath m_filePath;
			uint64 m_lastWriteTime;
			asIScriptContext* m_pScriptContext;
			String64 m_name;
			String64 m_nameBeforeReload;
			eScriptLoadStatus loadStatus;

			uint32 m_reloadDelay;
			bool m_bIsDirty;
			bool m_bIsCapabilityScript;
			
			ScriptDebugger* m_pDebugger;
			// TODO: Move to a debug class that is optional to have, so to minimize the runtime number of strings
			GrowingArray<StringL> m_fileNames; // files included in this script, used for debugging
		};
	}
}