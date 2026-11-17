#include "Game_PCH.h"

#include "CapabilityBaseClass.h"
#include "CapabilitySystem.h"
#include "ComponentManager.h"
#include "EntityManager.h"
#include "PhysicsSystem.h"
#include "ScriptManager.h"

#include "angelscript.h"
#include "Angelscript/Array.h"
#include "Angelscript/TypeRegistry.h"
#include "Angelscript/Handler.h"
#include "Angelscript/Runner.h"
#include "Angelscript/DebuggerTypes.h"

#include "CommonComponents.h"

namespace Hail
{
    // must be global to access internal script objects
    ScriptManager* g_pScriptManager = nullptr;
    CapabilitySystem* g_pCapabilitySystem = nullptr;
    ScriptFrameAccessibleData* g_pScriptFrameAccessibleData = nullptr;
    ComponentManager* g_pComponentManager = nullptr; 
    PhysicsSystem* g_pPhysicsSystem = nullptr;

    Hail::Transform2D* LocalGetEntityTransform(EntityHandle entityId)
    {
        H_ASSERT(entityId != MAX_UINT);
        return g_pScriptManager->GetEntityTransform(entityId);
    }

    asIScriptObject* LocalGetAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
    {
        return g_pScriptManager->GetAngelscriptComponent(entityHandle, componentTypeId);
    }

    asIScriptObject* LocalGetOrCreateAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
    {
        asIScriptObject* pObj = g_pScriptManager->GetOrCreateAngelscriptComponent(entityHandle, componentTypeId);
        return pObj;
    }

    bool LocalHasAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
    {
        return g_pScriptManager->HasAngelscriptComponent(entityHandle, componentTypeId);
    }

    bool LocalCreateCapabilityForEntity(uint32 entityHandle, std::string& capabilityName, std::string& capabilityPath)
    {
        CapabilityCreationRequest capabilityRequest;
        capabilityRequest.m_capabilityName = capabilityName;
        capabilityRequest.m_scriptFilePath = FilePath::GetAngelscriptDirectory().Data();
        capabilityRequest.m_scriptFilePath += FileObject(capabilityPath.c_str(), "as", capabilityRequest.m_scriptFilePath);
        Capability* pCapability = g_pScriptManager->CreateCapability(capabilityRequest);
        if (pCapability)
        {
            g_pCapabilitySystem->RegisterCapabilityToEntity(entityHandle, pCapability);
            return true;
        }
        return false;
    }

    void LocalRemoveCapabilityFromEntity(uint32 entityHandle, std::string& capabilityName)
    {
        g_pCapabilitySystem->UnregisterCapabilityFromEntity(entityHandle, capabilityName.c_str());
    }

    Camera2D* LocalGetCamera2D()
    {
        H_ASSERT(g_pScriptFrameAccessibleData && g_pScriptFrameAccessibleData->m_pGame2DCamera);
        return g_pScriptFrameAccessibleData->m_pGame2DCamera;
    }

