#include "Engine_PCH.h"
#include "Debugger.h"
#include "DebuggerMessagePackager.h"
#include <angelscript.h>

#include "TypeRegistry.h"
#include "Handler.h"

#include "HailEngine.h"

#include <fcntl.h>

#ifdef _WIN32
/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */
#endif

#include "MathUtils.h"

namespace Hail
{
    StringL RemoveTypeInformationFromDeclaration(const char* stringToPrune)
    {
        int32 firstSpaceIndex = StringUtility::FindFirstOfSymbol(stringToPrune, ' ');
        if (firstSpaceIndex < 0)
        {
            H_ERROR(StringL::Format("Invalid declaration: %s", stringToPrune));
            return StringL{ "InvalidType" };
        }
        return (stringToPrune + firstSpaceIndex + 1);
    }

    namespace AngelScript
    {
        Variable ASTypeToVariable(void* value, uint32 typeId, int expandMembers, asIScriptEngine* engine, TypeRegistry* pTypeRegistry)
        {
            Variable returnVariable;
            if (value == 0)
            {
                returnVariable.m_value = "null";
            }

            if (typeId == asTYPEID_VOID)
            {
                returnVariable.m_type = "void";
            }
            else if (typeId == asTYPEID_BOOL)
            {
                returnVariable.m_type = "bool";
                returnVariable.m_value = (*(bool*)value) ? StringL("true") : StringL("false");
            }
            else if (typeId == asTYPEID_INT8)
            {
                returnVariable.m_type = "int8";
                returnVariable.m_value = StringL::Format("%i", *(int8*)value);
            }
            else if (typeId == asTYPEID_INT16)
            {
                returnVariable.m_type = "int16";
                returnVariable.m_value = StringL::Format("%i", *(int16*)value);
            }
            else if (typeId == asTYPEID_INT32)
            {
                returnVariable.m_type = "int32";
                int32 intValue = *(int32*)value;
                returnVariable.m_value = StringL::Format("%i", *(int*)value);
            }
            else if (typeId == asTYPEID_INT64)
            {
                returnVariable.m_type = "int64";
                returnVariable.m_value = StringL::Format("%i", *(int64*)value);
            }
            else if (typeId == asTYPEID_UINT8)
            {
                returnVariable.m_type = "uint8";
                returnVariable.m_value = StringL::Format("%u", *(uint8*)value);
            }
            else if (typeId == asTYPEID_UINT16)
            {
                returnVariable.m_type = "uint16";
                returnVariable.m_value = StringL::Format("%u", *(uint16*)value);
            }
            else if (typeId == asTYPEID_UINT32)
            {
                returnVariable.m_type = "uint32";
                returnVariable.m_value = StringL::Format("%u", *(uint32*)value);
            }
            else if (typeId == asTYPEID_UINT64)
            {
                returnVariable.m_type = "uint64";
                returnVariable.m_value = StringL::Format("%u", *(uint64*)value);
            }
            else if (typeId == asTYPEID_FLOAT)
            {
                returnVariable.m_type = "float";
                float actualValue = *(float*)value;
                returnVariable.m_value = StringL::Format("%f", *(float*)value);
            }
            else if (typeId == asTYPEID_DOUBLE)
            {
                returnVariable.m_type = "double";
                returnVariable.m_value = StringL::Format("%d", *(double*)value);
            }
            else if ((typeId & asTYPEID_MASK_OBJECT) == 0)
            {
                // The type is an enum
                returnVariable.m_value = StringL::Format("%u", *(asUINT*)value);
                returnVariable.m_type = "enum";
                // Check if the value matches one of the defined enums
                if (engine)
                {
                    asITypeInfo* t = engine->GetTypeInfoById(typeId);
                    for (int n = t->GetEnumValueCount(); n-- > 0; )
                    {
                        int enumVal;
                        const char* enumName = t->GetEnumValueByIndex(n, &enumVal);
                        if (enumVal == *(int*)value)
                        {
                            returnVariable.m_name = enumName;
                            break;
                        }
                    }
                }
            }
            else if (typeId & asTYPEID_SCRIPTOBJECT)
            {
                // Dereference handles, so we can see what it points to
                if (typeId & asTYPEID_OBJHANDLE)
                    value = *(void**)value;

                asIScriptObject* obj = (asIScriptObject*)value;

                // Print the address of the object
                //s += StringL::Format("{%u}", obj);

                // Print the members
                if (obj && expandMembers > 0)
                {
                    asITypeInfo* type = obj->GetObjectType();
                    for (asUINT n = 0; n < obj->GetPropertyCount(); n++)
                    {
                        Variable& member = returnVariable.m_members.Add();
                        member = ASTypeToVariable(obj->GetAddressOfProperty(n), obj->GetPropertyTypeId(n), expandMembers - 1, type->GetEngine(), pTypeRegistry);
                        member.m_name = RemoveTypeInformationFromDeclaration(type->GetPropertyDeclaration(n));
                    }
                }
            }
            else
            {
                // Dereference handles, so we can see what it points to
                if (typeId & asTYPEID_OBJHANDLE)
                    value = *(void**)value;

                // Print the address for reference types so it will be
                // possible to see when handles point to the same object
                if (engine && value)
                {
                    returnVariable = pTypeRegistry->GetVariableFromCallback(typeId, value);
                }
            }
            return returnVariable;
        }
    }
}

using namespace Hail;
using namespace AngelScript;

namespace
{
#ifndef _WIN32 
    constexpr int InvalidSocket = -1;
#else
    constexpr unsigned int InvalidSocket = INVALID_SOCKET;
#endif

    int SockInit(void)
    {
#ifdef _WIN32
        WSADATA wsa_data;
        return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
        return 0;
#endif
    }

    int SockQuit(void)
    {
#ifdef _WIN32
        return WSACleanup();
#else
        return 0;
#endif
    }

    int SocketClose(Hail::H_Socket socketHandle)
    {

        int status = 0;

#ifdef _WIN32
        status = shutdown(socketHandle, SD_SEND);
        if (status == SOCKET_ERROR)
        { 
            status = closesocket(socketHandle); 
        }
#else
        status = shutdown(socketHandle, SHUT_RDWR);
        if (status == 0) 
        { 
            status = close(socketHandle); 
        }
#endif

        return status;
    }

    Hail::H_Socket CreateSocket()
    {
#define DEFAULT_PORT "27015"
        Hail::H_Socket returnSocket = 0;

#ifdef _WIN32

        struct addrinfo* result = NULL, hints;

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        // Resolve the local address and port to be used by the server
        int iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
        if (iResult != 0) {
            printf("getaddrinfo failed: %d\n", iResult);
            WSACleanup();
            return 1;
        }

        //sockaddr_in serverAddress;
        //serverAddress.sin_family = AF_INET;
        //serverAddress.sin_port = htons(8080);
        //serverAddress.sin_addr.s_addr = INADDR_ANY;

        returnSocket = socket(AF_INET, SOCK_STREAM, 0);
        H_ASSERT(returnSocket != InvalidSocket, "Failed to create server socket.");

        const int bindResult = bind(returnSocket, result->ai_addr, (int)result->ai_addrlen);
        H_ASSERT(bindResult != -1, "Failed to bind server socket");
      
        unsigned long iMode = 1;
        iResult = ioctlsocket(returnSocket, FIONBIO, &iMode);
        H_ASSERT(iResult == NO_ERROR, "ioctlsocket failed with error.");
#else
        // TODO: POSIX sockets

#endif

        return returnSocket;
    }
}


