#pragma once

#include "Types.h"

class asIScriptObject;
class asIScriptContext;
class asILockableSharedBool;

namespace Hail
{
    namespace AngelScript
    {
        class TypeRegistry;
    }

    enum class ECapabilityTickGroup
    {
        First = 0,
        Input,
        Gameplay,
        Last,
        Max
    };

    enum class ECapabilityTickSubGroup
    {
        Pre = 0,
        Early,
        Normal,
        Late,
        Post
    };

    void RegisterCapability(AngelScript::TypeRegistry* pTypeRegistry);

	class Capability
	{
    public:
        // Array<HashString> Tags;
        // Array<Object> Blockers;

        // Object Owner = nullptr;
        bool m_bActive = false;
        bool m_bDidLateSetup = false;
        bool m_bDidLateSetupHello = false;

        float m_activeDuration = 0.0;
        float m_inactiveDuration = 0.0;

        ECapabilityTickGroup m_group = ECapabilityTickGroup::Gameplay;
        ECapabilityTickSubGroup m_subGroup = ECapabilityTickSubGroup::Normal;

        void Setup();
        void LateSetup();

        void OnOwnerDestroyed();

        bool ShouldActivate() const;
        bool ShouldDeactivate() const;

        void TickActive(float deltaTime);
        void TickInactive(float deltaTime);

        void OnActivated();
        void OnDeactivated();

        float GetTimeDilation() const;

        static Capability* Factory();

        // Reference counting
        void AddRef();
        void Release();

        // Assignment operator
        Capability& operator=(const Capability& o)
        {
            // Copy only the content, not the script proxy class
            m_bActive = o.m_bActive;
            m_bDidLateSetup = o.m_bDidLateSetup;
            m_bDidLateSetupHello = o.m_bDidLateSetupHello;
            m_activeDuration = o.m_activeDuration;
            m_inactiveDuration = o.m_inactiveDuration;
            m_group = o.m_group;
            m_subGroup = o.m_subGroup;
            return *this;
        }

        static const char* GetCapabilityAngelScriptBaseClassRelativePath();

        Capability(asIScriptObject* obj);
        ~Capability();

        asIScriptContext* CallInternalObjectFunction(const char* functionName, const char* functionFullType, float* pFloatArgument = nullptr) const;
        // Reference count
        int m_refCount;

        // The C++ side holds a weak link to the script side to avoid a circular reference between the C++ side and script side
        asILockableSharedBool* m_pIsDead;
        asIScriptObject* m_pObj;

        // bool IsBlocked() const
        // {
        //     return Blockers.Num() > 0;
        // }
	};
}