    static AngelScript::Variable LocalGetPhysicsCollisionPropertiesVariableData(void* pObj)
    {
        PhysicsCollisionProperties& props = *(PhysicsCollisionProperties*)pObj;

        // TODO: update here
        AngelScript::Variable variableToReturn;
        variableToReturn.m_type = "PhysicsCollisionProperties";
        variableToReturn.m_value = StringL::Format(
            "m_mass : %f | m_drag : %f | m_friction : %f | m_bounciness : %f",
            "m_angularVelocity : %f | m_angularAcceleration : %f | m_momentOfInertia : %f | m_inverseMomentOfInertia : %f",
            "m_torque : %f | m_flags : %u",
            props.m_mass, props.m_drag, props.m_friction, props.m_bounciness,
            props.m_angularVelocity, props.m_angularAcceleration, props.m_momentOfInertia, props.m_inverseMomentOfInertia,
            props.m_torque, props.m_flags
            );

        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_mass", props.m_mass));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_drag", props.m_drag));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_friction", props.m_friction));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_bounciness", props.m_bounciness));

        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_angularVelocity", props.m_angularVelocity));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_angularAcceleration", props.m_angularAcceleration));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_momentOfInertia", props.m_momentOfInertia));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_inverseMomentOfInertia", props.m_inverseMomentOfInertia));

        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_torque", props.m_torque));
        variableToReturn.m_members.Add(AngelScript::GetVariableUint32("m_flags", props.m_flags));

        return variableToReturn;
    }

    static AngelScript::Variable LocalGetPhysicsComponentVariableData(void* pObj)
    {
        PhysicsComponent& physics = *(PhysicsComponent*)pObj;

        AngelScript::Variable variableToReturn;
        variableToReturn.m_type = "PhysicsComponent";
        variableToReturn.m_value = StringL::Format(
            "m_shapeDataIndex : %u | m_shape : %u | m_velocity : %f, %f",
            physics.m_shapeDataIndex,
            (uint32)physics.m_shape,
            physics.m_velocity.x,
            physics.m_velocity.y
        );

        variableToReturn.m_members.Add(AngelScript::GetVariableUint32("m_shapeDataIndex", physics.m_shapeDataIndex));

        AngelScript::Variable& memberShape = variableToReturn.m_members.Add();
        memberShape.m_name = "m_shape";
        memberShape.m_type = "EPhysicShapeType";
        memberShape.m_value = StringL::Format("%s", pEPhysicShapeTypeNames[(uint32)physics.m_shape]);

        variableToReturn.m_members.Add(AngelScript::GetVariableVec2("m_velocity", physics.m_velocity));

        AngelScript::Variable& memberProps = variableToReturn.m_members.Add();
        memberProps = LocalGetPhysicsCollisionPropertiesVariableData(&physics.m_props);
        memberProps.m_name = "m_props";

        return variableToReturn;
    }

    PhysicsComponent* LocalGetPhysicsComponent(uint32 entityHandle)
    {
        return g_pComponentManager->GetPhysicsComponent(entityHandle);
    }

    static AngelScript::Variable LocalGetSpriteComponentVariableData(void* pObj)
    {
        SpriteComponent& sprite = *(SpriteComponent*)pObj;
        glm::vec4 colorAsVec = sprite.m_color.GetColorVec4();
        AngelScript::Variable variableToReturn;
        variableToReturn.m_type = "SpriteComponent";
        variableToReturn.m_value = StringL::Format(
            "m_uvTR_BL : %f, %f, %f, %f | m_pivot : %f, %f | "
            "m_color : %f, %f, %f, %f  | "
            "m_materialInstanceID : %u | m_layer : %d | "
            "m_bSizeRelativeToRenderTarget : %s | m_bIsAffectedBy2DCamera : %s",
            sprite.m_uvTR_BL.x, sprite.m_uvTR_BL.y,
            sprite.m_uvTR_BL.z, sprite.m_uvTR_BL.w,
            sprite.m_pivot.x, sprite.m_pivot.y,
            colorAsVec.x, colorAsVec.y, colorAsVec.z, colorAsVec.w,
            sprite.m_materialInstanceID,
            (int)sprite.m_layer,
            sprite.m_bSizeRelativeToRenderTarget ? "true" : "false",
            sprite.m_bIsAffectedBy2DCamera ? "true" : "false"
        );

        variableToReturn.m_members.Add(AngelScript::GetVariableVec4("m_uvTR_BL", sprite.m_uvTR_BL));
        variableToReturn.m_members.Add(AngelScript::GetVariableVec2("m_pivot", sprite.m_pivot));
        variableToReturn.m_members.Add(AngelScript::GetVariableVec4("m_color", colorAsVec));
        variableToReturn.m_members.Add(AngelScript::GetVariableUint32("m_materialInstanceID", sprite.m_materialInstanceID));
        variableToReturn.m_members.Add(AngelScript::GetVariableInt32("m_layer", (int)sprite.m_layer));
        variableToReturn.m_members.Add(AngelScript::GetVariableBool("m_bSizeRelativeToRenderTarget", sprite.m_bSizeRelativeToRenderTarget));
        variableToReturn.m_members.Add(AngelScript::GetVariableBool("m_bIsAffectedBy2DCamera", sprite.m_bSizeRelativeToRenderTarget));

        return variableToReturn;
    }

    static AngelScript::Variable LocalGetPhysicsCollisionPairData(void* pObj)
    {
        PhysicsCollisionPair& physicsCollisionPair = *(PhysicsCollisionPair*)pObj;

        AngelScript::Variable variableToReturn;
        variableToReturn.m_type = "PhysicsCollisionPair";
        variableToReturn.m_value = StringL::Format(
            "m_entityA : %u | m_entityB : %u",
            physicsCollisionPair.m_entityA,
            physicsCollisionPair.m_entityB
        );

        variableToReturn.m_members.Add(AngelScript::GetVariableUint32("m_entityA", physicsCollisionPair.m_entityA));
        variableToReturn.m_members.Add(AngelScript::GetVariableUint32("m_entityB", physicsCollisionPair.m_entityB));

        return variableToReturn;
    }

    static AngelScript::Variable LocalGetPhysicsCollisionInfoData(void* pObj)
    {
        PhysicsCollisionInfo& physicsCollisionInfo = *(PhysicsCollisionInfo*)pObj;

        AngelScript::Variable variableToReturn;
        variableToReturn.m_type = "PhysicsCollisionInfo";
        variableToReturn.m_value = StringL::Format(
            "m_contactPoint : %f, %f | m_contactNormal : %f, %f | m_penetrationDepth : %f",
            physicsCollisionInfo.m_contactPoint.x,
            physicsCollisionInfo.m_contactPoint.y,
            physicsCollisionInfo.m_contactNormal.x,
            physicsCollisionInfo.m_contactNormal.y,
            physicsCollisionInfo.m_penetrationDepth
        );

        variableToReturn.m_members.Add(AngelScript::GetVariableVec2("m_contactPoint", physicsCollisionInfo.m_contactPoint));
        variableToReturn.m_members.Add(AngelScript::GetVariableVec2("m_contactNormal", physicsCollisionInfo.m_contactNormal));
        variableToReturn.m_members.Add(AngelScript::GetVariableFloat("m_penetrationDepth", physicsCollisionInfo.m_penetrationDepth));

        return variableToReturn;
    }

    static AngelScript::Variable LocalGetPhysicsCollisionEventData(void* pObj)
    {
        PhysicsCollisionEvent& physicsCollisionEvent = *(PhysicsCollisionEvent*)pObj;

        AngelScript::Variable variableToReturn;
        variableToReturn.m_type = "PhysicsCollisionEvent";
        
        const AngelScript::Variable& memberCollisionPair = variableToReturn.m_members.Add(LocalGetPhysicsCollisionPairData(&physicsCollisionEvent.m_collisionPair));
        const AngelScript::Variable&  memberCollisionInfo = variableToReturn.m_members.Add(LocalGetPhysicsCollisionInfoData(&physicsCollisionEvent.m_collisionInfo));
        AngelScript::Variable& memberType = variableToReturn.m_members.Add();
        memberType.m_name = "m_type";
        memberType.m_type = "EPhysicsCollisionEventType";
        memberType.m_value = StringL::Format("%s", pECollisionEventTypeNames[(uint32)physicsCollisionEvent.m_type]);

        variableToReturn.m_value = StringL::Format(
            "m_collisionPair : %s | m_collisionInfo : %s | m_type : %s",
            memberCollisionPair.m_value.Data(),
            memberCollisionInfo.m_value.Data(),
            memberType.m_value.Data()
        );

        return variableToReturn;
    }

    // Registered with AngelScript as 'array<string> @CreateArrayOfString()'
    AngelScript::ScriptArray* LocalCreateArrayOfCollisionEvents(GrowingArray<PhysicsCollisionEvent>& collisionEvents)
    {
        const uint32 collisionEventsCount = collisionEvents.Size();
        // If called from the script, there will always be an active 
        // context, which can be used to obtain a pointer to the engine.
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx)
        {
            asIScriptEngine* engine = ctx->GetEngine();

            // The script array needs to know its type to properly handle the elements.
            // Note that the object type should be cached to avoid performance issues
            // if the function is called frequently.
            asITypeInfo* t = engine->GetTypeInfoByDecl("Array<PhysicsCollisionEvent>");

            // Create an array with the initial size of 3 elements
            AngelScript::ScriptArray* arr = AngelScript::ScriptArray::Create(t, collisionEventsCount);
            for (asUINT i = 0; i < collisionEventsCount; i++)
            {
                // Set the value of each element
                arr->SetValue(i, &collisionEvents[i]);
            }

            // The ref count for the returned handle was already set in the array's constructor
            return arr;
        }
        return 0;
    }

    AngelScript::ScriptArray* LocalGetCollisionEventsForEntity(uint32 entityHandle)
    {
        H_ASSERT(g_pPhysicsSystem, "No physics system assigned");
        if (g_pPhysicsSystem->GetCollisionEventCountForEntity(entityHandle) == 0)
        {
            return nullptr;
        }

        GrowingArray<PhysicsCollisionEvent>& entityCollisionEvents = g_pPhysicsSystem->GetCollisionEventsForEntity(entityHandle);

        return LocalCreateArrayOfCollisionEvents(entityCollisionEvents);
    }
}