DebuggerServer::DebuggerServer(TypeDebuggerRegistry* pTypeDebuggerRegistry)
: m_socketHandle(InvalidSocket)
, m_currentClient(MAX_UINT)
, m_bIsDebugging(false)
, m_pActiveScript(nullptr)
, m_pTypeDebuggerRegistry(pTypeDebuggerRegistry)
{
    // Initialize socket use
    H_ASSERT(SockInit() == 0, "Failed to initialize the socket.");
    m_socketHandle = CreateSocket();
    // listening to the assigned socket 
    int iListenResult = listen(m_socketHandle, 4096);
    H_ASSERT(iListenResult == NO_ERROR, "Failed with listen.");
}

DebuggerServer::~DebuggerServer()
{
    SendDebuggerMessage(CreateStopDebugSessionMessage());

    H_ASSERT(SocketClose(m_socketHandle) == 0, "Failed to close socket properly");
    //Close Winsock / socket use.
    SockQuit();
}

Hail::AngelScript::ScriptDebugger::ScriptDebugger()
    : m_bIsDebugging(false)
    , m_pScriptContext(nullptr)
    , m_executionStatus(eScriptExecutionStatus::Normal)
    , m_bGeneratedStackData(false)
    , m_bGeneratedVariables(false)
    , m_currentLine(0)
    , m_pDebuggerServer(nullptr)
    , m_pTypeRegistry(nullptr)
{
    m_clientData.m_socket = InvalidSocket;
    m_clientData.m_bConnectedForDebugging = false;
    m_clientData.m_bDisconnected = true;
}

Hail::AngelScript::ScriptDebugger::ScriptDebugger(asIScriptContext* pContext, DebuggerServer* pDebugServer, TypeRegistry* pTypeRegistry)
    : m_bIsDebugging(false)
    , m_pScriptContext(pContext)
    , m_executionStatus(eScriptExecutionStatus::Normal)
    , m_bGeneratedStackData(false)
    , m_bGeneratedVariables(false)
    , m_currentLine(0)
    , m_pDebuggerServer(pDebugServer)
    , m_pTypeRegistry(pTypeRegistry)
{
    m_clientData.m_socket = InvalidSocket;
    m_clientData.m_bConnectedForDebugging = false;
    m_clientData.m_bDisconnected = true;
}

void Hail::AngelScript::ScriptDebugger::SetContext(asIScriptContext* pContext)
{
    m_pScriptContext = pContext;
}

void ScriptDebugger::LineCallback(asIScriptContext* pContext)
{
    if (!m_bIsDebugging)
        return;

    const char *file = 0;
	int lineNbr = pContext->GetLineNumber(0, 0, &file);

    if (m_executionStatus == eScriptExecutionStatus::StoppedExecution || lineNbr == m_currentLine)
        return;

    m_currentLine = lineNbr;
    const FilePath scriptObjectPath = file;
    FileBreakPoints* pFileBreakPoints = nullptr;
    for (uint16 i = 0; i < m_breakPoints.Size(); i++)
    {
        if (StringCompareCaseInsensitive(m_breakPoints[i].fileName.Data(), scriptObjectPath.Object().Name().CharString()))
        {
            pFileBreakPoints = &m_breakPoints[i];
            break;
        }
    }

    bool bHitBreakpoint = false;
    if (pFileBreakPoints)
    {
        for (size_t i = 0; i < pFileBreakPoints->breakPoints.Size(); i++)
        {
            if (pFileBreakPoints->breakPoints[i].line == lineNbr)
            {
                H_DEBUGMESSAGE("Hit breakpoint in file");
                m_executionStatus = eScriptExecutionStatus::StoppedExecution;
                bHitBreakpoint = true;
                break;
            }
        }
    }

    if (m_executionStatus == eScriptExecutionStatus::Normal)
        return;

    // If we have just stepped, stop on the next callback
    if (m_executionStatus == eScriptExecutionStatus::StepIn || m_executionStatus == eScriptExecutionStatus::PausedExecution)
    {
        m_executionStatus = eScriptExecutionStatus::StoppedExecution;
    }
    // if stepping over, we need to check the callstack size to see if we actually stop
    if (m_executionStatus == eScriptExecutionStatus::StepOver)
    {
        if (pContext->GetCallstackSize() <= m_callStack.Size())
        {
            m_executionStatus = eScriptExecutionStatus::StoppedExecution;
        }
    }
    if (m_executionStatus == eScriptExecutionStatus::StepOut)
    {
        if (pContext->GetCallstackSize() == 1 || pContext->GetCallstackSize() < m_callStack.Size())
        {
            m_executionStatus = eScriptExecutionStatus::StoppedExecution;
        }
    }


    if (m_executionStatus == eScriptExecutionStatus::StoppedExecution)
    {
        m_bGeneratedStackData = false;
        CreateCallstack(pContext, file);
        m_bGeneratedVariables = false;
        CreateVariables(pContext);

        if (bHitBreakpoint)
        {
            StringLW fileName = scriptObjectPath.Data();
            m_messages.Add(CreateHitBreakpointMessage(lineNbr, fileName.ToCharString()));
            H_DEBUGMESSAGE("Sending hit breakpoint message.");
        }
        else
        {
            m_messages.Add(CreateStopExecutionMessage());
            H_DEBUGMESSAGE("Sending stop execution message.");
        }

        pContext->Suspend();
    }

    // Will halt execution
    m_pDebuggerServer->UpdateDuringScriptExecution();
}

void Hail::AngelScript::ScriptDebugger::SetLineCallback()
{
    if (m_bIsDebugging)
    {
        H_ERROR("Debugger is already set for debugging, potential code error.");
        return;
    }
    Hail::AngelScript::ScriptDebugger* pDebugger = this;
    // Attaching line-callback for angelscript.
    m_pScriptContext->SetLineCallback(asMETHOD(ScriptDebugger, LineCallback), pDebugger, asCALL_THISCALL);
    m_bIsDebugging = true;
}

void Hail::AngelScript::ScriptDebugger::ClearLineCallback()
{
    if (!m_bIsDebugging)
    {
        H_ERROR("Debugger is not set for debugging, potential code error.");
        return;
    }
    m_pScriptContext->ClearLineCallback();
    m_bIsDebugging = false;
}

void Hail::AngelScript::ScriptDebugger::StopDebuggingScript()
{
    if (!m_bIsDebugging)
    {
        H_ERROR("Debugger is not set for debugging, potential code error.");
    }
    if (m_executionStatus != eScriptExecutionStatus::Normal)
    {
        SetExecutionStatus(eScriptExecutionStatus::Normal);
    }

    ClearLineCallback();
    m_registeredObjects.RemoveAll();
    m_breakPoints.RemoveAll();
    m_bGeneratedVariables = false;
    m_bGeneratedStackData = false;

    H_DEBUGMESSAGE("Stop debugging");
}

