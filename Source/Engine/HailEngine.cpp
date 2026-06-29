#include "Engine_PCH.h"
#include "HailEngine.h"

#include "Input\InputHandler.h"
#include "Input\InputActionMap.h"

#include "Interface/ApplicationInitData.h"

#include "ApplicationWindow.h"
#include "Timer.h"
#include "Resources\ResourceManager.h"
#include "Resources\ResourceRegistry.h"
#include "Settings.h"
#include "Interface\ResourceInterface.h"
#include "ThreadSynchronizer.h"

#include "InternalMessageHandling\InternalMessageLogger.h"
#include "StringMemoryAllocator.h"

#include <iostream>
#include "imgui.h"
#include "ImGui\ImGuiCommands.h"

#include "angelscript.h"
#include "AngelScript\Handler.h"
#include "AngelScript\Runner.h"
#include "AngelScript\Debugger.h"

#ifdef PLATFORM_WINDOWS

#include "Windows/Windows_ApplicationWindow.h"
#include "Windows/Windows_InputHandler.h"
#include "Windows/Windows_Renderer.h"
#include "Hail_Time.h"
//#elif PLATFORM_OSX//.... more to be added

#endif

using namespace Hail;

namespace Hail
{
	struct EngineData
	{
		Timer timer;
		InputHandler* inputHandler = nullptr;
		InputActionMap inputActionMap;
		ApplicationWindow* appWindow = nullptr;
		Renderer* renderer = nullptr;
		ResourceManager* resourceManager = nullptr;
		ResourceRegistry resourceRegistry;
		ThreadSyncronizer threadSynchronizer;
		ImGuiCommandManager imguiCommandRecorder;
		callback_function_totalTime_dt_frmData updateFunctionToCall = nullptr;
		callback_function shutdownFunctionToCall = nullptr;
		RenderSettings m_renderSettings;
		ApplicationSettings m_appSettings;

		// TODO move all atomics to its own class to structure it up better
		std::atomic<bool> m_bPauseSimulation = false;
		std::atomic<bool> runApplication = false;
		std::atomic<bool> pauseApplication = false;
		std::atomic<bool> runMainThread = false;
		std::atomic<bool> terminateApplication = false;

		std::atomic<bool> applicationLoopDone = false;
		std::atomic<float> gameFrameTimer = 0.0f;

		std::thread applicationThread;
		float applicationTickRate = 0;

		AngelScript::Handler* pAsHandler;
	};

	EngineData* g_engineData = nullptr;

	void MainLoop();
	void ProcessRendering(const bool applicationThreadLocked);
	void ProcessApplicationThread();
	void Cleanup();

	// Transfers atomic settings, syncs the non atomic flags and sets the settings in the rendering systems
	void TransferSettings();
}

