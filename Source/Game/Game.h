//Interface for the entire engine
#pragma once
#include "StartupAttributes.h"
#include "InputMappings.h"

#include "GameSystems/EntityManager.h"
#include "GameSystems/PhysicsSystem.h"
#include "GameSystems/ScriptManager.h"
#include "GameSystems/ComponentManager.h"
#include "GameSystems/CapabilitySystem.h"

namespace Hail
{
	namespace AngelScript
	{
		class Handler;
	}

	struct ApplicationFrameData;
	struct RecordedImGuiCommands;
	struct ApplicationCommandPool;
	struct InputMapping;

	class GameApplication
	{
	public:
		void Init(void* initData);
		void PostInit();
		void Update(double totalTime, float deltaTime, Hail::ApplicationFrameData& recievedFrameData);
		void Shutdown();

	private:
		Hail::InputMapping m_inputMapping;
		Hail::RecordedImGuiCommands* m_recordedImguiCommands;
		void FillFrameData(Hail::ApplicationCommandPool& renderCommandPoolToFill);

		ScriptManager m_scriptManager;
		PhysicsSystem m_physicsSystem;
		EntityManager m_entityManager;

		ComponentManager m_componentManager;
		CapabilitySystem m_capabilitySystem;

		AngelScript::Handler* m_pAsHandler;




	};
}