void Hail::AngelScript::ScriptDebugger::AddBreakpoints(const FileBreakPoints& breakPoints)
{
    for (int i = m_breakPoints.Size(); i > 0; i--)
    {
        if (StringCompareCaseInsensitive(breakPoints.fileName, m_breakPoints[i - 1].fileName))
        {
            m_breakPoints.RemoveCyclicAtIndex(i - 1);
            break;
        }
    }

    m_breakPoints.Add(breakPoints);
}

void Hail::AngelScript::ScriptDebugger::RemoveBreakpoints(const FileBreakPoints& breakPoints)
{
    for (int i = m_breakPoints.Size(); i > 0; i--)
    {
        if (StringCompareCaseInsensitive(breakPoints.fileName, m_breakPoints[i - 1].fileName))
        {
            m_breakPoints.RemoveCyclicAtIndex(i - 1);
            break;
        }
    }
}

void Hail::AngelScript::ScriptDebugger::RemoveBreakpoints()
{
    m_breakPoints.RemoveAll();
    H_DEBUGMESSAGE("Remove breakpoints");
}

void Hail::AngelScript::ScriptDebugger::SetExecutionStatus(eScriptExecutionStatus status)
{
    if (status == m_executionStatus)
    {
        H_ERROR("Possible programming error, sending same status as already active on debugger");
        return;
    }
    m_bGeneratedStackData = false;
    m_bGeneratedVariables = false;
    m_executionStatus = status;
}

void Hail::AngelScript::ScriptDebugger::SendGeneratedCallstack()
{
    if (!m_bGeneratedStackData)
    {
        // no existing callstack, send error
        return;
    }

    m_messages.Add(CreateCallstackMessage(m_callStack));
    H_DEBUGMESSAGE("Sending callstack.");
}

void Hail::AngelScript::ScriptDebugger::SendVariables(eCallStack callStackToSend)
{

    if (!m_bGeneratedVariables)
    {
        // Send empty message
        m_messages.Add(CreateVariablesMessage(nullptr));
    }
    else
    {
        m_messages.Add(CreateVariablesMessage(&m_variables[(uint32)callStackToSend]));
    }
    H_DEBUGMESSAGE("Sending variables message.");
}

void Hail::AngelScript::ScriptDebugger::SendVariable(eCallStack callStackToSend, StringL variableRequested)
{
    const Variable* pVarToSend = nullptr;
    if (m_bGeneratedVariables)
    {
        for (size_t i = 0; i < m_variables[(uint32)callStackToSend].Size(); i++)
        {
            const Variable& var = m_variables[(uint32)callStackToSend][i];
            if (StringCompare(var.m_name, variableRequested))
            {
                pVarToSend = &var;
                break;
            }
        }
        if (!pVarToSend)
        {
            // Also search local scope
            for (size_t i = 0; i < m_variables[(uint32)eCallStack::local].Size(); i++)
            {
                const Variable& var = m_variables[(uint32)eCallStack::local][i];
                if (StringCompare(var.m_name, variableRequested))
                {
                    pVarToSend = &var;
                    break;
                }
            }
        }
    }
    H_DEBUGMESSAGE("Sending specific variable message.");
    m_messages.Add(CreateVariableMessage(pVarToSend));
}

void Hail::AngelScript::ScriptDebugger::CreateCallstack(asIScriptContext* pContext, const char* pFileName)
{
    if (m_bGeneratedStackData)
        return;

    m_callStack.RemoveAll();
    // Create Callstack
    if (!m_bGeneratedStackData)
    {
        for (asUINT n = 0; n < pContext->GetCallstackSize(); n++)
        {
            int32 stackLineNbr = 0;
            stackLineNbr = pContext->GetLineNumber(n, 0, &pFileName);
            StackFrame& frame = m_callStack.Add();
            frame.m_line = stackLineNbr;
            frame.m_functionName = pContext->GetFunction(n)->GetDeclaration();
            frame.m_sourceFile = pFileName;
        }
        m_bGeneratedStackData = true;
    }
}

