#include "Types.h"
#include "Containers\VectorOnStack\VectorOnStack.h"
#include "Containers\StaticArray\StaticArray.h"

#include "Script.h"
#include "DebuggerMessagePackager.h"
class asIScriptEngine;
class asIScriptContext;
class asITypeInfo;

namespace Hail
{
	constexpr uint32 MAX_ATTACHED_DEBUGGERS = 4u;
#ifdef _WIN32 
	typedef unsigned int H_Socket;
#else
	typedef int H_Socket;
#endif

	namespace AngelScript
	{
		class DebuggerServer;
		class TypeDebuggerRegistry;
		class TypeRegistry;

		Variable ASTypeToVariable(void* value, uint32 typeId, int expandMembers, asIScriptEngine* engine, TypeRegistry* pTypeRegistry);
		StringL RemoveTypeInformationFromDeclaration(const char* stringToPrune);

		class ScriptDebugger
		{
		public:

			ScriptDebugger();
			ScriptDebugger(asIScriptContext* pContext, DebuggerServer* pDebugServer, TypeRegistry* pTypeRegistry);
			void SetContext(asIScriptContext* pContext);

			// Line callback invoked by context
			void LineCallback(asIScriptContext* pContext);
			void SetLineCallback();
			void ClearLineCallback();

			void StopDebuggingScript();
			void AddBreakpoints(const FileBreakPoints& breakPoints);
			void RemoveBreakpoints(const FileBreakPoints& breakPoints);
			void RemoveBreakpoints();
			void SetExecutionStatus(eScriptExecutionStatus status);
			bool GetIsDebugging() const { return m_bIsDebugging; }
			eScriptExecutionStatus GetStatus() const { return m_executionStatus; }

			GrowingArray<DebuggerMessage>& GetMessages() { return m_messages; }
			void SendGeneratedCallstack();
			void SendErrorMessage(const char* section, int row, const char* message);
			void SendWarningMessage(const char* section, int row, const char* message);
			void SendVariables(eCallStack callStackToSend);
			void SendVariable(eCallStack callStackToSend, StringL variableRequested);

			TypeRegistry* GetTypeRegistry() { return m_pTypeRegistry; }

		private:

			friend class DebuggerServer;

			void CreateCallstack(asIScriptContext* pContext, const char* pFileName);
			void CreateVariables(asIScriptContext* pContext);

			bool m_bIsDebugging;
			asIScriptContext* m_pScriptContext;
			// TODO add these breakpoints to a hashmap
			GrowingArray<FileBreakPoints> m_breakPoints;
			eScriptExecutionStatus m_executionStatus;
			int m_currentLine;

			GrowingArray<DebuggerMessage> m_messages;
			bool m_bGeneratedStackData;
			GrowingArray<StackFrame> m_callStack;
			bool m_bGeneratedVariables;
			StaticArray<GrowingArray<Variable>, (uint32)eCallStack::count> m_variables;

			// TODO: make a map to add this too
			GrowingArray<asITypeInfo*> m_registeredObjects;
			DebuggerServer* m_pDebuggerServer;
			TypeRegistry* m_pTypeRegistry;

			struct Client
			{
				bool m_bConnectedForDebugging;
				bool m_bDisconnected;
				H_Socket m_socket;
			};
			Client m_clientData;
		};

		class DebuggerServer
		{
		public:

			explicit DebuggerServer(TypeDebuggerRegistry* pTypeDebuggerRegistry);
			~DebuggerServer();

			void Update(bool bAreScriptsReloading);

			// This is the while loop that holds the exectuion of a script while debugging.
			void UpdateDuringScriptExecution();

			void SetScriptToDebug(Script* pScript) { m_pActiveScript = pScript; }
			Script* GetActiveScript() const { return m_pActiveScript; }

			void StartDebugging();
			void StopDebugging();
			void AddBreakpoints(const FileBreakPoints& breakpointsToAdd);
			void SendVariables(eCallStack callStackType);
			void FindVariable(eCallStack callStackType, StringL variableToFind);
			void ContinueDebugging();
			void PauseDebugging();
			void StepIn();
			void StepOver();
			void StepOut();
			void SendCallstack();
			void RequestBuildErrors(MessageHeader header);
			void RequestEngineTypes(MessageHeader header);
			void AddBuildError(const BuildErrorInfo& buildError);
			
