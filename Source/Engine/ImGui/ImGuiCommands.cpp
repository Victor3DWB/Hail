#include "Engine_PCH.h"
#include "ImGuiCommands.h"
#include "imgui.h"
#include "DebugMacros.h"
#include "ImGuiMaterialEditor.h"
#include "ImGuiAssetBrowser.h"
#include "ImGuiMessageLogger.h"
#include "Resources\ResourceManager.h"
#include "Resources\TextureManager.h"
#include "ImGuiPropertyWindow.h"
#include "ImGuiContext.h"
#include "ImGuiProfilerWindow.h"
#include "HailEngine.h"
#include "Settings.h"
#include "Resources/ResourceRegistry.h"

namespace Hail
{
	ImGuiMaterialEditor g_materialEditor;
	ImGuiAssetBrowser g_assetBrowser;
	ImGuiPropertyWindow g_propertyWindow;
	ImGuiMessageLogger g_messageLogger;
	ImGuiProfilerWindow g_profilerWindow;
	ImGuiContext g_contextObject;
}


bool Hail::ImGuiCommandRecorder::AddBeginCommand(const String64& windowName, uint32_t responseIndex)
{
	ImGuiCommand command = { IMGUI_TYPES::WINDOW, windowName, responseIndex };
	m_commands.Add(command);
	return m_bools[responseIndex].GetResponseValue();
}

void Hail::ImGuiCommandRecorder::AddCloseCommand()
{
	ImGuiCommand command = { IMGUI_TYPES::CLOSEWINDOW, ""};
	m_commands.Add(command);
}

bool Hail::ImGuiCommandRecorder::AddTreeNode(const String64& name, uint32_t responseIndex)
{
	ImGuiCommand command = { IMGUI_TYPES::BEGIN_TREENODE, name, responseIndex };
	m_commands.Add(command);
	return m_bools[responseIndex].GetResponseValue();
}

void Hail::ImGuiCommandRecorder::AddTreeNodeEnd()
{
	ImGuiCommand command = { IMGUI_TYPES::END_TREENODE, "" };
	m_commands.Add(command);
}

bool Hail::ImGuiCommandRecorder::AddTabBar(const String64& name, uint32_t responseIndex)
{
	ImGuiCommand command = { IMGUI_TYPES::BEGIN_TAB_BAR, name, responseIndex };
	m_commands.Add(command);
	return m_bools[responseIndex].GetResponseValue();
}

void Hail::ImGuiCommandRecorder::AddTabBarEnd()
{
	ImGuiCommand command = { IMGUI_TYPES::END_TAB_BAR, "" };
	m_commands.Add(command);
}

bool Hail::ImGuiCommandRecorder::AddTabItem(const String64& name, uint32_t responseIndex)
{
	ImGuiCommand command = { IMGUI_TYPES::BEGIN_TAB_ITEM, name, responseIndex };
	m_commands.Add(command);
	return m_bools[responseIndex].GetResponseValue();
}

void Hail::ImGuiCommandRecorder::OpenMaterialEditor()
{
	ImGuiCommand command = { IMGUI_TYPES::MATERIAL_EDITOR, "" };
	m_commands.Add(command);
}

void Hail::ImGuiCommandRecorder::AddTabItemEnd()
{
	ImGuiCommand command = { IMGUI_TYPES::END_TAB_ITEM, "" };
	m_commands.Add(command);
}


void Hail::ImGuiCommandRecorder::AddSameLine()
{
	ImGuiCommand command = { IMGUI_TYPES::SAME_LINE, "" };
	m_commands.Add(command);
}

void Hail::ImGuiCommandRecorder::AddSeperator()
{
	ImGuiCommand command = { IMGUI_TYPES::SEPERATOR, "" };
	m_commands.Add(command);
}

void Hail::ImGuiCommandRecorder::AddCheckbox(const String64& name, uint32_t responseIndex, bool value)
{
	ImGuiCommand command = { IMGUI_TYPES::CHECKBOX, name, responseIndex };
	m_commands.Add(command);
	m_bools[responseIndex].SetValue(value);
}

void Hail::ImGuiCommandRecorder::AddFloatSlider(const String64& name, uint32_t responseIndex, float value)
{
	ImGuiCommand command = { IMGUI_TYPES::SLIDERF, name, responseIndex };
	m_commands.Add(command);
	m_floats[responseIndex].SetValue(value);
}