void Hail::AngelScript::ScriptDebugger::CreateVariables(asIScriptContext* pContext)
{
    if (m_bGeneratedVariables)
        return;
    // Tokenize the input string to get the variable scope and name
    //asUINT len = 0;
    //string scope;
    //StringL name;
    //StringL str = expr;
    //asETokenClass t = engine->ParseToken(str.c_str(), 0, &len);
    //while( t == asTC_IDENTIFIER || (t == asTC_KEYWORD && len == 2 && str.compare(0, 2, "::") == 0) )
    //{
    //	if( t == asTC_KEYWORD )
    //	{
    //		if( scope == "" && name == "" )
    //			scope = "::";			// global scope
    //		else if( scope == "::" || scope == "" )
    //			scope = name;			// namespace
    //		else
    //			scope += "::" + name;	// nested namespace
    //		name = "";
    //	}
    //	else if( t == asTC_IDENTIFIER )
    //		name.assign(str.c_str(), len);

    //	// Skip the parsed token and get the next one
    //	str = str.substr(len);
    //	t = engine->ParseToken(str.c_str(), 0, &len);
    //}

    asIScriptModule* mod = pContext->GetFunction()->GetModule();
    if (!mod) return;
    //if( name.size() )
    //{
    //	// Find the variable
    //void *ptr = 0;
    //int typeId = 0;

    asIScriptFunction* localFunc = pContext->GetFunction();
    if (!localFunc) return;

    //	// skip local variables if a scope was informed
    //	if( scope == "" )
    //	{
    //		// We start from the end, in case the same name is reused in different scopes
    //		for( asUINT n = func->GetVarCount(); n-- > 0; )
    //		{
    //			const char* varName = 0;
    //			ctx->GetVar(n, 0, &varName, &typeId);
    //			if( ctx->IsVarInScope(n) && varName != 0 && name == varName )
    //			{
    //				ptr = ctx->GetAddressOfVar(n);
    //				break;
    //			}
    //		}

    // Look for class members, if we're in a class method
    //if (!ptr && localFunc->GetObjectType())
    {
        //if (name == "this")
        //{
        //    ptr = pContext->GetThisPointer();
        //    typeId = pContext->GetThisTypeId();
        //}
        //else
        //{
            //asITypeInfo* type = engine->GetTypeInfoById(pContext->GetThisTypeId());
            //for (asUINT n = 0; n < type->GetPropertyCount(); n++)
            //{
            //    const char* propName = 0;
            //    int offset = 0;
            //    bool isReference = 0;
            //    int compositeOffset = 0;
            //    bool isCompositeIndirect = false;
            //    type->GetProperty(n, &propName, &typeId, 0, 0, &offset, &isReference, 0, &compositeOffset, &isCompositeIndirect);
            //    if (name == propName)
            //    {
            //        ptr = (void*)(((asBYTE*)pContext->GetThisPointer()) + compositeOffset);
            //        if (isCompositeIndirect) ptr = *(void**)ptr;
            //        ptr = (void*)(((asBYTE*)ptr) + offset);
            //        if (isReference) ptr = *(void**)ptr;
            //        break;
            //    }
            //}
        //}
    }
    //	}

    //	// Look for global variables
    //	if( !ptr )
    //	{
    //		if( scope == "" )
    //		{
    //			// If no explicit scope was informed then use the namespace of the current function by default
    //			scope = func->GetNamespace();
    //		}
    //		else if( scope == "::" )
    //		{
    //			// The global namespace will be empty
    //			scope = "";
    //		}

    //		asIScriptModule *mod = func->GetModule();
    //		if( mod )
    //		{
    //			for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ )
    //			{
    //				const char *varName = 0, *nameSpace = 0;
    //				mod->GetGlobalVar(n, &varName, &nameSpace, &typeId);

    //				// Check if both name and namespace match
    //				if( name == varName && scope == nameSpace )
    //				{
    //					ptr = mod->GetAddressOfGlobalVar(n);
    //					break;
    //				}
    //			}
    //		}
    //	}

    //	if( ptr )
    //	{
    //		// TODO: If there is a . after the identifier, check for members
    //		// TODO: If there is a [ after the identifier try to call the 'opIndex(expr) const' method 
    //		if( str != "" )
    //		{
    //			Output("Invalid expression. Expression doesn't end after symbol\n");
    //		}
    //		else
    //		{
    //			stringstream s;
    //			// TODO: Allow user to set if members should be expanded
    //			// Expand members by default to 3 recursive levels only
    //			s << ToString(ptr, typeId, 3, engine) << endl;
    //			Output(s.str());
    //		}
    //	}
    //	else
    //	{
    //		Output("Invalid expression. No matching symbol\n");
    //	}
    //}
    //else
    //{
    //	Output("Invalid expression. Expected identifier\n");
    //}
    //ListMemberProperties
    m_variables[(int)eCallStack::self].RemoveAll();
    void* ptr = pContext->GetThisPointer();
    if (ptr)
    {
        // TODO: Allow user to define if members should be expanded or not
        // Expand members by default to 3 recursive levels only
        Variable& thisVariable = m_variables[(int)eCallStack::self].Add();
        thisVariable = ASTypeToVariable(ptr, pContext->GetThisTypeId(), 3, pContext->GetEngine(), GetTypeRegistry());
        thisVariable.m_name = "this";
    }

    //}

    //ListGlobalVariables

    m_variables[(int)eCallStack::global].RemoveAll();
    for (asUINT n = 0; n < mod->GetGlobalVarCount(); n++)
    {
        int typeId = 0;
        mod->GetGlobalVar(n, 0, 0, &typeId);
        // TODO: Allow user to set how many recursive expansions should be done
        // Expand members by default to 3 recursive levels only
        Variable& globalVariable = m_variables[(int)eCallStack::global].Add();
        globalVariable = ASTypeToVariable(mod->GetAddressOfGlobalVar(n), typeId, 3, pContext->GetEngine(), GetTypeRegistry());
        globalVariable.m_name = RemoveTypeInformationFromDeclaration(mod->GetGlobalVarDeclaration(n));

    }

    //ListLocalVariables
    m_variables[(int)eCallStack::local].RemoveAll();
    if (localFunc)
    {
        for (asUINT n = 0; n < localFunc->GetVarCount(); n++)
        {
            // Skip temporary variables
            // TODO: Should there be an option to view temporary variables too?
            const char* name;
            localFunc->GetVar(n, &name);
            if (name == 0 || StringLength(name) == 0)
                continue;

            if (pContext->IsVarInScope(n))
            {
                // TODO: Allow user to set if members should be expanded or not
                // Expand members by default to 3 recursive levels only
                int typeId;
                pContext->GetVar(n, 0, 0, &typeId);
                Variable& localVariable = m_variables[(int)eCallStack::local].Add();
                localVariable = ASTypeToVariable(pContext->GetAddressOfVar(n), typeId, 3, pContext->GetEngine(), GetTypeRegistry());
                localVariable.m_name = RemoveTypeInformationFromDeclaration(localFunc->GetVarDecl(n));
            }
        }
    }

    m_bGeneratedVariables = true;
}

void Hail::AngelScript::DebuggerServer::Update(bool bAreScriptsReloading)
{
    {
        // Look for new connections
        const H_Socket connectingSocket = accept(m_socketHandle, nullptr, nullptr);
        if (connectingSocket != InvalidSocket)
        {
            ScriptDebugger& newClient = m_clients.Add();
            newClient.m_clientData.m_socket = connectingSocket;
            newClient.m_clientData.m_bConnectedForDebugging = false;
            newClient.m_clientData.m_bDisconnected = false;
        }
    }

    ListenToMessages();
    if (m_bIsDebugging && m_pActiveScript)
    {
        GrowingArray<DebuggerMessage>& debuggerMessages = m_pActiveScript->m_pDebugger->GetMessages();
        for (size_t iDebugMessage = 0; iDebugMessage < debuggerMessages.Size(); iDebugMessage++)
        {
            SendDebuggerMessage(debuggerMessages[iDebugMessage]);
        }
        debuggerMessages.RemoveAll();
    }
    if (!bAreScriptsReloading && !m_returnRequests.Empty())
    {

        for (size_t iMessageRequest = 0; iMessageRequest < m_returnRequests.Size(); iMessageRequest++)
        {
            const MessageHeader& message = m_returnRequests[iMessageRequest];

            switch (message.type)
            {
            case Hail::AngelScript::eDebuggerMessageType::RequestBuildErrors:
                SendDebuggerMessage(CreateBuildErrorMessage(message, m_registeredBuildErrors));
                H_DEBUGMESSAGE("Sending build errors");
                break;
            case Hail::AngelScript::eDebuggerMessageType::RequestEngineTypes:
                SendDebuggerMessage(CreateEngineTypeResponseMessage(message, m_pTypeDebuggerRegistry));
                H_DEBUGMESSAGE("Sending engine registered types");
                break;
            case Hail::AngelScript::eDebuggerMessageType::Disconnect:
            case Hail::AngelScript::eDebuggerMessageType::StopExecution:
            case Hail::AngelScript::eDebuggerMessageType::HitBreakpoint:
            case Hail::AngelScript::eDebuggerMessageType::StartDebugSession:
            case Hail::AngelScript::eDebuggerMessageType::CreateBreakpoints:
            case Hail::AngelScript::eDebuggerMessageType::Paused:
            case Hail::AngelScript::eDebuggerMessageType::Stopped:
            case Hail::AngelScript::eDebuggerMessageType::Continued:
            case Hail::AngelScript::eDebuggerMessageType::CallStack:
            case Hail::AngelScript::eDebuggerMessageType::VariableRequest:
            case Hail::AngelScript::eDebuggerMessageType::EvaluateRequest:
            case Hail::AngelScript::eDebuggerMessageType::StepIn:
            case Hail::AngelScript::eDebuggerMessageType::StepOver:
            case Hail::AngelScript::eDebuggerMessageType::StepOut:
            case Hail::AngelScript::eDebuggerMessageType::End:
            default:
                H_ASSERT(false);
                break;
            }

        }
        m_returnRequests.RemoveAll();
        m_registeredBuildErrors.RemoveAll();
    }

}