bool Hail::InitEngine(StartupAttributes& startupData)
{
	SetMainThread();
	InternalMessageLogger::Initialize();
	StringMemoryAllocator::Initialize();

	g_engineData = new EngineData();
	SetGlobalTimer(&g_engineData->timer);

#ifdef PLATFORM_WINDOWS
	g_engineData->appWindow = new Windows_ApplicationWindow();
	g_engineData->inputHandler = new Windows_InputHandler();
	g_engineData->renderer = new VlkRenderer();
#endif

	g_engineData->inputHandler->InitInputMapping();
	if(!g_engineData->appWindow->Init(startupData, g_engineData->inputHandler))
	{
		Cleanup();
		return false;
	}
	g_engineData->renderer->InitDevice(&g_engineData->timer, startupData.m_pErrorManager);
	if (startupData.m_pErrorManager->GetAreErrorsLogged())
	{
		Cleanup();
		return false;
	}
	g_engineData->resourceRegistry.Init();

	g_engineData->resourceManager = new ResourceManager();
	g_engineData->renderer->InitGraphicsEngineAndContext(g_engineData->resourceManager, startupData.m_pErrorManager);
	if (!g_engineData->resourceManager->InitResources(g_engineData->renderer->GetRenderingDevice(), 
		g_engineData->renderer->GetCurrentContext(), startupData.renderTargetResolution, startupData.startupWindowResolution, startupData.m_pErrorManager))
	{
		Cleanup();
		return false;
	}

	// Dependent on resources of the resource manager
	g_engineData->renderer->Initialize(startupData.m_pErrorManager);

	if (startupData.m_pErrorManager->GetAreErrorsLogged())
	{
		Cleanup();
		return false;
	}
	ResourceInterface::InitializeResourceInterface(*g_engineData->resourceManager);

	g_engineData->inputActionMap.Init(g_engineData->inputHandler);

	g_engineData->pAsHandler = new AngelScript::Handler(&g_engineData->inputActionMap, &g_engineData->threadSynchronizer, true);

	g_engineData->applicationTickRate = (float32)startupData.applicationTickRate;
	const float tickTime = 1.0f / g_engineData->applicationTickRate;
	g_engineData->threadSynchronizer.Init(tickTime);
	g_engineData->imguiCommandRecorder.Init(g_engineData->resourceManager);

	ApplcationInitData appInitData;
	appInitData.m_pAsHandler = g_engineData->pAsHandler;
	appInitData.m_pAsTypeRegistry = g_engineData->pAsHandler->GetTypeRegistry();
	appInitData.m_pInputMapping = &g_engineData->inputHandler->GetInputMapping();

	startupData.initFunctionToCall(&appInitData); // Init the calling application
	g_engineData->updateFunctionToCall = startupData.updateFunctionToCall;
	g_engineData->shutdownFunctionToCall = startupData.shutdownFunctionToCall;

	g_engineData->threadSynchronizer.SynchronizeAppData(g_engineData->inputActionMap, g_engineData->imguiCommandRecorder.FetchImguiResults(), *g_engineData->resourceManager);
	startupData.postInitFunctionToCall();

	return true;
}

void Hail::CleanupEngineSystems()
{
	StringMemoryAllocator::Deinitialize();
}

void Hail::StartEngine()
{
	g_engineData->runApplication = true;
	g_engineData->runMainThread = true;
	g_engineData->applicationThread = std::thread( &ProcessApplicationThread);
	MainLoop();
}

void Hail::ShutDownEngine()
{
	g_engineData->terminateApplication = true;
}

Hail::InputHandler& Hail::GetInputHandler()
{
	return *g_engineData->inputHandler;
}

Hail::ResourceRegistry& Hail::GetResourceRegistry()
{
	return g_engineData->resourceRegistry;
}

const Hail::Timer& Hail::GetRenderLoopTimer()
{
	return g_engineData->timer;
}

bool Hail::IsRunning()
{
	return g_engineData->runApplication.load();
}

bool Hail::IsApplicationTerminated()
{
	return g_engineData->terminateApplication.load();
}

void Hail::HandleApplicationMessage(ApplicationMessage message)
{
	if (g_engineData)
		g_engineData->appWindow->SetApplicationSettings(message);
}

Hail::ApplicationWindow* Hail::GetApplicationWIndow()
{
	return g_engineData->appWindow;
}

void Hail::SetSimulationMode(Hail::eEngineSimulationMode simulationMode)
{
	if (simulationMode == eEngineSimulationMode::Paused)
	{
		std::atomic_exchange(&g_engineData->m_bPauseSimulation, true);
	}
	else
	{
		std::atomic_exchange(&g_engineData->m_bPauseSimulation, false);
	}
}


void Hail::MainLoop()
{
	bool lockApplicationThread = false;
	EngineData& engineData = *g_engineData;
	while (engineData.runMainThread)
	{
		engineData.timer.FrameStart();

		// Updates window state and checks for input messages from OS
		engineData.appWindow->ApplicationUpdateLoop();

		TransferSettings();

		const glm::uvec2 resolution = Hail::GetApplicationWIndow()->GetWindowResolution();
		if(lockApplicationThread == false && g_engineData->m_renderSettings.m_bPausedSimulation == false)
		{
			engineData.threadSynchronizer.SynchronizeRenderData(engineData.timer.GetDeltaTime());
		}

		if (resolution.x != 0.0f && resolution.y != 0.0f)
		{
			ProcessRendering(lockApplicationThread);
		}

		//SwapData
		if (engineData.applicationLoopDone)
		{
			// This area of the code is synchronized and locks the game thread, so keep code running here to a minimal
			engineData.inputHandler->UpdateGamepads();
			g_engineData->inputActionMap.UpdateInputActions();
			InternalMessageLogger::GetInstance().Update();
			engineData.imguiCommandRecorder.SwitchCommandBuffers(lockApplicationThread);
			engineData.threadSynchronizer.SynchronizeAppData(engineData.inputActionMap, engineData.imguiCommandRecorder.FetchImguiResults(), *engineData.resourceManager);

			if (lockApplicationThread)
			{
				// Exception if the application thread is locked by a debug command
				engineData.pauseApplication = true;
				engineData.runApplication = false;
				engineData.applicationThread.join();
				engineData.threadSynchronizer.SynchronizeRenderData(0.0f);
			}

			engineData.applicationLoopDone = false;
			engineData.inputHandler->UpdateKeyStates();
			engineData.threadSynchronizer.TransferGameCommandsToRenderCommands(*engineData.resourceManager);
		}
		if (engineData.pauseApplication == false)
		{
			lockApplicationThread = false;
		}
	}
	engineData.applicationThread.join();
	engineData.renderer->WaitForGPU();
	Cleanup();
}

