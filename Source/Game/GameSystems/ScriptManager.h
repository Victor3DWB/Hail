#pragma once

#include "Utility/Filepath.hpp"
#include "CapabilityBaseClass.h"

class asIScriptEngine;

namespace Hail
{
	class Camera2D;
	class CapabilitySystem;
	class ComponentManager;
	class EntityManager;
	class PhysicsSystem;

	namespace AngelScript
	{
		class TypeRegistry;
		class Handler;
		class Runner;
	}

	struct ScriptFrameAccessibleData
	{
		Camera2D* m_pGame2DCamera = nullptr;
	};

	class ScriptManager
	{
	public:

		struct InitParams
		{
			AngelScript::Handler* m_pHandler;
			EntityManager* m_pEntityManager;
			ComponentManager* m_pComponentManager;
			CapabilitySystem* m_pCapabilitySystem;
			PhysicsSystem* m_pPhysicsSystem;
		};

		bool Init(InitParams initParams);
		void Cleanup();

		void Update(float deltaTime, ScriptFrameAccessibleData& scriptFrameAccessibleData);

		Capability* CreateCapability(CapabilityCreationRequest capabilityCreationRequest);
		const FilePath& GetScriptFilePathForCapability(const String64& capabilityName);
		Transform2D* GetEntityTransform(EntityHandle entityId);
		
		asIScriptObject* GetAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId);
		asIScriptObject* GetOrCreateAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId);
		bool HasAngelscriptComponent(uint32 entityHandle, uint32 componentTypeId);

	private:
		AngelScript::Runner* m_pRunner;
		EntityManager* m_pEntityManager;
		ComponentManager* m_pComponentManager;
		asIScriptEngine* m_pScriptEngine;
		bool RegisterGameScriptClasses(AngelScript::TypeRegistry* pTypeRegistry);
		bool RegisterCommonComponentsAndMembers(AngelScript::TypeRegistry* pTypeRegistry);
		

	};
	
}



