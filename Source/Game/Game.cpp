#include "Game_PCH.h"

#include "Game.h"

#include <iostream>
#include "ThreadSynchronizer.h"
#include "RenderCommands.h"
#include "Interface/ApplicationInitData.h"

#include "Camera.h"
#include "Input/InputActionMap.h"

#include "ImGui/ImGuiCommands.h"
#include "ImGui/ImGuiFileBrowser.h"

#include "HailEngine.h"

#include "Interface/ResourceInterface.h"

#include "Interface/DebugLineHelpers.h"
#include "Reflection/Reflection.h"

#include "GameSystems/ComponentArray.h"
#include "GameSystems/SpriteSystem.h"

#include "angelscript.h"
#include "AngelScript/Handler.h"


namespace
{
	Hail::Camera g_camera;
	Hail::Camera2D g_2DCamera;
	float g_movementSpeed = 10.0f;
	float g_spriteMovementSpeed = 5.0f;
	//Hail::GameCommand_Sprite player;
	//Hail::GameCommand_Sprite sprites[5];
	Hail::GameCommand_Sprite g_backgroundSprite;
	bool renderPlayer = true;
	Hail::ImGuiFileBrowserData g_fileBrowserData;

	Hail::GameCommand_Text g_textCommand1{};
	Hail::GameCommand_Text g_textCounter{};
	Hail::GameCommand_Text g_textCounterNumber{};
	Hail::uint32 g_debugCounter{0u};
	Hail::uint32 g_frameCounter{ 0u };

	Hail::GUID spaceShipGuid;
	Hail::GUID backgroundGuid;
	Hail::GUID debugGridGuid;

	Hail::EntityHandle g_playerHandle;
	Hail::EntityHandle g_linePhysicsEntities[2];
	Hail::EntityHandle g_spherePhysicsEntities[5];

	// Stable data so that the script systems pointer is correct, might update to a per frame structure
	Hail::ScriptFrameAccessibleData g_scriptAccessibleFrameData;
}

namespace Hail
{
	void GameApplication::PostInit()
	{
		g_backgroundSprite.materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(backgroundGuid);
		m_componentManager.GetSpriteComponent(g_playerHandle)->m_materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(spaceShipGuid);
		for (size_t i = 0; i < 5; i++)
		{
			m_componentManager.GetSpriteComponent(g_spherePhysicsEntities[i])->m_materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(debugGridGuid);
		}
	}