			void AddMandatoryIncludePath(const char* pMandatoryRelativeScriptPath);

		private:

			void SendDebuggerMessage(DebuggerMessage& messageToSend);
			void ListenToMessages();

			VectorOnStack<ScriptDebugger, MAX_ATTACHED_DEBUGGERS> m_clients;
			Script* m_pActiveScript;
			H_Socket m_socketHandle;
			uint32 m_currentClient;
			bool m_bIsDebugging;
			TypeDebuggerRegistry* m_pTypeDebuggerRegistry;

			GrowingArray<MessageHeader> m_returnRequests;
			GrowingArray<BuildErrorInfo> m_registeredBuildErrors;

			// Relative paths based from the AngelScript base directory, reconstructs the entire path on engine requests.
			GrowingArray<const char*> m_mandatoryEngineIncludes;
		};

	}
}


// Keeping the Official AS debugger reference code commented out for future reading and reference
//#ifndef DEBUGGER_H
//#define DEBUGGER_H
//
//#ifndef ANGELSCRIPT_H 
//// Avoid having to inform include path if header is already include before
//#include <angelscript.h>
//#endif
//
//#include <string>
//#include <vector>
//#include <map>
//
//BEGIN_AS_NAMESPACE
//
//class CDebugger
//{
//public:
//	CDebugger();
//	virtual ~CDebugger();
//
//	// Register callbacks to handle to-string conversions of application types
//	// The expandMembersLevel is a counter for how many recursive levels the members should be expanded.
//	// If the object that is being converted to a string has members of its own the callback should call
//	// the debugger's ToString passing in expandMembersLevel - 1.
//	typedef std::string (*ToStringCallback)(void *obj, int expandMembersLevel, CDebugger *dbg);
//	virtual void RegisterToStringCallback(const asITypeInfo *ti, ToStringCallback callback);
//
//	// User interaction
//	virtual void TakeCommands(asIScriptContext *ctx);
//	virtual void Output(const std::string &str);
//
//	// Line callback invoked by context
//	virtual void LineCallback(asIScriptContext *ctx);
//
//	// Commands
//	virtual void PrintHelp();
//	virtual void AddFileBreakPoint(const std::string &file, int lineNbr);
//	virtual void AddFuncBreakPoint(const std::string &func);
//	virtual void ListBreakPoints();
//	virtual void ListLocalVariables(asIScriptContext *ctx);
//	virtual void ListGlobalVariables(asIScriptContext *ctx);
//	virtual void ListMemberProperties(asIScriptContext *ctx);
//	virtual void ListStatistics(asIScriptContext *ctx);
//	virtual void PrintCallstack(asIScriptContext *ctx);
//	virtual void PrintValue(const std::string &expr, asIScriptContext *ctx);
//
//	// Helpers
//	virtual bool InterpretCommand(const std::string &cmd, asIScriptContext *ctx);
//	virtual bool CheckBreakPoint(asIScriptContext *ctx);
//	virtual std::string ToString(void *value, asUINT typeId, int expandMembersLevel, asIScriptEngine *engine);
//
//	// Optionally set the engine pointer in the debugger so it can be retrieved
//	// by callbacks that need it. This will hold a reference to the engine.
//	virtual void SetEngine(asIScriptEngine *engine);
//	virtual asIScriptEngine *GetEngine();
//	
//protected:
//	enum DebugAction
//	{
//		CONTINUE,  // continue until next break point
//		STEP_INTO, // stop at next instruction
//		STEP_OVER, // stop at next instruction, skipping called functions
//		STEP_OUT   // run until returning from current function
//	};
//	DebugAction        m_action;
//	asUINT             m_lastCommandAtStackLevel;
//	asIScriptFunction *m_lastFunction;
//
//	struct BreakPoint
//	{
//		BreakPoint(std::string f, int n, bool _func) : name(f), lineNbr(n), func(_func), needsAdjusting(true) {}
//		std::string name;
//		int         lineNbr;
//		bool        func;
//		bool        needsAdjusting;
//	};
//	std::vector<BreakPoint> m_breakPoints;
//
//	asIScriptEngine *m_engine;
//
//	// Registered callbacks for converting types to strings
//	std::map<const asITypeInfo*, ToStringCallback> m_toStringCallbacks;
//};
//
//END_AS_NAMESPACE
//
//#endif