void Hail::ProcessRendering(const bool applicationThreadLocked)
{
	EngineData& engineData = *g_engineData;
	Hail::InputMapping& inputMapping = g_engineData->inputHandler->GetInputMapping();

	Renderer::RenderStartFrameParams startFrameParams;
	startFrameParams.m_pRenderPool = &g_engineData->threadSynchronizer.GetRenderPool();
	startFrameParams.m_renderSettings = g_engineData->m_renderSettings;

	engineData.renderer->StartFrame(startFrameParams);
	engineData.renderer->Prepare();

	ImGuiCommandManager::RenderParams imGuiRenderParams;
	imGuiRenderParams.m_pFrameRenderSettings = &g_engineData->m_renderSettings;
	imGuiRenderParams.m_pRenderContext = g_engineData->renderer->GetCurrentContext();
	if (applicationThreadLocked)
	{
		bool unlockApplicationThread = false;
		engineData.imguiCommandRecorder.RenderSingleImguiCommand(unlockApplicationThread, imGuiRenderParams);
		if (unlockApplicationThread)
		{
			engineData.pauseApplication = false;
			engineData.runApplication = true;
			engineData.applicationThread = std::thread(&ProcessApplicationThread);
			engineData.inputHandler->UpdateKeyStates();
		}
	}
	else
	{
		engineData.imguiCommandRecorder.RenderImguiCommands(imGuiRenderParams);
	}

	engineData.renderer->Render();
	engineData.renderer->EndFrame();
}

void Hail::ProcessApplicationThread()
{
	EngineData& engineData = *g_engineData;
	Timer applicationTimer;
	const float tickTime = 1.0f / engineData.applicationTickRate;
	float applicationTime = 0.0;

	while(engineData.runApplication)
	{
		applicationTimer.FrameStart();
		applicationTime += applicationTimer.GetDeltaTime();
		if (applicationTime >= tickTime && !engineData.applicationLoopDone)
		{
			engineData.updateFunctionToCall(applicationTimer.GetTotalTime(), tickTime, engineData.threadSynchronizer.GetAppFrameData());
			engineData.threadSynchronizer.PrepareApplicationData();
			applicationTime = 0.0;
			engineData.applicationLoopDone = true;
		}
		if(engineData.terminateApplication)
		{
			engineData.runApplication = false;
			engineData.shutdownFunctionToCall();
		}
	}
	if (engineData.pauseApplication == false)
	{
		g_engineData->runMainThread = false;
	}

}

void Hail::Cleanup()
{
	g_engineData->imguiCommandRecorder.DeInit();
	g_engineData->renderer->Cleanup();
	if (asIScriptEngine* pScriptEngine = g_engineData->pAsHandler->GetScriptEngine())
		pScriptEngine->ShutDownAndRelease();
	SAFEDELETE(g_engineData->pAsHandler);
	SAFEDELETE(g_engineData->appWindow);
	SAFEDELETE(g_engineData->inputHandler);
	SAFEDELETE(g_engineData->renderer);
	SAFEDELETE(g_engineData->resourceManager);
	SAFEDELETE(g_engineData);
	InternalMessageLogger::Deinitialize();
}

void Hail::TransferSettings()
{
	g_engineData->m_renderSettings.m_bPausedSimulation = g_engineData->m_bPauseSimulation;

}