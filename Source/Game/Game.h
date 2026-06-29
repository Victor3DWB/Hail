//Interface for the entire engine
#pragma once
#include "StartupAttributes.h"
#include "InputMappings.h"

#include "GameSystems/ScriptManager.h"

namespace Hail
{
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

		ScriptManager m_ScriptManager;

	};
}



