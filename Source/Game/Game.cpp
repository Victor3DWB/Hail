#include "Game_PCH.h"

#include "Game.h"

#include <iostream>
#include "Engine/ThreadSynchronizer.h"
#include "Engine/RenderCommands.h"
#include "Engine/Interface/ApplicationInitData.h"

#include "Camera.h"
#include "Engine/Input/InputActionMap.h"

#include "Engine/ImGui/ImGuiCommands.h"
#include "Engine/ImGui/ImGuiFileBrowser.h"

#include "Engine/HailEngine.h"
#include "Engine/ImGui/ImGuiFileBrowser.h"

#include "Engine/Interface/ResourceInterface.h"

#include "Utility/DebugLineHelpers.h"
#include "Reflection/Reflection.h"

namespace
{
	Hail::Camera g_camera;
	Hail::Camera2D g_2DCamera;
	float g_movementSpeed = 10.0f;
	float g_spriteMovementSpeed = 5.0f;
	Hail::GameCommand_Sprite player;
	Hail::GameCommand_Sprite sprites[5];
	Hail::GameCommand_Sprite manySprites[100];
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
}

namespace Hail
{
	void GameApplication::PostInit()
	{
		sprites[0].materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(backgroundGuid);
		player.materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(spaceShipGuid);
		for (size_t i = 1; i < 5; i++)
		{
			sprites[i].materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(debugGridGuid);
		}
	}

	void GameApplication::Init(void* initData)
	{
		ApplcationInitData* pAppInitData = (ApplcationInitData*)initData;
		m_ScriptManager.Init(pAppInitData->m_pAsHandler, pAppInitData->m_pAsTypeRegistry);

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
		player.transform.SetPosition({ 0.5f, 0.5f });
		player.bSizeRelativeToRenderTarget = false;
		player.transform.SetScale({ 1.f, 1.f });
		player.m_layer = 0;
		//sprites[0].materialInstanceID = 2;
		sprites[0].transform.SetPosition({ 0.5f, 0.5f });
		sprites[0].m_layer = -1;
		sprites[0].bIsAffectedBy2DCamera = false;
		sprites[0].bSizeRelativeToRenderTarget = true;
		sprites[0].transform.SetScale({ 1.f, 1.f });

		sprites[1].transform.SetPosition({ -400, -200 });
		sprites[2].transform.SetPosition({ 200, 200 });
		sprites[3].transform.SetPosition({ -200, 200 });
		sprites[4].transform.SetPosition({ 200, -200 });
		for (uint32_t i = 1; i < 5; i++)
		{
			sprites[i].transform.SetRotationRad({ i * Math::PIf * 0.25f });
			sprites[i].index = i + 1;
			sprites[i].transform.SetScale({ 0.25, 0.25 });
			sprites[i].m_layer = 1;
		}

		for (int x = 0; x < 10; x++)
		{
			for (int y = 0; y < 10; y++)
			{
				int i = x + y * 10;
				manySprites[i].transform.SetRotationRad({ i * 0.25f });
				manySprites[i].transform.SetRotationRad({ i * 0.25f });
				manySprites[i].index = 5 + i;
				manySprites[i].transform.SetScale({ 0.15, 0.15 });
				manySprites[i].transform.SetPosition({ (float)x * 40.1f , (float)y * 40.1f });
				manySprites[i].bSizeRelativeToRenderTarget = true;
				manySprites[i].m_layer = 0;
				manySprites[i].materialInstanceID = sprites[3].materialInstanceID;
			}
		}

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
	}

	bool g_bTest = false;
	float g_fTest = 0.0f;
	String64 g_sTest = "";