void Hail::ImGuiCommandRecorder::AddIntSlider(const String64& name, uint32_t responseIndex, int value)
{
	ImGuiCommand command = { IMGUI_TYPES::SLIDERI, name, responseIndex };
	m_commands.Add(command);
	m_ints[responseIndex].SetValue(value);
}

bool Hail::ImGuiCommandRecorder::AddButton(const String64& name, uint32_t responseIndex)
{
	ImGuiCommand command = { IMGUI_TYPES::BUTTON, name, responseIndex };
	m_commands.Add(command);
	return m_bools[responseIndex].GetResponseValue();
}

bool Hail::ImGuiCommandRecorder::AddTextInput(const String64& name, uint32_t responseIndex, const StringL& textValue)
{
	ImGuiCommand command = { IMGUI_TYPES::TEXT_INPUT, name, responseIndex };
	m_commands.Add(command);
	m_strings[responseIndex].SetValue(textValue);
	return m_bools[responseIndex].GetResponseValue();
}

void Hail::ImGuiCommandRecorder::OpenFileBrowser(ImGuiFileBrowserData* fileBrowserDataToFill)
{
	m_lockThreadData = fileBrowserDataToFill;
	m_lockGameThread = true;
	m_lockType = IMGUI_TYPES::FILE_BROWSER;
}

void Hail::ImGuiCommandRecorder::ClearAndTransferResponses(ImGuiCommandRecorder& writeFrom)
{
	const uint32_t commandsSize = m_commands.Size();
	for (size_t i = 0; i < commandsSize; i++)
	{
		ImGuiCommandRecorder::ImGuiCommand& command = m_commands[i];
		switch (command.commandType)
		{
		case ImGuiCommandRecorder::IMGUI_TYPES::WINDOW:
		case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TREENODE:
		case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_ITEM:
		case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_BAR:
			m_bools[command.responseIndex].TransferOneTimeValue();
			m_bools[command.responseIndex].GetRequestValueRef() = false;
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::DROPDOWN:

			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::CLOSEWINDOW:
		case ImGuiCommandRecorder::IMGUI_TYPES::END_TREENODE:
		case ImGuiCommandRecorder::IMGUI_TYPES::END_TAB_ITEM:
		case ImGuiCommandRecorder::IMGUI_TYPES::END_TAB_BAR:
			break;

		case ImGuiCommandRecorder::IMGUI_TYPES::SLIDERF:
			m_floats[command.responseIndex].TransferValue(writeFrom.m_floats[command.responseIndex]);
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::SLIDERI:
			m_ints[command.responseIndex].TransferValue(writeFrom.m_ints[command.responseIndex]);

			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::CHECKBOX:
			m_bools[command.responseIndex].TransferValue(writeFrom.m_bools[command.responseIndex]);
			break;

		case ImGuiCommandRecorder::IMGUI_TYPES::BUTTON:
			m_bools[command.responseIndex].TransferOneTimeValue();
			m_bools[command.responseIndex].GetRequestValueRef() = false;
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::TEXT_INPUT:
			m_bools[command.responseIndex].TransferOneTimeValue();
			m_bools[command.responseIndex].GetRequestValueRef() = false;
			m_strings[command.responseIndex].TransferValue(writeFrom.m_strings[command.responseIndex]);
			break;
		default:
			break;
		}
	}
	m_commands.RemoveAll();
}

void Hail::ImGuiCommandManager::Init(ResourceManager* resourceManager)
{
	m_resourceManager = resourceManager;
	m_commandRecorder[0].m_commands.Prepare(100);
	m_commandRecorder[1].m_commands.Prepare(100);
	g_contextObject.SetResourceManager(resourceManager);
}

void Hail::ImGuiCommandManager::DeInit()
{
	g_assetBrowser.DeInit();
}