void Hail::AngelScript::DebuggerServer::UpdateDuringScriptExecution()
{
    if (m_bIsDebugging && m_pActiveScript)
    {
        bool pausedRendering = false;

        if (m_pActiveScript->m_pDebugger->GetStatus() == eScriptExecutionStatus::StoppedExecution)
        {
            pausedRendering = true;
            SetSimulationMode(eEngineSimulationMode::Paused);
        }

        while (m_pActiveScript->m_pDebugger->GetStatus() == eScriptExecutionStatus::StoppedExecution && !IsApplicationTerminated())
        {
            ListenToMessages();

            GrowingArray<DebuggerMessage>& debuggerMessages = m_pActiveScript->m_pDebugger->GetMessages();
            for (size_t iDebugMessage = 0; iDebugMessage < debuggerMessages.Size(); iDebugMessage++)
            {
                SendDebuggerMessage(debuggerMessages[iDebugMessage]);
            }
            debuggerMessages.RemoveAll();
        }

        // Reset engine state
        if (pausedRendering)
        {
            SetSimulationMode(eEngineSimulationMode::Normal);
        }
    }
}

void Hail::AngelScript::DebuggerServer::StartDebugging()
{
    H_ASSERT(m_currentClient != MAX_UINT, "Must start debugging on a valid client.");
    ScriptDebugger& client = m_clients[m_currentClient];
    client.m_clientData.m_bConnectedForDebugging = true;
    H_DEBUGMESSAGE(StringL::Format("Client nr %d connected for debugging.", m_currentClient + 1));
}

void Hail::AngelScript::DebuggerServer::StopDebugging()
{
    H_ASSERT(m_currentClient != MAX_UINT, "Must stop debugging on a valid client.");
    ScriptDebugger& client = m_clients[m_currentClient];
    client.m_clientData.m_bDisconnected = false;
    H_DEBUGMESSAGE(StringL::Format("Client nr %d stopped debugging.", m_currentClient + 1));
}

void Hail::AngelScript::DebuggerServer::AddBreakpoints(const FileBreakPoints& breakpointsToAdd)
{
    if (m_pActiveScript)
    {
        for (uint16 iFileName = 0; iFileName < m_pActiveScript->m_fileNames.Size(); iFileName++)
        {
            if (StringCompareCaseInsensitive(m_pActiveScript->m_fileNames[iFileName], breakpointsToAdd.fileName))
            {
                if (breakpointsToAdd.breakPoints.Empty())
                    m_pActiveScript->m_pDebugger->RemoveBreakpoints(breakpointsToAdd);
                else
                    m_pActiveScript->m_pDebugger->AddBreakpoints(breakpointsToAdd);
            }
        }
    }
}

void Hail::AngelScript::DebuggerServer::ContinueDebugging()
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SetExecutionStatus(eScriptExecutionStatus::Normal);
    H_DEBUGMESSAGE("Continue debugging.");
}

void Hail::AngelScript::DebuggerServer::PauseDebugging()
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SetExecutionStatus(eScriptExecutionStatus::PausedExecution);
    H_DEBUGMESSAGE("Paused debugging.");
}

void Hail::AngelScript::DebuggerServer::StepIn()
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SetExecutionStatus(eScriptExecutionStatus::StepIn);
    H_DEBUGMESSAGE("Step in.");
}

void Hail::AngelScript::DebuggerServer::StepOver()
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SetExecutionStatus(eScriptExecutionStatus::StepOver);
    H_DEBUGMESSAGE("Step over.");
}

void Hail::AngelScript::DebuggerServer::StepOut()
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SetExecutionStatus(eScriptExecutionStatus::StepOut);
    H_DEBUGMESSAGE("Step over.");
}

void Hail::AngelScript::DebuggerServer::SendVariables(eCallStack callStackToGet)
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SendVariables(callStackToGet);
}

void Hail::AngelScript::DebuggerServer::FindVariable(eCallStack callStackType, StringL variableToFind)
{
    if (m_pActiveScript)
        m_pActiveScript->m_pDebugger->SendVariable(callStackType, variableToFind);
}

void Hail::AngelScript::DebuggerServer::SendCallstack()
{
    if (m_pActiveScript)
    {
        H_DEBUGMESSAGE("Send Generated Callstack Request");
        m_pActiveScript->m_pDebugger->SendGeneratedCallstack();
    }
    // else do error stuff
}

void Hail::AngelScript::DebuggerServer::RequestBuildErrors(MessageHeader header)
{
    if (m_pActiveScript)
    {
        H_ASSERT(header.type == eDebuggerMessageType::RequestBuildErrors);
        H_DEBUGMESSAGE("Requested build error log");
        m_returnRequests.Add(header);
    }
}

void Hail::AngelScript::DebuggerServer::RequestEngineTypes(MessageHeader header)
{
    H_ASSERT(header.type == eDebuggerMessageType::RequestEngineTypes);
    H_DEBUGMESSAGE("Requested engine types");
    m_returnRequests.Add(header);
}

void Hail::AngelScript::DebuggerServer::AddBuildError(const BuildErrorInfo& buildError)
{
    m_registeredBuildErrors.Add(buildError);
}

void Hail::AngelScript::DebuggerServer::SendDebuggerMessage(DebuggerMessage& messageToSend)
{
    for (int iClient = 0; iClient < m_clients.Size(); iClient++)
    {
        if (m_clients[iClient].m_clientData.m_bConnectedForDebugging)
        {
            int iSendResult = send(m_clients[iClient].m_clientData.m_socket, (const char*)(&messageToSend.m_header), sizeof(MessageHeader), 0);
            if (messageToSend.m_header.messageLength)
                iSendResult = send(m_clients[iClient].m_clientData.m_socket, (messageToSend.m_data.GetMessageData()), messageToSend.m_header.messageLength, 0);
            if (iSendResult == SOCKET_ERROR) {
                H_ERROR(StringL::Format("send failed: %d\n", WSAGetLastError()));
            }
        }
    }

}

void Hail::AngelScript::DebuggerServer::ListenToMessages()
{
    char buffer[4096] = { 0 };

    VectorOnStack<uint32, MAX_ATTACHED_DEBUGGERS> disconnectingSockets;
    const bool wasDebugging = m_bIsDebugging;
    m_bIsDebugging = false;

    for (uint32 iClient = 0; iClient < m_clients.Size(); iClient++)
    {
        buffer[0] = 0;
        ScriptDebugger& script = m_clients[iClient];
        const int iResult = recv(script.m_clientData.m_socket, buffer, sizeof(buffer), 0);
        m_currentClient = iClient;
        if (iResult > 0)
        {
            HandleDebuggerMessage(this, iResult, buffer);
        }
        else if (iResult == 0)
        {
            H_DEBUGMESSAGE("Client connection closing...");
            disconnectingSockets.Add(iClient);
        }
        if (script.m_clientData.m_bConnectedForDebugging)
        {
            if (m_pActiveScript && m_pActiveScript->m_pDebugger->GetIsDebugging() == false)
            {
                m_pActiveScript->m_pDebugger->SetLineCallback();
            }

            if (script.m_clientData.m_bDisconnected)
            {
                script.m_clientData.m_bDisconnected = false;
                GrowingArray<DebuggerMessage>& debuggerMessages = script.GetMessages();
                for (size_t iDebugMessage = 0; iDebugMessage < debuggerMessages.Size(); iDebugMessage++)
                {
                    SendDebuggerMessage(debuggerMessages[iDebugMessage]);
                }
                debuggerMessages.RemoveAll();
                script.m_clientData.m_bConnectedForDebugging = false;
                disconnectingSockets.Add(iClient);
            }
            else
            {
                m_bIsDebugging = true;
            }
        }
    }
    m_currentClient = MAX_UINT;
    // Removing highest indices first to keep the order. 
    for (int i = disconnectingSockets.Size(); i > 0; --i)
    {
        H_Socket socket = m_clients[disconnectingSockets[i - 1]].m_clientData.m_socket;
        m_clients[disconnectingSockets[i - 1]].m_clientData.m_socket = InvalidSocket;
        const int iResult = recv(socket, buffer, sizeof(buffer), 0);
        closesocket(socket);
        m_clients.RemoveCyclicAtIndex(disconnectingSockets[i - 1]);
    }
    if (wasDebugging && !m_bIsDebugging && m_pActiveScript)
    {
        m_pActiveScript->m_pDebugger->StopDebuggingScript();
    }
}