	void GameApplication::Update(double totalTime, float deltaTime, Hail::ApplicationFrameData& recievedFrameData)
	{
		m_ScriptManager.Update();


		if (sprites[0].materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(backgroundGuid) == eResourceState::Loaded)
			sprites[0].materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(backgroundGuid);
		if (player.materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(spaceShipGuid) == eResourceState::Loaded)
			player.materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(spaceShipGuid);
		for (size_t i = 1; i < 5; i++)
		{
			if (sprites[i].materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(debugGridGuid) == eResourceState::Loaded)
				sprites[i].materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(debugGridGuid);
		}
		for (size_t i = 0; i < 50; i++)
		{
			if (manySprites[i].materialInstanceID == MAX_UINT && Hail::ResourceInterface::GetMaterialResourceState(debugGridGuid) == eResourceState::Loaded)
				manySprites[i].materialInstanceID = Hail::ResourceInterface::GetMaterialInstanceResourceHandle(debugGridGuid);
		}

		Hail::ApplicationFrameData& frameData = recievedFrameData;
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
		glm::vec2 direction = frameData.inputActionMap->GetDirectionInput(eInputAction::PlayerMoveJoystickL);
		direction.y *= (-1.0f);
		bool moved = false;


		if (direction.x != 0.0 || direction.y != 0.0f)
			moved = true;

		{
			const glm::vec2 oldCameraPos = g_2DCamera.GetPosition();
			const glm::vec2 directionRStick = frameData.inputActionMap->GetDirectionInput(eInputAction::PlayerMoveJoystickR);
			g_2DCamera.SetPosition(oldCameraPos + glm::vec2(directionRStick.x, directionRStick.y * -1.0f) * 10.f);
		}

		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerMoveLeft) == Hail::eInputState::Down)
		{
			direction += (glm::vec2{ -1.0, 0.0 });
			moved = true;
		}
		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerMoveRight) == Hail::eInputState::Down)
		{
			direction += (glm::vec2{ 1.0, 0.0 });
			moved = true;
		}
		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerMoveUp) == Hail::eInputState::Down)
		{
			direction += (glm::vec2{ 0.0, -1.0 });
			moved = true;
		}
		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerMoveDown) == Hail::eInputState::Down)
		{
			direction += glm::vec2{ 0.0, 1.0 };
			moved = true;
		}

		if (moved)
		{
			direction = glm::normalize(direction);
			player.transform.AddToPosition(direction * g_spriteMovementSpeed);
			player.transform.LookAt(direction);
		}
		if (frameData.inputActionMap->GetButtonInput(eInputAction::PlayerPause) == Hail::eInputState::Released)
		{
			renderPlayer = !renderPlayer;
		}

		if (frameData.inputActionMap->GetButtonInput(eInputAction::DebugAction1) == Hail::eInputState::Released)
		{
			H_ERROR("Error frome game");
		}
		if (frameData.inputActionMap->GetButtonInput(eInputAction::DebugAction2) == Hail::eInputState::Released)
		{
			H_WARNING("Warning frome game");
		}

		player.color.SetA(sinf(totalTime) * 0.5 + 0.5f);

		for (uint32_t i = 1; i < 5; i++)
		{
			sprites[i].transform.AddToRotationEuler({ 0.15f * (i + 1) });
			//DrawLine2DPixelSpace(*frameData.commandPoolToFill, sprites[i].transform.GetPosition(), sprites[i].transform.GetRotationRad(), 10.f * (i * 10.f), true);
		}
		FillFrameData(*frameData.commandPoolToFill);
		g_frameCounter++;
	}

	void GameApplication::Shutdown()
	{
		m_ScriptManager.Cleanup();

		g_textCommand1.text.Clear();
		g_textCounter.text.Clear();
		g_textCounterNumber.text.Clear();
	}


	void GameApplication::FillFrameData(Hail::ApplicationCommandPool& commandPoolToFill)
	{
		commandPoolToFill.camera3D = g_camera;
		for (uint32_t i = 0; i < 1; i++)
		{
			commandPoolToFill.AddSpriteCommand(sprites[i]);
		}
		//for (uint32_t i = 0; i < 100; i++)
		//{
		//	commandPoolToFill.AddSpriteCommand(manySprites[i]);
		//}
		if (renderPlayer)
		{
			commandPoolToFill.AddSpriteCommand(player);
			commandPoolToFill.playerPosition = player.transform.GetPosition();
			g_textCounter.transform.SetPosition(player.transform.GetPosition() + glm::vec2(-140, -55));
			g_textCounterNumber.transform.SetPosition(player.transform.GetPosition() + glm::vec2(30, -55));
			//DrawCircle2DPixelSpace(commandPoolToFill, player.transform.GetPosition(), 10.0f, true);
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