void Hail::ImGuiCommandManager::RenderImguiCommands(RenderParams renderParams)
{
	if (!renderParams.m_pFrameRenderSettings->m_bEnableImgui)
		return;

	ImGuiCommandRecorder& recorder = m_commandRecorder[m_readCommandRecorder];
	const uint32_t commandsSize = recorder.m_commands.Size();
	bool testBoolList[5];
	for (size_t i = 0; i < commandsSize; i++)
	{
		ImGuiCommandRecorder::ImGuiCommand& command = recorder.m_commands[i];
		switch (command.commandType)
		{
		case ImGuiCommandRecorder::IMGUI_TYPES::WINDOW:
			m_pushedTypeStack.Add(ImGuiCommandRecorder::IMGUI_TYPES::WINDOW);
			if (ImGui::Begin(command.name.Data()))
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = true;
				m_numberOfOpenWindows++;
			}
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::CLOSEWINDOW:
			PopDownToType(ImGuiCommandRecorder::IMGUI_TYPES::WINDOW);
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::DROPDOWN:
			//if (ImGui::BeginCombo("combo 1", "Din Mamma"))
			//{
			//	ImGui::Selectable("iS THIS SELECTABLE?", testBoolList, ImGuiSelectableFlags_::ImGuiSelectableFlags_AllowDoubleClick);
			//	ImGui::EndCombo();
			//}
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TREENODE:
			if (ImGui::TreeNode(command.name.Data()))
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = true;
				m_pushedTypeStack.Add(ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TREENODE);
				m_numberOfOpenTreeNodes++;
				//m_numberOfOpenTreeNodes++;
			}
			else
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = false;
			}
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::END_TREENODE:
			if (m_numberOfOpenTreeNodes != 0)
			{
				PopDownToType(ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TREENODE);
			}

			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_BAR:
			if (ImGui::BeginTabBar(command.name.Data()))
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = true;
				m_pushedTypeStack.Add(ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_BAR);
				m_numberOfOpenTabBars++;
			}
			else
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = false;
			}
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::END_TAB_BAR:
			if (m_numberOfOpenTabBars != 0)
			{
				PopDownToType(ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_BAR);
			}
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_ITEM:
			if (ImGui::BeginTabItem(command.name.Data()))
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = true;
				m_pushedTypeStack.Add(ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_ITEM);
				m_numberOfOpenTabItems++;
			}
			else
			{
				recorder.m_bools[command.responseIndex].GetRequestValueRef() = false;
			}
		
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::END_TAB_ITEM:
			if (m_numberOfOpenTabItems != 0)
			{
				PopDownToType(ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_ITEM);
			}
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::SAME_LINE:
			ImGui::SameLine();
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::SEPERATOR:
			ImGui::Separator();
			break;


		case ImGuiCommandRecorder::IMGUI_TYPES::SLIDERF:
			if (m_numberOfOpenWindows != 0)
			{
				ImGui::DragFloat(command.name.Data(), &recorder.m_floats[command.responseIndex].GetRequestValueRef(), 0.01);
			}
			//else error handling
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::SLIDERI:
			if (m_numberOfOpenWindows != 0)
			{
				ImGui::DragInt(command.name.Data(), &recorder.m_ints[command.responseIndex].GetRequestValueRef());
			}
			//else error handling
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::CHECKBOX:
			if (m_numberOfOpenWindows != 0)
			{
				ImGui::Checkbox(command.name.Data(), &recorder.m_bools[command.responseIndex].GetRequestValueRef());
			}
			//else error handling
			break;

		case ImGuiCommandRecorder::IMGUI_TYPES::BUTTON:
			if (m_numberOfOpenWindows != 0)
			{
				if (ImGui::Button(command.name.Data()))
				{
					recorder.m_bools[command.responseIndex].GetRequestValueRef() = true;
				}
			}
			//else error handling
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::TEXT_INPUT:
			if (m_numberOfOpenWindows != 0)
			{
				if (ImGui::InputText(command.name.Data(), recorder.m_strings[command.responseIndex].GetRequestValueRef().Data(), 256, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
				{
					recorder.m_bools[command.responseIndex].GetRequestValueRef() = true;
				}
			}
			//else error handling
			break;
		case ImGuiCommandRecorder::IMGUI_TYPES::MATERIAL_EDITOR:
			//TODO: remove
			//g_materialEditor.RenderImGuiCommands(&m_fileBrowser, &m_openMaterialWindow);
			break;

		default:
			break;
		}
	}
	if (m_numberOfOpenWindows != 0 ||
		m_numberOfOpenTabItems != 0 ||
		m_numberOfOpenTabBars != 0 ||
		m_numberOfOpenTreeNodes != 0)
	{
		//Add Error here
		while (!m_pushedTypeStack.Empty())
		{
			SendImGuiPopCommand(m_pushedTypeStack.RemoveLast());
		}
	}
	RenderEngineImgui(renderParams.m_pRenderContext);
}
void Hail::ImGuiCommandManager::RenderSingleImguiCommand(bool& unlockApplicationThread, RenderParams renderParams)
{
	if (!renderParams.m_pFrameRenderSettings->m_bEnableImgui)
		return;

	RenderImguiCommands(renderParams);
	bool succesfulSetup = true;
	if (m_successfullySetupSingleRenderSystem)
	{
		switch (m_singleCommandRenderType)
		{
		case ImGuiCommandRecorder::IMGUI_TYPES::FILE_BROWSER:
			m_fileBrowser.RenderImGuiCommands(unlockApplicationThread);
			break;
		default:
			break;
		}
	}
	else
	{
		RenderErrorModal(unlockApplicationThread);
	}

	RenderEngineImgui(renderParams.m_pRenderContext);

	if (unlockApplicationThread)
	{
		m_lockThreadData = nullptr;
		m_singleCommandRenderType = ImGuiCommandRecorder::IMGUI_TYPES::COUNT;
		m_successfullySetupSingleRenderSystem = false;
	}
}
void Hail::ImGuiCommandManager::PopDownToType(ImGuiCommandRecorder::IMGUI_TYPES typeToPop)
{
	if (m_pushedTypeStack.Empty())
	{
		//Add error here
		return;
	}
	ImGuiCommandRecorder::IMGUI_TYPES poppedType = m_pushedTypeStack.RemoveLast();
	while (poppedType != typeToPop)
	{
		SendImGuiPopCommand(poppedType);
		poppedType = m_pushedTypeStack.Last();
		if (m_pushedTypeStack.Empty())
		{
			//Add error here
			return;
		}
		m_pushedTypeStack.RemoveLast();
	}
	SendImGuiPopCommand(poppedType);
}
void Hail::ImGuiCommandManager::PopLatestStackType()
{
	if (m_pushedTypeStack.Empty())
	{
		//Add error here
		return;
	}
	SendImGuiPopCommand(m_pushedTypeStack.RemoveLast());
}