// -- Old reference debugger v --

//#include <iostream>  // cout
//#include <sstream>   // stringstream
//#include <stdlib.h>  // atoi
//#include <assert.h>  // assert
//#include <cstring>   // strlen
//
//using namespace std;
//
//BEGIN_AS_NAMESPACE
//
//CDebugger::CDebugger()
//{
//	m_action = CONTINUE;
//	m_lastFunction = 0;
//	m_engine = 0;
//}
//
//CDebugger::~CDebugger()
//{
//	SetEngine(0);
//}
//

//
//void CDebugger::RegisterToStringCallback(const asITypeInfo *ot, ToStringCallback callback)
//{
//	if( m_toStringCallbacks.find(ot) == m_toStringCallbacks.end() )
//		m_toStringCallbacks.insert(map<const asITypeInfo*, ToStringCallback>::value_type(ot, callback));
//}
//
//void CDebugger::LineCallback(asIScriptContext *ctx)
//{
//	assert( ctx );
//
//	// This should never happen, but it doesn't hurt to validate it
//	if( ctx == 0 )
//		return;
//
//	// By default we ignore callbacks when the context is not active.
//	// An application might override this to for example disconnect the
//	// debugger as the execution finished.
//	if( ctx->GetState() != asEXECUTION_ACTIVE )
//		return;
//
//	if( m_action == CONTINUE )
//	{
//		if( !CheckBreakPoint(ctx) )
//			return;
//	}
//	else if( m_action == STEP_OVER )
//	{
//		if( ctx->GetCallstackSize() > m_lastCommandAtStackLevel )
//		{
//			if( !CheckBreakPoint(ctx) )
//				return;
//		}
//	}
//	else if( m_action == STEP_OUT )
//	{
//		if( ctx->GetCallstackSize() >= m_lastCommandAtStackLevel )
//		{
//			if( !CheckBreakPoint(ctx) )
//				return;
//		}
//	}
//	else if( m_action == STEP_INTO )
//	{
//		CheckBreakPoint(ctx);
//
//		// Always break, but we call the check break point anyway 
//		// to tell user when break point has been reached
//	}
//
//	stringstream s;
//	const char *file = 0;
//	int lineNbr = ctx->GetLineNumber(0, 0, &file);
//	s << (file ? file : "{unnamed}") << ":" << lineNbr << "; " << ctx->GetFunction()->GetDeclaration() << endl;
//	Output(s.str());
//
//	TakeCommands(ctx);
//}
//
//bool CDebugger::CheckBreakPoint(asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//		return false;
//
//	// TODO: Should cache the break points in a function by checking which possible break points
//	//       can be hit when entering a function. If there are no break points in the current function
//	//       then there is no need to check every line.
//
//	const char *tmp = 0;
//	int lineNbr = ctx->GetLineNumber(0, 0, &tmp);
//
//	// Consider just filename, not the full path
//	string file = tmp ? tmp : "";
//	size_t r = file.find_last_of("\\/");
//	if( r != string::npos )
//		file = file.substr(r+1);
//
//	// Did we move into a new function?
//	asIScriptFunction *func = ctx->GetFunction();
//	if( m_lastFunction != func )
//	{
//		// Check if any breakpoints need adjusting
//		for( size_t n = 0; n < m_breakPoints.size(); n++ )
//		{
//			// We need to check for a breakpoint at entering the function
//			if( m_breakPoints[n].func )
//			{
//				if( m_breakPoints[n].name == func->GetName() )
//				{
//					stringstream s;
//					s << "Entering function '" << m_breakPoints[n].name << "'. Transforming it into break point" << endl;
//					Output(s.str());
//
//					// Transform the function breakpoint into a file breakpoint
//					m_breakPoints[n].name           = file;
//					m_breakPoints[n].lineNbr        = lineNbr;
//					m_breakPoints[n].func           = false;
//					m_breakPoints[n].needsAdjusting = false;
//				}
//			}
//			// Check if a given breakpoint fall on a line with code or else adjust it to the next line
//			else if( m_breakPoints[n].needsAdjusting &&
//					 m_breakPoints[n].name == file )
//			{
//				int line = func->FindNextLineWithCode(m_breakPoints[n].lineNbr);
//				if( line >= 0 )
//				{
//					m_breakPoints[n].needsAdjusting = false;
//					if( line != m_breakPoints[n].lineNbr )
//					{
//						stringstream s;
//						s << "Moving break point " << n << " in file '" << file << "' to next line with code at line " << line << endl;
//						Output(s.str());
//
//						// Move the breakpoint to the next line
//						m_breakPoints[n].lineNbr = line;
//					}
//				}
//			}
//		}
//	}
//	m_lastFunction = func;
//
//	// Determine if there is a breakpoint at the current line
//	for( size_t n = 0; n < m_breakPoints.size(); n++ )
//	{
//		// TODO: do case-less comparison for file name
//
//		// Should we break?
//		if( !m_breakPoints[n].func &&
//			m_breakPoints[n].lineNbr == lineNbr &&
//			m_breakPoints[n].name == file )
//		{
//			stringstream s;
//			s << "Reached break point " << n << " in file '" << file << "' at line " << lineNbr << endl;
//			Output(s.str());
//			return true;
//		}
//	}
//
//	return false;
//}
//
//void CDebugger::TakeCommands(asIScriptContext *ctx)
//{
//	for(;;)
//	{
//		char buf[512];
//
//		Output("[dbg]> ");
//		cin.getline(buf, 512);
//
//		if( InterpretCommand(string(buf), ctx) )
//			break;
//	}
//}
//
//bool CDebugger::InterpretCommand(const string &cmd, asIScriptContext *ctx)
//{
//	if( cmd.length() == 0 ) return true;
//
//	switch( cmd[0] )
//	{
//	case 'c':
//		m_action = CONTINUE;
//		break;
//
//	case 's':
//		m_action = STEP_INTO;
//		break;
//
//	case 'n':
//		m_action = STEP_OVER;
//		m_lastCommandAtStackLevel = ctx ? ctx->GetCallstackSize() : 1;
//		break;
//
//	case 'o':
//		m_action = STEP_OUT;
//		m_lastCommandAtStackLevel = ctx ? ctx->GetCallstackSize() : 0;
//		break;
//
//	case 'b':
//		{
//			// Set break point
//			size_t p = cmd.find_first_not_of(" \t", 1);
//			size_t div = cmd.find(':'); 
//			if( div != string::npos && div > 2 && p > 1 )
//			{
//				string file = cmd.substr(2, div-2);
//				string line = cmd.substr(div+1);
//
//				int nbr = atoi(line.c_str());
//
//				AddFileBreakPoint(file, nbr);
//			}
//			else if( div == string::npos && p != string::npos && p > 1 )
//			{
//				string func = cmd.substr(p);
//
//				AddFuncBreakPoint(func);
//			}
//			else
//			{
//				Output("Incorrect format for setting break point, expected one of:\n"
//				       " b <file name>:<line number>\n"
//				       " b <function name>\n");
//			}
//		}
//		// take more commands
//		return false;
//
//	case 'r':
//		{
//			// Remove break point
//			size_t p = cmd.find_first_not_of(" \t", 1);
//			if( cmd.length() > 2 && p != string::npos && p > 1 )
//			{
//				string br = cmd.substr(2);
//				if( br == "all" )
//				{
//					m_breakPoints.clear();
//					Output("All break points have been removed\n");
//				}
//				else
//				{
//					int nbr = atoi(br.c_str());
//					if( nbr >= 0 && nbr < (int)m_breakPoints.size() )
//						m_breakPoints.erase(m_breakPoints.begin()+nbr);
//					ListBreakPoints();
//				}
//			}
//			else
//			{
//				Output("Incorrect format for removing break points, expected:\n"
//				       " r <all|number of break point>\n");
//			}
//		}
//		// take more commands
//		return false;
//
//	case 'l':
//		{
//			// List something
//			bool printHelp = false;
//			size_t p = cmd.find_first_not_of(" \t", 1);
//			if( p != string::npos && p > 1 )
//			{
//				if( cmd[p] == 'b' )
//				{
//					ListBreakPoints();
//				}
//				else if( cmd[p] == 'v' )
//				{
//					ListLocalVariables(ctx);
//				}
//				else if( cmd[p] == 'g' )
//				{
//					ListGlobalVariables(ctx);
//				}
//				else if( cmd[p] == 'm' )
//				{
//					ListMemberProperties(ctx);
//				}
//				else if( cmd[p] == 's' )
//				{
//					ListStatistics(ctx);
//				}
//				else
//				{
//					Output("Unknown list option.\n");
//					printHelp = true;
//				}
//			}
//			else
//			{
//				Output("Incorrect format for list command.\n");
//				printHelp = true;
//			}
//
//			if( printHelp )
//			{
//				Output("Expected format: \n"
//					   " l <list option>\n"
//					   "Available options: \n"
//					   " b - breakpoints\n"
//					   " v - local variables\n"
//					   " m - member properties\n"
//					   " g - global variables\n"
//					   " s - statistics\n");
//			}
//		}
//		// take more commands
//		return false;
//
//	case 'h':
//		PrintHelp();
//		// take more commands
//		return false;
//
//	case 'p':
//		{
//			// Print a value 
//			size_t p = cmd.find_first_not_of(" \t", 1);
//			if( p != string::npos && p > 1 )
//			{
//				PrintValue(cmd.substr(p), ctx);
//			}
//			else
//			{
//				Output("Incorrect format for print, expected:\n"
//					   " p <expression>\n");
//			}
//		}
//		// take more commands
//		return false;
//
//	case 'w':
//		// Where am I?
//		PrintCallstack(ctx);
//		// take more commands
//		return false;
//
//	case 'a':
//		// abort the execution
//		if( ctx == 0 )
//		{
//			Output("No script is running\n");
//			return false;
//		}
//		ctx->Abort();
//		break;
//
//	default:
//		Output("Unknown command\n");
//		// take more commands
//		return false;
//	}
//
//	// Continue execution
//	return true;
//}
//
//void CDebugger::PrintValue(const std::string &expr, asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//	{
//		Output("No script is running\n");
//		return;
//	}
//
//	asIScriptEngine *engine = ctx->GetEngine();
//
//	// Tokenize the input string to get the variable scope and name
//	asUINT len = 0;
//	string scope;
//	string name;
//	string str = expr;
//	asETokenClass t = engine->ParseToken(str.c_str(), 0, &len);
//	while( t == asTC_IDENTIFIER || (t == asTC_KEYWORD && len == 2 && str.compare(0, 2, "::") == 0) )
//	{
//		if( t == asTC_KEYWORD )
//		{
//			if( scope == "" && name == "" )
//				scope = "::";			// global scope
//			else if( scope == "::" || scope == "" )
//				scope = name;			// namespace
//			else
//				scope += "::" + name;	// nested namespace
//			name = "";
//		}
//		else if( t == asTC_IDENTIFIER )
//			name.assign(str.c_str(), len);
//
//		// Skip the parsed token and get the next one
//		str = str.substr(len);
//		t = engine->ParseToken(str.c_str(), 0, &len);
//	}
//
//	if( name.size() )
//	{
//		// Find the variable
//		void *ptr = 0;
//		int typeId = 0;
//
//		asIScriptFunction *func = ctx->GetFunction();
//		if( !func ) return;
//
//		// skip local variables if a scope was informed
//		if( scope == "" )
//		{
//			// We start from the end, in case the same name is reused in different scopes
//			for( asUINT n = func->GetVarCount(); n-- > 0; )
//			{
//				const char* varName = 0;
//				ctx->GetVar(n, 0, &varName, &typeId);
//				if( ctx->IsVarInScope(n) && varName != 0 && name == varName )
//				{
//					ptr = ctx->GetAddressOfVar(n);
//					break;
//				}
//			}
//
//			// Look for class members, if we're in a class method
//			if( !ptr && func->GetObjectType() )
//			{
//				if( name == "this" )
//				{
//					ptr = ctx->GetThisPointer();
//					typeId = ctx->GetThisTypeId();
//				}
//				else
//				{
//					asITypeInfo *type = engine->GetTypeInfoById(ctx->GetThisTypeId());
//					for( asUINT n = 0; n < type->GetPropertyCount(); n++ )
//					{
//						const char *propName = 0;
//						int offset = 0;
//						bool isReference = 0;
//						int compositeOffset = 0;
//						bool isCompositeIndirect = false;
//						type->GetProperty(n, &propName, &typeId, 0, 0, &offset, &isReference, 0, &compositeOffset, &isCompositeIndirect);
//						if( name == propName )
//						{
//							ptr = (void*)(((asBYTE*)ctx->GetThisPointer())+compositeOffset);
//							if (isCompositeIndirect) ptr = *(void**)ptr;
//							ptr = (void*)(((asBYTE*)ptr) + offset);
//							if( isReference ) ptr = *(void**)ptr;
//							break;
//						}
//					}
//				}
//			}
//		}
//
//		// Look for global variables
//		if( !ptr )
//		{
//			if( scope == "" )
//			{
//				// If no explicit scope was informed then use the namespace of the current function by default
//				scope = func->GetNamespace();
//			}
//			else if( scope == "::" )
//			{
//				// The global namespace will be empty
//				scope = "";
//			}
//
//			asIScriptModule *mod = func->GetModule();
//			if( mod )
//			{
//				for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ )
//				{
//					const char *varName = 0, *nameSpace = 0;
//					mod->GetGlobalVar(n, &varName, &nameSpace, &typeId);
//
//					// Check if both name and namespace match
//					if( name == varName && scope == nameSpace )
//					{
//						ptr = mod->GetAddressOfGlobalVar(n);
//						break;
//					}
//				}
//			}
//		}
//
//		if( ptr )
//		{
//			// TODO: If there is a . after the identifier, check for members
//			// TODO: If there is a [ after the identifier try to call the 'opIndex(expr) const' method 
//			if( str != "" )
//			{
//				Output("Invalid expression. Expression doesn't end after symbol\n");
//			}
//			else
//			{
//				stringstream s;
//				// TODO: Allow user to set if members should be expanded
//				// Expand members by default to 3 recursive levels only
//				s << ToString(ptr, typeId, 3, engine) << endl;
//				Output(s.str());
//			}
//		}
//		else
//		{
//			Output("Invalid expression. No matching symbol\n");
//		}
//	}
//	else
//	{
//		Output("Invalid expression. Expected identifier\n");
//	}
//}
//
//void CDebugger::ListBreakPoints()
//{
//	// List all break points
//	stringstream s;
//	for( size_t b = 0; b < m_breakPoints.size(); b++ )
//		if( m_breakPoints[b].func )
//			s << b << " - " << m_breakPoints[b].name << endl;
//		else
//			s << b << " - " << m_breakPoints[b].name << ":" << m_breakPoints[b].lineNbr << endl;
//	Output(s.str());
//}
//
//void CDebugger::ListMemberProperties(asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//	{
//		Output("No script is running\n");
//		return;
//	}
//
//	void *ptr = ctx->GetThisPointer();
//	if( ptr )
//	{
//		stringstream s;
//		// TODO: Allow user to define if members should be expanded or not
//		// Expand members by default to 3 recursive levels only
//		s << "this = " << ToString(ptr, ctx->GetThisTypeId(), 3, ctx->GetEngine()) << endl;
//		Output(s.str());
//	}
//}
//
//void CDebugger::ListLocalVariables(asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//	{
//		Output("No script is running\n");
//		return;
//	}
//
//	asIScriptFunction *func = ctx->GetFunction();
//	if( !func ) return;
//
//	stringstream s;
//	for( asUINT n = 0; n < func->GetVarCount(); n++ )
//	{
//		// Skip temporary variables
//		// TODO: Should there be an option to view temporary variables too?
//		const char* name;
//		func->GetVar(n, &name);
//		if (name == 0 || strlen(name) == 0)
//			continue;
//
//		if( ctx->IsVarInScope(n) )
//		{
//			// TODO: Allow user to set if members should be expanded or not
//			// Expand members by default to 3 recursive levels only
//			int typeId;
//			ctx->GetVar(n, 0, 0, &typeId);
//			s << func->GetVarDecl(n) << " = " << ToString(ctx->GetAddressOfVar(n), typeId, 3, ctx->GetEngine()) << endl;
//		}
//	}
//	Output(s.str());
//}
//
//void CDebugger::ListGlobalVariables(asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//	{
//		Output("No script is running\n");
//		return;
//	}
//
//	// Determine the current module from the function
//	asIScriptFunction *func = ctx->GetFunction();
//	if( !func ) return;
//
//	asIScriptModule *mod = func->GetModule();
//	if( !mod ) return;
//
//	stringstream s;
//	for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ )
//	{
//		int typeId = 0;
//		mod->GetGlobalVar(n, 0, 0, &typeId);
//		// TODO: Allow user to set how many recursive expansions should be done
//		// Expand members by default to 3 recursive levels only
//		s << mod->GetGlobalVarDeclaration(n) << " = " << ToString(mod->GetAddressOfGlobalVar(n), typeId, 3, ctx->GetEngine()) << endl;
//	}
//	Output(s.str());
//}
//
//void CDebugger::ListStatistics(asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//	{
//		Output("No script is running\n");
//		return;
//	}
//
//	asIScriptEngine *engine = ctx->GetEngine();
//	
//	asUINT gcCurrSize, gcTotalDestr, gcTotalDet, gcNewObjects, gcTotalNewDestr;
//	engine->GetGCStatistics(&gcCurrSize, &gcTotalDestr, &gcTotalDet, &gcNewObjects, &gcTotalNewDestr);
//
//	stringstream s;
//	s << "Garbage collector:" << endl;
//	s << " current size:          " << gcCurrSize << endl;
//	s << " total destroyed:       " << gcTotalDestr << endl;
//	s << " total detected:        " << gcTotalDet << endl;
//	s << " new objects:           " << gcNewObjects << endl;
//	s << " new objects destroyed: " << gcTotalNewDestr << endl;
//
//	Output(s.str());
//}
//
//void CDebugger::PrintCallstack(asIScriptContext *ctx)
//{
//	if( ctx == 0 )
//	{
//		Output("No script is running\n");
//		return;
//	}
//
//	stringstream s;
//	const char *file = 0;
//	int lineNbr = 0;
//	for( asUINT n = 0; n < ctx->GetCallstackSize(); n++ )
//	{
//		lineNbr = ctx->GetLineNumber(n, 0, &file);
//		s << (file ? file : "{unnamed}") << ":" << lineNbr << "; " << ctx->GetFunction(n)->GetDeclaration() << endl;
//	}
//	Output(s.str());
//}
//
//void CDebugger::AddFuncBreakPoint(const string &func)
//{
//	// Trim the function name
//	size_t b = func.find_first_not_of(" \t");
//	size_t e = func.find_last_not_of(" \t");
//	string actual = func.substr(b, e != string::npos ? e-b+1 : string::npos);
//
//	stringstream s;
//	s << "Adding deferred break point for function '" << actual << "'" << endl;
//	Output(s.str());
//
//	BreakPoint bp(actual, 0, true);
//	m_breakPoints.push_back(bp);
//}
//
//void CDebugger::AddFileBreakPoint(const string &file, int lineNbr)
//{
//	// Store just file name, not entire path
//	size_t r = file.find_last_of("\\/");
//	string actual;
//	if( r != string::npos )
//		actual = file.substr(r+1);
//	else
//		actual = file;
//
//	// Trim the file name
//	size_t b = actual.find_first_not_of(" \t");
//	size_t e = actual.find_last_not_of(" \t");
//	actual = actual.substr(b, e != string::npos ? e-b+1 : string::npos);
//
//	stringstream s;
//	s << "Setting break point in file '" << actual << "' at line " << lineNbr << endl;
//	Output(s.str());
//
//	BreakPoint bp(actual, lineNbr, false);
//	m_breakPoints.push_back(bp);
//}
//
//void CDebugger::PrintHelp()
//{
//	Output(" c - Continue\n"
//	       " s - Step into\n"
//	       " n - Next step\n"
//	       " o - Step out\n"
//	       " b - Set break point\n"
//	       " l - List various things\n"
//	       " r - Remove break point\n"
//	       " p - Print value\n"
//	       " w - Where am I?\n"
//	       " a - Abort execution\n"
//	       " h - Print this help text\n");
//}
//
//void CDebugger::Output(const string &str)
//{
//	// By default we just output to stdout
//	cout << str;
//}
//
//void CDebugger::SetEngine(asIScriptEngine *engine)
//{
//	if( m_engine != engine )
//	{
//		if( m_engine )
//			m_engine->Release();
//		m_engine = engine;
//		if( m_engine )
//			m_engine->AddRef();
//	}
//}
//
//asIScriptEngine *CDebugger::GetEngine()
//{
//	return m_engine;
//}
//
//END_AS_NAMESPACE