	void GameApplication::Init(void* initData)
	{
		ApplcationInitData* pAppInitData = (ApplcationInitData*)initData;
		m_pAsHandler = new AngelScript::Handler(pAppInitData->m_pInputSystem, pAppInitData->m_pThreadSynchronizer, true);

		ScriptManager::InitParams scriptManagerInitParams;
		scriptManagerInitParams.m_pCapabilitySystem = &m_capabilitySystem;
		scriptManagerInitParams.m_pComponentManager = &m_componentManager;
		scriptManagerInitParams.m_pEntityManager = &m_entityManager;
		scriptManagerInitParams.m_pHandler = m_pAsHandler;
		m_scriptManager.Init(scriptManagerInitParams);

		m_componentManager.Init(&m_entityManager);

		CapabilitySystem::CapabilitySystemInitParams capabilityInitParams;
		capabilityInitParams.m_pEntityManager = &m_entityManager;
		capabilityInitParams.m_pComponentManager = &m_componentManager;
		capabilityInitParams.m_pScriptManager = &m_scriptManager;
		m_capabilitySystem.Init(capabilityInitParams);

		spaceShipGuid.m_data1 = 3698324670;
		spaceShipGuid.m_data2 = 58616;
		spaceShipGuid.m_data3 = 18806;
		spaceShipGuid.m_data4[0] = 130;
		spaceShipGuid.m_data4[1] = 207;
		spaceShipGuid.m_data4[2] = 174;
		spaceShipGuid.m_data4[3] = 64;
		spaceShipGuid.m_data4[4] = 37;
		spaceShipGuid.m_data4[5] = 86;
		spaceShipGuid.m_data4[6] = 92;
		spaceShipGuid.m_data4[7] = 175;
		Hail::ResourceInterface::LoadMaterialInstanceResource(spaceShipGuid);
		backgroundGuid.m_data1 = 221196968;
		backgroundGuid.m_data2 = 33006;
		backgroundGuid.m_data3 = 17645;
		backgroundGuid.m_data4[0] = 180;
		backgroundGuid.m_data4[1] = 67;
		backgroundGuid.m_data4[2] = 112;
		backgroundGuid.m_data4[3] = 246;
		backgroundGuid.m_data4[4] = 44;
		backgroundGuid.m_data4[5] = 166;
		backgroundGuid.m_data4[6] = 126;
		backgroundGuid.m_data4[7] = 203;
		Hail::ResourceInterface::LoadMaterialInstanceResource(backgroundGuid);
		debugGridGuid.m_data1 = 107330914;
		debugGridGuid.m_data2 = 8412;
		debugGridGuid.m_data3 = 20452;
		debugGridGuid.m_data4[0] = 181;
		debugGridGuid.m_data4[1] = 33;
		debugGridGuid.m_data4[2] = 121;
		debugGridGuid.m_data4[3] = 115;
		debugGridGuid.m_data4[4] = 212;
		debugGridGuid.m_data4[5] = 6;
		debugGridGuid.m_data4[6] = 155;
		debugGridGuid.m_data4[7] = 118;
		Hail::ResourceInterface::LoadMaterialInstanceResource(debugGridGuid);

		g_camera.GetTransform() = glm::lookAt(glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		m_inputMapping = *pAppInitData->m_pInputMapping;

		g_fileBrowserData.extensionsToSearchFor = { "tga", "frag", "vert" };
		g_fileBrowserData.pathToBeginSearchingIn = FilePath::GetResourceSourceDirectory();

		g_textCommand1.index = 0;
		g_textCounter.index = 1;
		g_textCounterNumber.index = 2;

		g_textCommand1.m_layer = 0;
		g_textCounter.m_layer = 1;
		g_textCounterNumber.m_layer = 1;

		g_textCommand1.text = L"Tjenare tjena böddy! <3 :}";
		g_textCommand1.transform.SetPosition({ 0.5f, 0.5f });
		g_textCounter.text = L"Räknare :";
		g_textCounterNumber.text = StringLW::Format(L"%u", g_debugCounter);

		g_textCommand1.textSize = 48u;
		g_textCounter.textSize = 48u;
		g_textCounterNumber.textSize = 64u;
		g_textCommand1.color = glm::vec3(1.0, 0.120, 0.9);
		g_textCounter.color = glm::vec3(1.0, 0.9, 0.9);
		g_textCounterNumber.color = glm::vec3(1.0, 0.0, 0.0);


		g_linePhysicsEntities[0] = m_entityManager.CreateEntity();
		m_entityManager.GetEntityFromIndex(g_linePhysicsEntities[0]).m_transform.SetPosition({ 0.5f, 0.5f });
		g_linePhysicsEntities[1] = m_entityManager.CreateEntity();
		m_entityManager.GetEntityFromIndex(g_linePhysicsEntities[1]).m_transform.SetPosition({ 0.5f, 50.75f });
		PhysicsCollisionProperties linePhysicsProperties;
		linePhysicsProperties.m_mass = 0.f;

		PhysicsLineData lineData;
		lineData.m_length = 1500.4f;
		lineData.m_rotation = Math::PIf;// + 0.914;
		m_componentManager.AddComponentToEntity(m_entityManager.GetEntityFromIndex(g_linePhysicsEntities[0]).GetId(), EComponentType::PhysicsComponent);
		*m_componentManager.GetPhysicsComponent(m_entityManager.GetEntityFromIndex(g_linePhysicsEntities[0]).GetId()) = m_physicsSystem.CreatePhysicsLineComponent(lineData, linePhysicsProperties);
		lineData.m_rotation = Math::PIf;// -0.814;
		m_componentManager.AddComponentToEntity(m_entityManager.GetEntityFromIndex(g_linePhysicsEntities[1]).GetId(), EComponentType::PhysicsComponent);
		*m_componentManager.GetPhysicsComponent(m_entityManager.GetEntityFromIndex(g_linePhysicsEntities[1]).GetId()) = m_physicsSystem.CreatePhysicsLineComponent(lineData, linePhysicsProperties);

		const glm::vec2 spawnPositions[5] = { { -80.f, -350.f }, { 80.f, -350.f }, { -40.f, -100.f }, { 0.f, -100.f }, { 40.f, -100.f } };

		for (uint32 i = 0; i < 5; i++)
		{
			g_spherePhysicsEntities[i] = m_entityManager.CreateEntity();
			m_entityManager.GetEntityFromIndex(g_spherePhysicsEntities[i]).m_transform.SetPosition(spawnPositions[i]);
			m_entityManager.GetEntityFromIndex(g_spherePhysicsEntities[i]).m_transform.SetScale({0.25, 0.25});
		}

		PhysicsCollisionProperties circlePhysicsProperties;
		circlePhysicsProperties.m_mass = 10.0f;
		PhysicsCircleData circlePhysicsData;
		circlePhysicsData.m_radius = 20.015;
		for (uint32 iTestEntites = 0; iTestEntites < 5; iTestEntites++)
		{
			m_componentManager.AddComponentToEntity(m_entityManager.GetEntityFromIndex(g_spherePhysicsEntities[iTestEntites]).GetId(), EComponentType::PhysicsComponent);
			m_componentManager.AddComponentToEntity(m_entityManager.GetEntityFromIndex(g_spherePhysicsEntities[iTestEntites]).GetId(), EComponentType::SpriteComponent);
			*m_componentManager.GetPhysicsComponent(m_entityManager.GetEntityFromIndex(g_spherePhysicsEntities[iTestEntites]).GetId()) = m_physicsSystem.CreatePhysicsCircleComponent(circlePhysicsData, circlePhysicsProperties);
		}

		g_playerHandle = m_entityManager.CreateEntity();
		CapabilityCreationRequest capabilityRequest;
		capabilityRequest.m_capabilityName = "PlayerMovementCapability";
		capabilityRequest.m_scriptFilePath = FilePath::GetAngelscriptDirectory().Data();
		capabilityRequest.m_scriptFilePath += L"IncludeScript.as";
		Capability* pCapability = m_scriptManager.CreateCapability(capabilityRequest);
		if (pCapability)
		{
			m_capabilitySystem.RegisterCapabilityToEntity(g_playerHandle, pCapability);
		}
		m_componentManager.AddComponentToEntity(g_playerHandle, EComponentType::PhysicsComponent);
		m_componentManager.AddComponentToEntity(g_playerHandle, EComponentType::SpriteComponent);
		circlePhysicsProperties.m_mass = 0.0f;
		*m_componentManager.GetPhysicsComponent(g_playerHandle) = m_physicsSystem.CreatePhysicsCircleComponent(circlePhysicsData, circlePhysicsProperties);

		m_componentManager.GetSpriteComponent(g_playerHandle)->m_bSizeRelativeToRenderTarget = false;
		m_componentManager.GetSpriteComponent(g_playerHandle)->m_layer = 2;

		for (uint32_t i = 0; i < 5; i++)
		{
			m_componentManager.GetSpriteComponent(g_spherePhysicsEntities[i])->m_bIsAffectedBy2DCamera = true;
			m_componentManager.GetSpriteComponent(g_spherePhysicsEntities[i])->m_bSizeRelativeToRenderTarget = false;
			m_componentManager.GetSpriteComponent(g_spherePhysicsEntities[i])->m_layer = 1;
		}

		g_backgroundSprite.transform.SetPosition({ 0.5f, 0.5f });
		g_backgroundSprite.m_layer = -1;
		g_backgroundSprite.bIsAffectedBy2DCamera = false;
		g_backgroundSprite.bSizeRelativeToRenderTarget = true;
		g_backgroundSprite.transform.SetScale({ 1.f, 1.f });
	}

	bool g_bTest = false;
	float g_fTest = 0.0f;
	String64 g_sTest = "";


	void GameApplication::Update(double totalTime, float deltaTime, Hail::ApplicationFrameData& recievedFrameData)
	{
		Hail::ApplicationFrameData& frameData = recievedFrameData;

		PhysicsUpdatePayload physicsUpdate;
		physicsUpdate.m_deltaTime = deltaTime;
		physicsUpdate.m_bShowDebugSymbols = true;
		physicsUpdate.m_pEntityManager = &m_entityManager;
		physicsUpdate.m_pPhysicComponents = &m_componentManager.GetPhysicsComponents();
		physicsUpdate.p_commandPoolToFill = frameData.commandPoolToFill;
		m_physicsSystem.UpdatePhysicsWorld(physicsUpdate);

		g_scriptAccessibleFrameData.m_pGame2DCamera = &g_2DCamera;

		m_scriptManager.Update(deltaTime, g_scriptAccessibleFrameData);
		m_capabilitySystem.Update(deltaTime);


		if (g_backgroundSprite.materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(backgroundGuid) == eResourceState::Loaded)
			g_backgroundSprite.materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(backgroundGuid);
		if (m_componentManager.GetSpriteComponent(g_playerHandle)->m_materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(spaceShipGuid) == eResourceState::Loaded)
			m_componentManager.GetSpriteComponent(g_playerHandle)->m_materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(spaceShipGuid);
		for (size_t i = 0; i < 5; i++)
		{
			if (m_componentManager.GetSpriteComponent(g_spherePhysicsEntities[i])->m_materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(debugGridGuid) == eResourceState::Loaded)
				m_componentManager.GetSpriteComponent(g_spherePhysicsEntities[i])->m_materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(debugGridGuid);
		}


		g_2DCamera.SetResolution(frameData.commandPoolToFill->camera2D.GetResolution());
		//Make Gawme ^^
		g_fTest = recievedFrameData.imguiCommandRecorder->GetResponseValue<float>(3);
		g_sTest = recievedFrameData.imguiCommandRecorder->GetResponseValue<StringL>(4);
		if (recievedFrameData.imguiCommandRecorder->AddBeginCommand("ImGui From Game Thread", 0))
		{
			if (recievedFrameData.imguiCommandRecorder->AddButton("File Browser", 1))
			{
				g_fTest += 100.0f;
				recievedFrameData.imguiCommandRecorder->OpenFileBrowser(&g_fileBrowserData);
			}
			g_bTest = recievedFrameData.imguiCommandRecorder->GetResponseValue<bool>(2);
			recievedFrameData.imguiCommandRecorder->AddSameLine();
			recievedFrameData.imguiCommandRecorder->AddCheckbox("Checkbox", 2, g_bTest);
			recievedFrameData.imguiCommandRecorder->AddSeperator();
			if (recievedFrameData.imguiCommandRecorder->AddTreeNode("Tree", 5))
			{
				if (recievedFrameData.imguiCommandRecorder->AddTextInput("Text test", 4, g_sTest))
				{
					g_fTest -= 50.0f;
				}
				if (recievedFrameData.imguiCommandRecorder->AddTabBar("TabBar", 6))
				{
					bool anyTabOpened = false;
					if (recievedFrameData.imguiCommandRecorder->AddTabItem("TestTab1", 7))
					{
						anyTabOpened = true;
						recievedFrameData.imguiCommandRecorder->AddFloatSlider("Float slider", 3, g_fTest);
						recievedFrameData.imguiCommandRecorder->AddTabItemEnd();
					}
					if (recievedFrameData.imguiCommandRecorder->AddTabItem("TestTab2", 8))
					{
						recievedFrameData.imguiCommandRecorder->OpenMaterialEditor();
						anyTabOpened = true;
						recievedFrameData.imguiCommandRecorder->AddFloatSlider("Float slider", 3, g_fTest);
						recievedFrameData.imguiCommandRecorder->AddTabItemEnd();

						if (recievedFrameData.imguiCommandRecorder->AddTreeNode("Tree within a tree", 9))
						{
							recievedFrameData.imguiCommandRecorder->AddTreeNodeEnd();
						}
					}
					recievedFrameData.imguiCommandRecorder->AddTabBarEnd();
				}

				recievedFrameData.imguiCommandRecorder->AddTreeNodeEnd();
			}
			if (recievedFrameData.imguiCommandRecorder->AddTreeNode("Tree within a tree", 10))
			{
				recievedFrameData.imguiCommandRecorder->AddTreeNodeEnd();
			}
			const float cameraZoomResponse = recievedFrameData.imguiCommandRecorder->GetResponseValue<float>(11);
			g_2DCamera.SetZoom(cameraZoomResponse != 0.f ? cameraZoomResponse : g_2DCamera.GetZoom());
			recievedFrameData.imguiCommandRecorder->AddFloatSlider("2D Camera zoom", 11, g_2DCamera.GetZoom());

		}
		recievedFrameData.imguiCommandRecorder->AddCloseCommand();


		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerAction1) == Hail::eInputState::Pressed)
		{
			g_camera.GetTransform().AddToPosition(glm::vec3{ 0.0, 0.0, 1.0 } *g_movementSpeed);
			g_textCounterNumber.text = StringLW::Format(L"%u", ++g_debugCounter);
			g_textCounterNumber.color = glm::vec3(1.0, g_debugCounter * 0.01f, g_debugCounter * 0.01f);
		}
		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerAction2) == Hail::eInputState::Pressed)
		{
			g_camera.GetTransform().AddToPosition(glm::vec3{ 0.0, 0.0, -1.0 } *g_movementSpeed);
		}

		{
			const glm::vec2 oldCameraPos = g_2DCamera.GetPosition();
			const glm::vec2 directionRStick = frameData.inputActionMap->GetDirectionInput(eInputAction::PlayerMoveJoystickR);
			//g_2DCamera.SetPosition(oldCameraPos + glm::vec2(directionRStick.x, directionRStick.y * -1.0f) * 10.f);
		}

		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerPause) == Hail::eInputState::Released)
		{
			renderPlayer = !renderPlayer;
		}

		FillFrameData(*frameData.commandPoolToFill);
		g_frameCounter++;
	}

	void GameApplication::Shutdown()
	{
		m_scriptManager.Cleanup();

		if (asIScriptEngine* pScriptEngine = m_pAsHandler->GetScriptEngine())
		{
			pScriptEngine->ShutDownAndRelease();
		}
		SAFEDELETE(m_pAsHandler);

		g_textCommand1.text.Clear();
		g_textCounter.text.Clear();
		g_textCounterNumber.text.Clear();
	}


	void GameApplication::FillFrameData(Hail::ApplicationCommandPool& commandPoolToFill)
	{
		commandPoolToFill.camera3D = g_camera;
		commandPoolToFill.AddSpriteCommand(g_backgroundSprite);

		SpriteUpdatePayload spriteUpdatePayload;
		spriteUpdatePayload.m_pSpriteComponents = &m_componentManager.GetSpriteComponents();
		spriteUpdatePayload.p_commandPoolToFill = &commandPoolToFill;

		UpdateSpriteComponents(spriteUpdatePayload);

		if (renderPlayer)
		{
			const Entity& playerEntity = m_entityManager.GetEntityFromIndex(g_playerHandle);
			commandPoolToFill.playerPosition = playerEntity.m_transform.GetPosition();

			g_textCounter.transform.SetPosition(playerEntity.m_transform.GetPosition() + glm::vec2(-140, -55));
			g_textCounterNumber.transform.SetPosition(playerEntity.m_transform.GetPosition() + glm::vec2(30, -55));
			commandPoolToFill.AddTextCommand(g_textCounter);
			commandPoolToFill.AddTextCommand(g_textCounterNumber);
		}
		else
		{
			g_textCommand1.transform.SetRotationEuler(-90 + g_frameCounter);
		}
		commandPoolToFill.AddTextCommand(g_textCommand1);
		commandPoolToFill.camera2D = g_2DCamera;
		commandPoolToFill.m_meshCommands.Add(Hail::GameCommand_Mesh());


	}

}