void Hail::ImGuiCommandManager::RenderErrorModal(bool& unlockApplicationThread)
{
	ImGui::OpenPopup("Error Message");
	if (ImGui::BeginPopupModal("Error Message"))
	{
		switch (m_singleCommandRenderType)
		{
		case ImGuiCommandRecorder::IMGUI_TYPES::FILE_BROWSER:
			ImGui::Text("Filepath used when setting up filebrowser is not valid.\nTo make filebrowser work, update calling command's filepath.");
		default:
			break;
		}

		ImGui::Separator();
		if (ImGui::Button("Done"))
		{
			unlockApplicationThread = true;
		}
		ImGui::EndPopup();
	}
}

void Hail::ImGuiCommandManager::SendImGuiPopCommand(ImGuiCommandRecorder::IMGUI_TYPES typeToPop)
{
	switch (typeToPop)
	{
	case ImGuiCommandRecorder::IMGUI_TYPES::WINDOW:
		if (m_numberOfOpenWindows != 0)
		{
			m_numberOfOpenWindows--;
		}
		ImGui::End();
		break;
	case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TREENODE:
		if (m_numberOfOpenTreeNodes != 0)
		{
			ImGui::TreePop();
			m_numberOfOpenTreeNodes--;
		}
		break;
	case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_BAR:
		if (m_numberOfOpenTabBars != 0)
		{
			ImGui::EndTabBar();
			m_numberOfOpenTabBars--;
		}
		break;
	case ImGuiCommandRecorder::IMGUI_TYPES::BEGIN_TAB_ITEM:
		if (m_numberOfOpenTabItems != 0)
		{
			ImGui::EndTabItem();
			m_numberOfOpenTabItems--;
		}
		break;
	default:
		break;
	}
}

