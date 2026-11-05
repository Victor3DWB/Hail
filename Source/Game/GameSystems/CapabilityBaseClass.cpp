#include "Game_PCH.h"
#include "CapabilityBaseClass.h"
#include "ScriptManager.h"

#include "angelscript.h"
#include "Angelscript/TypeRegistry.h"
#include "Angelscript/DebuggerTypes.h"
#include "Angelscript/Debugger.h"

#include "EntityManager.h"

namespace Hail
{
    static uint32 g_capabilityIdCounter = 0u;
    AngelScript::TypeRegistry* g_pTypeRegistry = nullptr;
    AngelScript::Variable GetScriptCapabilityVariableData(void* pObj)
    {
        Capability* pCapability = (Capability*)pObj;
        AngelScript::Variable variableToReturn = AngelScript::ASTypeToVariable(pCapability->GetObject(), pCapability->GetObject()->GetTypeId(), 1, g_pTypeRegistry->GetEngine(), g_pTypeRegistry);
        variableToReturn.m_value = StringL("");
        variableToReturn.m_type = AngelScript::RemoveTypeInformationFromDeclaration(pCapability->GetObject()->GetObjectType()->GetPropertyDeclaration(0));

        return variableToReturn;
    }

    const char* Capability::GetCapabilityAngelScriptBaseClassRelativePath()
    {
        return "EngineScripts/Capability.as";
    }

    void Capability::RegisterAngelscriptFunctions(AngelScript::TypeRegistry* pTypeRegistry)
    {
        g_pTypeRegistry = pTypeRegistry;

        bool bResult;

        bResult = pTypeRegistry->GetEngine()->RegisterEnum("ECapabilityTickGroup");
        H_ASSERT(bResult, "Failed to register input enum");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickGroup", "First", 0, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickGroup", "Input", 1, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickGroup", "Gameplay", 2, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickGroup", "Last", 3, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickGroup", "Max", 4, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");

        bResult = pTypeRegistry->GetEngine()->RegisterEnum("ECapabilityTickSubGroup");
        H_ASSERT(bResult, "Failed to register input enum");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickSubGroup", "Pre", 0, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickSubGroup", "Early", 1, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickSubGroup", "Normal", 2, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickSubGroup", "Late", 3, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");
        bResult = pTypeRegistry->RegisterGlobalEnumValue("ECapabilityTickSubGroup", "Post", 4, H_FILE_LINE); H_ASSERT(bResult, "Failed to register enum value");

        // Register the object properties
        bResult = pTypeRegistry->RegisterType("Capability_t", 0, asOBJ_REF, H_FILE_LINE);
        H_ASSERT(bResult);
        bResult = pTypeRegistry->RegisterVariableFunction("Capability_t", &GetScriptCapabilityVariableData); H_ASSERT(bResult);