bool Hail::ScriptManager::Init(InitParams initParams)
{
    m_pComponentManager = initParams.m_pComponentManager;
    m_pEntityManager = initParams.m_pEntityManager;
    m_pScriptEngine = initParams.m_pHandler->GetScriptEngine();

    g_pScriptManager = this;
    g_pCapabilitySystem = initParams.m_pCapabilitySystem;
    g_pComponentManager = initParams.m_pComponentManager;
    g_pPhysicsSystem = initParams.m_pPhysicsSystem;

    initParams.m_pHandler->GetScriptEngine()->RegisterInterface("IComponent");
    bool bRegistryResult = RegisterGameScriptClasses(initParams.m_pHandler->GetTypeRegistry());

    m_pRunner = new AngelScript::Runner();
    initParams.m_pHandler->SetActiveScriptRunner(m_pRunner);

    AngelScript::Runner::InitParams runnerInitParams;
    runnerInitParams.m_pCapabilitySystem = initParams.m_pCapabilitySystem;
    runnerInitParams.m_pScriptEngine = initParams.m_pHandler->GetScriptEngine();
    runnerInitParams.m_pTypeRegistry = initParams.m_pHandler->GetTypeRegistry();
    runnerInitParams.m_pComponentManager = initParams.m_pComponentManager;

    m_pRunner->Init(runnerInitParams);
    m_pRunner->AddMandatoryScriptPath("CapabilityBase", Capability::GetCapabilityAngelScriptBaseClassRelativePath());
    m_pRunner->AddMandatoryScriptPath("ComponentBase", "EngineScripts/ComponentBase.as");

    StringLW componentScriptPath = FilePath::GetAngelscriptDirectory().Data();
    componentScriptPath += L"Components.as";
    bRegistryResult &= m_pRunner->SetScriptDefinedComponents(FilePath(componentScriptPath));
    m_pRunner->AddMandatoryScriptPath("AngelscriptDefinedComponents", "Components.as");

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

void Hail::ScriptManager::Update(float deltaTime, ScriptFrameAccessibleData& scriptFrameAccessibleData)
{
    g_pScriptFrameAccessibleData = &scriptFrameAccessibleData;
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
    bResult = RegisterCommonComponentsAndMembers(pTypeRegistry);
    H_ASSERT(bResult, "Failed to register global func");

    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetEntityTransform", "Transform2D", false, true }, { {"entityHandle", "uint" } }, asFUNCTION(LocalGetEntityTransform), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");

    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetAngelscriptComponent", "IComponent", false, true }, { {"entityHandle", "uint" }, {"componentTypeId", "uint" } }, asFUNCTION(LocalGetAngelscriptComponent), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetOrCreateAngelscriptComponent", "IComponent", false, true }, { {"entityHandle", "uint" }, {"componentTypeId", "uint" } }, asFUNCTION(LocalGetOrCreateAngelscriptComponent), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    bResult = pTypeRegistry->RegisterGlobalMethod({ "HasAngelscriptComponent", "bool" }, { {"entityHandle", "uint" }, {"componentTypeId", "uint" } }, asFUNCTION(LocalHasAngelscriptComponent), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");


    bResult = pTypeRegistry->RegisterGlobalMethod({ "CreateCapabilityForEntity", "bool" }, { {"entityHandle", "uint" }, {"capabilityName", "string" }, {"capabilityPath", "string" } }, asFUNCTION(LocalCreateCapabilityForEntity), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
    bResult = pTypeRegistry->RegisterGlobalMethod({ "RemoveCapabilityFromEntity", "void" }, { {"entityHandle", "uint" }, {"capabilityName", "string" } }, asFUNCTION(LocalRemoveCapabilityFromEntity), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");
   
    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetCamera2D", "Camera2D", false, true }, {}, asFUNCTION(LocalGetCamera2D), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");

    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetCollisionEventsForEntity", "Array<PhysicsCollisionEvent>", false, true }, { {"entityHandle", "uint" } }, asFUNCTION(LocalGetCollisionEventsForEntity), H_FILE_LINE);
    H_ASSERT(bResult, "Failed to register global func");


    return bResult;
}

bool Hail::ScriptManager::RegisterCommonComponentsAndMembers(AngelScript::TypeRegistry* pTypeRegistry)
{
    bool bResult;

    bResult = m_pScriptEngine->RegisterEnum("EPhysicShapeType");
    // Register the object properties
    H_ASSERT(bResult, "Failed to register input enum");
    for (int i = 0; i < (int)EPhysicShapeType::Count; i++)
    {
        bResult = pTypeRegistry->RegisterGlobalEnumValue("EPhysicShapeType", pEPhysicShapeTypeNames[i], i, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register enum value");
    }

    // PhysicsCollisionProperties
    bResult = pTypeRegistry->RegisterType("PhysicsCollisionProperties", 0, asOBJ_REF | asOBJ_NOCOUNT, H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties type");
    bResult = pTypeRegistry->RegisterVariableFunction("PhysicsCollisionProperties", &LocalGetPhysicsCollisionPropertiesVariableData); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties variable data func");

    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_mass", "float" }, asOFFSET(PhysicsCollisionProperties, m_mass), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_drag", "float" }, asOFFSET(PhysicsCollisionProperties, m_drag), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_friction", "float" }, asOFFSET(PhysicsCollisionProperties, m_friction), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_bounciness", "float" }, asOFFSET(PhysicsCollisionProperties, m_bounciness), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_angularVelocity", "float" }, asOFFSET(PhysicsCollisionProperties, m_angularVelocity), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_angularAcceleration", "float" }, asOFFSET(PhysicsCollisionProperties, m_angularAcceleration), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_momentOfInertia", "float" }, asOFFSET(PhysicsCollisionProperties, m_momentOfInertia), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_inverseMomentOfInertia", "float" }, asOFFSET(PhysicsCollisionProperties, m_inverseMomentOfInertia), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_torque", "float" }, asOFFSET(PhysicsCollisionProperties, m_torque), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionProperties", { "m_flags", "uint32" }, asOFFSET(PhysicsCollisionProperties, m_flags), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionProperties properties");

    // PhysicsComponent
    bResult = pTypeRegistry->RegisterType("PhysicsComponent", 0, asOBJ_REF | asOBJ_NOCOUNT, H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsComponent type");
    bResult = pTypeRegistry->RegisterVariableFunction("PhysicsComponent", &LocalGetPhysicsComponentVariableData); H_ASSERT(bResult, "Failed to register PhysicsComponent variable data func");

    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsComponent", { "m_shapeDataIndex", "uint32" }, asOFFSET(PhysicsComponent, m_shapeDataIndex), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsComponent", { "m_shape", "EPhysicShapeType" }, asOFFSET(PhysicsComponent, m_shape), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsComponent", { "m_velocity", "Vec2" }, asOFFSET(PhysicsComponent, m_velocity), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsComponent", { "m_props", "PhysicsCollisionProperties" }, asOFFSET(PhysicsComponent, m_props), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsComponent properties");

    bResult = pTypeRegistry->RegisterGlobalMethod({ "GetPhysicsComponent", "PhysicsComponent", false, true }, { {"entityHandle", "uint" } }, asFUNCTION(LocalGetPhysicsComponent), H_FILE_LINE);

    // SpriteComponent
    bResult = pTypeRegistry->RegisterType("SpriteComponent", 0, asOBJ_REF | asOBJ_NOCOUNT, H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent type");
    bResult = pTypeRegistry->RegisterVariableFunction("SpriteComponent", &LocalGetSpriteComponentVariableData); H_ASSERT(bResult, "Failed to register SpriteComponent variable data func");

    bResult = pTypeRegistry->RegisterClassObjectMember("SpriteComponent", { "m_pivot", "Vec2" }, asOFFSET(SpriteComponent, m_pivot), H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("SpriteComponent", { "m_color", "uint32" }, asOFFSET(SpriteComponent, m_color), H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("SpriteComponent", { "m_materialInstanceID", "uint32" }, asOFFSET(SpriteComponent, m_materialInstanceID), H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("SpriteComponent", { "m_layer", "int16" }, asOFFSET(SpriteComponent, m_layer), H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("SpriteComponent", { "m_bSizeRelativeToRenderTarget", "bool" }, asOFFSET(SpriteComponent, m_bSizeRelativeToRenderTarget), H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("SpriteComponent", { "m_bIsAffectedBy2DCamera", "bool" }, asOFFSET(SpriteComponent, m_bIsAffectedBy2DCamera), H_FILE_LINE); H_ASSERT(bResult, "Failed to register SpriteComponent properties");

    // PhysicsCollisionPair
    bResult = pTypeRegistry->RegisterType("PhysicsCollisionPair", sizeof(PhysicsCollisionPair), asOBJ_VALUE | asOBJ_POD, H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionPair type");
    bResult = pTypeRegistry->RegisterVariableFunction("PhysicsCollisionPair", &LocalGetPhysicsCollisionPairData); H_ASSERT(bResult, "Failed to register PhysicsCollisionPair variable data func");

    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionPair", { "m_entityA", "uint32" }, asOFFSET(PhysicsCollisionPair, m_entityA), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionPair properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionPair", { "m_entityB", "uint32" }, asOFFSET(PhysicsCollisionPair, m_entityB), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionPair properties");
    
    // PhysicsCollisionInfo
    bResult = pTypeRegistry->RegisterType("PhysicsCollisionInfo", sizeof(PhysicsCollisionInfo), asOBJ_VALUE | asOBJ_POD, H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionPair type");
    bResult = pTypeRegistry->RegisterVariableFunction("PhysicsCollisionInfo", &LocalGetPhysicsCollisionInfoData); H_ASSERT(bResult, "Failed to register PhysicsCollisionPair variable data func");

    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionInfo", { "m_contactPoint", "Vec2" }, asOFFSET(PhysicsCollisionInfo, m_contactPoint), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionInfo properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionInfo", { "m_contactNormal", "Vec2" }, asOFFSET(PhysicsCollisionInfo, m_contactNormal), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionInfo properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionInfo", { "m_penetrationDepth", "float" }, asOFFSET(PhysicsCollisionInfo, m_penetrationDepth), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionInfo properties");
    
    // EPhysicsCollisionEventType
    bResult = m_pScriptEngine->RegisterEnum("EPhysicsCollisionEventType");
    // Register the object properties
    H_ASSERT(bResult, "Failed to register input enum");
    for (int i = 0; i < (int)EPhysicsCollisionEventType::Count; i++)
    {
        bResult = pTypeRegistry->RegisterGlobalEnumValue("EPhysicsCollisionEventType", pECollisionEventTypeNames[i], i, H_FILE_LINE);
        H_ASSERT(bResult, "Failed to register enum value");
    }
    // PhysicsCollisionEvent
    bResult = pTypeRegistry->RegisterType("PhysicsCollisionEvent", sizeof(PhysicsCollisionEvent), asOBJ_VALUE | asOBJ_POD, H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionEvent type");
    bResult = pTypeRegistry->RegisterVariableFunction("PhysicsCollisionEvent", &LocalGetPhysicsCollisionEventData); H_ASSERT(bResult, "Failed to register PhysicsCollisionEvent variable data func");

    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionEvent", { "m_collisionPair", "PhysicsCollisionPair" }, asOFFSET(PhysicsCollisionEvent, m_collisionPair), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionEvent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionEvent", { "m_collisionInfo", "PhysicsCollisionInfo" }, asOFFSET(PhysicsCollisionEvent, m_collisionInfo), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionEvent properties");
    bResult = pTypeRegistry->RegisterClassObjectMember("PhysicsCollisionEvent", { "m_type", "EPhysicsCollisionEventType" }, asOFFSET(PhysicsCollisionEvent, m_type), H_FILE_LINE); H_ASSERT(bResult, "Failed to register PhysicsCollisionEvent properties");

    return bResult;
}

Hail::Transform2D* Hail::ScriptManager::GetEntityTransform(EntityHandle entityId)
{
    H_ASSERT(entityId != MAX_UINT);

    Entity& entity = m_pEntityManager->GetEntityFromIndex(entityId);
    return entity.GetTransform();
}

asIScriptObject* Hail::ScriptManager::GetAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
{
    AngelscriptComponent* pAngelScriptComponent = m_pComponentManager->GetAngelscriptComponent(entityHandle, componentTypeId);
    if (pAngelScriptComponent)
    {
        pAngelScriptComponent->m_pObject->AddRef();
        return pAngelScriptComponent->m_pObject;
    }
    return nullptr;
}

asIScriptObject* Hail::ScriptManager::GetOrCreateAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
{
    AngelscriptComponent* pAngelScriptComponent = m_pComponentManager->GetOrCreateAngelscriptComponent(entityHandle, componentTypeId, m_pScriptEngine);
    if (pAngelScriptComponent)
    {
        pAngelScriptComponent->m_pObject->AddRef();
        return pAngelScriptComponent->m_pObject;
    }
    return nullptr;
}

bool Hail::ScriptManager::HasAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId)
{
    return m_pComponentManager->HasAngelscriptComponent(entityHandle, componentTypeId);
}