void Hail::ImGuiCommandManager::RenderEngineImgui(RenderContext* pRenderContext)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
	if (ImGui::Begin("Engine Window", nullptr, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Windows"))
			{
				if (ImGui::MenuItem("Asset Browser", NULL, &m_openAssetBrowser))
				{
					if (m_openAssetBrowser)
					{
						m_openPropertyWindow = true;
						m_openMessageLogger = false;
						m_bOpenProfilerlWindow = false;
					}
				}
				if (ImGui::MenuItem("Message Logger", NULL, &m_openMessageLogger))
				{
					if (m_openMessageLogger)
					{
						m_openAssetBrowser = false;
						m_openPropertyWindow = false;
						m_bOpenProfilerlWindow = false;
					}
				}
				if (ImGui::MenuItem("Profiler", NULL, &m_bOpenProfilerlWindow))
				{
					if (m_bOpenProfilerlWindow)
					{
						m_openAssetBrowser = false;
						m_openPropertyWindow = false;
						m_openMessageLogger = false;
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Reload"))
			{
				if (ImGui::MenuItem("Reload All Resources"))
					m_resourceManager->SetReloadOfAllResources();
				if (ImGui::MenuItem("Reload All Texures"))
					m_resourceManager->SetReloadOfAllTextures();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		if (m_openMessageLogger)
		{
			g_messageLogger.RenderImGuiCommands();
		}
		else if (m_openAssetBrowser)
		{
			g_assetBrowser.RenderImGuiCommands(pRenderContext, &m_fileBrowser, m_resourceManager, &g_contextObject);
			if (m_openPropertyWindow)
			{
				ImGui::SameLine();
				ImGuiPropertyWindowReturnValue propertyReturnValue = g_propertyWindow.RenderImGuiCommands(&m_fileBrowser, &g_contextObject);
				if (propertyReturnValue == ImGuiPropertyWindowReturnValue::OpenMaterialWindow)
				{
					m_openMaterialWindow = true;
					g_materialEditor.SetMaterialAsset(&g_contextObject);
				}
				else if (propertyReturnValue == ImGuiPropertyWindowReturnValue::ReloadResource)
				{
					if (g_contextObject.GetCurrentContextType() == ImGuiContextsType::Texture)
					{
						TextureContextAsset* pTextureObject = (TextureContextAsset*)g_contextObject.GetCurrentContextObject();
						m_resourceManager->GetTextureManager()->ReloadImportTextureResource(pTextureObject->m_filePath);
					}
				}
			}
		}
		else if (m_bOpenProfilerlWindow)
		{
			g_profilerWindow.RenderImGuiCommands(&g_contextObject);
		}
	}
	ImGui::End();

	if (m_openMaterialWindow)
	{
		g_materialEditor.RenderImGuiCommands(&m_fileBrowser, *m_resourceManager, &g_contextObject, &m_openMaterialWindow);
	}

	ImGui::PopStyleVar();
	g_messageLogger.FillAndSortMessageList(false);
}

void Hail::ImGuiCommandManager::PopStackType(ImGuiCommandRecorder::IMGUI_TYPES referenceTypeToPop)
{
	if (m_pushedTypeStack.Empty())
	{
		//Add error here
		return;
	}
	const ImGuiCommandRecorder::IMGUI_TYPES typeToPop = m_pushedTypeStack.RemoveLast();

	if (referenceTypeToPop != typeToPop)
	{
		//AddErrorHere
	}
	SendImGuiPopCommand(typeToPop);
}

void Hail::ImGuiCommandManager::SwitchCommandBuffers(bool& shouldLockApplicationThread)
{
	//Read will become write, transfer read to writes values
	m_commandRecorder[m_readCommandRecorder].ClearAndTransferResponses(m_commandRecorder[m_writeCommandRecorder]);
	const uint32_t oldWrite = m_writeCommandRecorder;
	m_writeCommandRecorder = m_readCommandRecorder;
	m_readCommandRecorder = oldWrite;

	if (m_commandRecorder[m_readCommandRecorder].m_lockGameThread)
	{
		m_singleCommandRenderType = m_commandRecorder[m_readCommandRecorder].m_lockType;
		m_commandRecorder[m_readCommandRecorder].m_lockType = ImGuiCommandRecorder::IMGUI_TYPES::COUNT;
		m_lockThreadData = m_commandRecorder[m_readCommandRecorder].m_lockThreadData;
		m_commandRecorder[m_readCommandRecorder].m_lockThreadData = nullptr;
		m_commandRecorder[m_readCommandRecorder].m_lockGameThread = false;
		shouldLockApplicationThread = true;

		switch (m_singleCommandRenderType)
		{
		case ImGuiCommandRecorder::IMGUI_TYPES::FILE_BROWSER:
			m_successfullySetupSingleRenderSystem = m_fileBrowser.Init(reinterpret_cast<ImGuiFileBrowserData*>(m_lockThreadData));
		default:
			break;
		}
	}

}

template<typename Type>
inline void Hail::ImGuiCommand<Type>::SetValue(Type val)
{
	m_requestVal = val;
	if (val != m_responseVal)
	{
		m_isDirty = true;
	}
}

template<typename Type>
inline void Hail::ImGuiCommand<Type>::TransferValue(ImGuiCommand<Type>& otherCommand)
{
	m_responseVal = m_requestVal;
	if (otherCommand.m_isDirty)
	{
		m_responseVal = otherCommand.m_requestVal;
	}
	else
	{
		otherCommand.m_requestVal = m_responseVal;
	}
	otherCommand.m_isDirty = false;
}

template<typename Type>
inline void Hail::ImGuiCommand<Type>::TransferOneTimeValue()
{
	m_responseVal = m_requestVal;
}