        bResult = pTypeRegistry->RegisterManagedClassConstructor("Capability_t", "Capability_t@ f()", { }, asBEHAVE_FACTORY, asCALL_CDECL, asFUNCTION(Capability::Factory), H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterManagedClassConstructor("Capability_t", "void f()", { }, asBEHAVE_ADDREF, asCALL_THISCALL, asMETHOD(Capability, AddRef), H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterManagedClassConstructor("Capability_t", "void f()", { }, asBEHAVE_RELEASE, asCALL_THISCALL, asMETHOD(Capability, Release), H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");

        bResult = pTypeRegistry->RegisterClassOperatorOverload("Capability_t", { "opAssign", "Capability_t", AngelScript::EVariableTypeDataState::Ref }, asMETHOD(Capability, operator=), H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");


        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "Setup", "void" }, { }, asMETHOD(Capability, Setup), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "LateSetup", "void" }, { }, asMETHOD(Capability, LateSetup), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "OnOwnerDestroyed", "void" }, { }, asMETHOD(Capability, OnOwnerDestroyed), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "ShouldActivate", "bool" }, { }, asMETHOD(Capability, ShouldActivate), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "ShouldDeactivate", "bool" }, { }, asMETHOD(Capability, ShouldDeactivate), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "TickActive", "void" }, { { "deltaTime", "float" } }, asMETHODPR(Capability, TickActive, (float), void), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "TickInactive", "void" }, { { "deltaTime", "float" } }, asMETHODPR(Capability, TickInactive, (float), void), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "OnActivated", "void" }, { }, asMETHOD(Capability, OnActivated), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");
        bResult = pTypeRegistry->RegisterClassMethod("Capability_t", { "OnDeactivated", "void" }, { }, asMETHOD(Capability, OnDeactivated), asCALL_THISCALL, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register Capability func");

        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_bActive", "bool" }, asOFFSET(Capability, m_bActive), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_bDidLateSetup", "bool" }, asOFFSET(Capability, m_bDidLateSetup), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_bDidLateSetupHello", "bool" }, asOFFSET(Capability, m_bDidLateSetupHello), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_activeDuration", "float" }, asOFFSET(Capability, m_activeDuration), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_inactiveDuration", "float" }, asOFFSET(Capability, m_inactiveDuration), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_group", "ECapabilityTickGroup" }, asOFFSET(Capability, m_group), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_subGroup", "ECapabilityTickSubGroup" }, asOFFSET(Capability, m_subGroup), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
        bResult = pTypeRegistry->RegisterClassObjectMember("Capability_t", { "m_owningEntityId", "uint" }, asOFFSET(Capability, m_owningEntityId), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
        H_ASSERT(bResult, "Failed to register Capability member");
    }

    Capability::Capability(asIScriptObject* obj) : 
        m_bActive(false),
        m_bDidLateSetup(false),
        m_bDidLateSetupHello(false),
        m_activeDuration(0.0),
        m_inactiveDuration(0.0),
        m_group(ECapabilityTickGroup::Gameplay),
        m_subGroup(ECapabilityTickSubGroup::Normal),
        m_pObj(0), 
        m_pIsDead(0), 
        m_refCount(1),
        m_owningEntityId(MAX_UINT),
        m_id(g_capabilityIdCounter++)
    {
        // Get the weak ref flag for the script object to avoid holding a strong reference to the script class
        m_pIsDead = obj->GetWeakRefFlag();
        m_pIsDead->AddRef();
        m_pObj = obj;
    }

    Capability::~Capability()
    {
        // Release the weak ref flag
        m_pIsDead->Release();
    }

    const char* Capability::GetCapabilityName() const
    {
        H_ASSERT(m_pObj);
        asITypeInfo* type = m_pObj->GetObjectType();
        return type->GetName();
    }

    asIScriptContext* Capability::CallInternalObjectFunction(const char* functionName, const char* functionFullType, float* pFloatArgument) const
    {
        // If the script side is still alive, then call the scripted function
        if (!m_pIsDead->Get())
        {
            // Check that it isn't our script class instance that is makng the call, in 
            // which case we do not callback to the script to avoid endless recursiveness
            asIScriptContext* ctx = asGetActiveContext();
            asIScriptFunction* func = ctx ? ctx->GetFunction(0) : 0;
            const bool bSciptClassInstanceNotMakingTheCall = !func || StringCompare(func->GetName(), functionName) != true || !ctx || ctx->GetThisPointer(0) != m_pObj;
            if (bSciptClassInstanceNotMakingTheCall)
            {
                // Call the script function CallMe so the script can provide the overloaded behavior
                asIScriptEngine* engine = m_pObj->GetEngine();
                ctx = engine->RequestContext();
                

                // Get the actual virtual method (the overridden version in the script, if it exists)
                asIScriptFunction* pVirtualFunc = m_pObj->GetObjectType()->GetMethodByDecl(functionFullType, true);
                // Get the non-virtual method (the base class/parent implementation)
                asIScriptFunction* pNonVirtualFunc = m_pObj->GetObjectType()->GetMethodByDecl(functionFullType, false);

                // GetMethodByDecl returns the virtual function on the script class
                // thus when calling it, the VM will execute the derived method
                asIScriptFunction* pScriptFunc = pVirtualFunc ? pVirtualFunc : pNonVirtualFunc; // m_pObj->GetObjectType()->GetMethodByDecl(functionFullType);
                H_ASSERT(pScriptFunc, "No script function implemented matching function call.");
                ctx->Prepare(pScriptFunc);
                ctx->SetObject(m_pObj);
                if (pFloatArgument)
                {
                    ctx->SetArgFloat(0, *pFloatArgument);
                }
                int r = ctx->Execute();

                if (r != asEXECUTION_FINISHED)
                {
                    if (r == asEXECUTION_EXCEPTION)
                    {
                        H_ERROR(StringL::Format("Exception in Capability %s at line %i. Exception message: %s", this->GetCapabilityName(), ctx->GetExceptionLineNumber(), ctx->GetExceptionString()));
                    }
                    engine->ReturnContext(ctx);
                    return nullptr;
                }
                return ctx;
            }
        }
        return nullptr;
    }

    void Capability::Setup()
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("Setup", "void Setup()");
    }

    void Capability::LateSetup()
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("LateSetup", "void LateSetup()");
        if (pCtx)
        {
            m_pObj->GetEngine()->ReturnContext(pCtx);
        }
    }

    void Capability::OnOwnerDestroyed()
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("OnOwnerDestroyed", "void OnOwnerDestroyed()");
        if (pCtx)
        {
            m_pObj->GetEngine()->ReturnContext(pCtx);
        }
    }

    bool Capability::ShouldActivate() const
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("ShouldActivate", "bool ShouldActivate()");
        if (pCtx)
        {
            const bool bScriptResult = (bool)pCtx->GetReturnByte();
            m_pObj->GetEngine()->ReturnContext(pCtx);
            return bScriptResult;
        }
        return false;
    }

    bool Capability::ShouldDeactivate() const
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("ShouldDeactivate", "bool ShouldDeactivate()");
        if (pCtx)
        {
            const bool bScriptResult = (bool)pCtx->GetReturnByte();
            m_pObj->GetEngine()->ReturnContext(pCtx);
            return bScriptResult;
        }
        return false;
    }

    void Capability::TickActive(float deltaTime)
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("TickActive", "void TickActive(float deltaTime)", &deltaTime);
        if (pCtx)
        {
            m_pObj->GetEngine()->ReturnContext(pCtx);
        }
    }

    void Capability::TickInactive(float deltaTime)
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("TickInactive", "void TickInactive(float deltaTime)", &deltaTime);
        if (pCtx)
        {
            m_pObj->GetEngine()->ReturnContext(pCtx);
        }
    }

    void Capability::OnActivated()
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("OnActivated", "void OnActivated()");
        if (pCtx)
        {
            m_pObj->GetEngine()->ReturnContext(pCtx);
        }
    }

    void Capability::OnDeactivated()
    {
        asIScriptContext* pCtx = CallInternalObjectFunction("OnDeactivated", "void OnDeactivated()");
        if (pCtx)
        {
            m_pObj->GetEngine()->ReturnContext(pCtx);
        }
    }

    float Capability::GetTimeDilation() const
    {
        return 1.0f;
    }

    Capability* Capability::Factory()
	{
        asIScriptContext* ctx = asGetActiveContext();

        // Get the function that is calling the factory so we can be certain it is the Capability script class
        asIScriptFunction* func = ctx->GetFunction(0);
        if (func->GetObjectType() == 0 || !StringCompare(func->GetObjectType()->GetName(), "Capability"))
        {
            ctx->SetException("Invalid attempt to manually instantiate Capability_t");
            return 0;
        }

        // Get the this pointer from the calling function so the Capability C++
        // class can be linked with the Capability script class
        asIScriptObject* obj = reinterpret_cast<asIScriptObject*>(ctx->GetThisPointer(0));

        Capability* pCapability = new Capability(obj);

        obj->SetUserData(pCapability, (uint32)ERegisteredAngelScriptClasses::Capability);

        return pCapability;
	}

    void Capability::SetOwningEntity(const EntityHandle& entity)
    {
        H_ASSERT(m_owningEntityId == MAX_UINT);
        m_owningEntityId = entity;
    }

    void Capability::AddRef()
    {
        H_DEBUGMESSAGE(StringL::Format("Capability::AddRef %p ref=%d\n", this, m_refCount + 1 ));
        m_refCount++;
        // Increment also the reference counter to the script side so
        // it isn't accidentally destroyed before the C++ side
        if (!m_pIsDead->Get())
            m_pObj->AddRef();
    }

    void Capability::Release()
    {
        H_DEBUGMESSAGE(StringL::Format("Capability::Release %p ref=%d\n", this, m_refCount - 1));
        // Release the script instance too
        if (!m_pIsDead->Get())
            m_pObj->Release();
        m_refCount--;
    }
}



