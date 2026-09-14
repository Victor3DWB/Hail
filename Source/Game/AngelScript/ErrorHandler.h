#pragma once	

class asIScriptEngine;
struct asSMessageInfo;

namespace Hail
{
	namespace AngelScript
	{
		class Runner;
		class ErrorHandler
		{
		public:
			void Init(asIScriptEngine* pScriptEngine, bool bEnableDebugger);
			void MessageCallback(const asSMessageInfo* pMsg, void* param);
			void SetActiveScriptRunner(Runner* pRunner);

		private:
			Runner* m_pRunner;
			bool m_bEnableDebugger;
		};
	}


}