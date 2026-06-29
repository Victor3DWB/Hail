#include "Engine_PCH.h"
#include "ImGuiAssetBrowser.h"

#include "ImGuiContext.h"

#include "Reflection/SerializationOverride.h"
#include "imgui.h"

#include "MathUtils.h"
#include "Utility\StringUtility.h"

#include "Resources\MaterialManager.h"
#include "Resources\MaterialResources.h"
#include "Resources\ResourceManager.h"
#include "Resources\ResourceRegistry.h"
#include "Resources\TextureManager.h"

#include "Rendering\RenderContext.h"

#include "HailEngine.h"

#include "ImGuiHelpers.h"
#ifdef PLATFORM_WINDOWS
#include "Resources\Vulkan\VlkTextureResource.h"
#endif

using namespace Hail;
#pragma optimize("", off)

namespace 
{
	//StaticArray<String64, (uint32_t)eMaterialType::COUNT> g_materialTypeStrings;
	constexpr uint32 ASSET_SIZE = 100;
	// will return true if double clicked
	bool RenderAssetPreview(SelectAbleFileObject& fileObject, ImGuiTextureResource* imageResource, const MetaResource* pMetaResource)
	{
		if (!imageResource)
			return false;

		ImGuiIO& io = ImGui::GetIO();
		ImGui::BeginChild(fileObject.m_fileObject.Name().ToCharString().Data(), { ASSET_SIZE, ASSET_SIZE * 1.3 });
		ImGuiID id = ImGui::GetID(fileObject.m_fileObject.Name().ToCharString().Data());

		io.MousePos;
		const ImVec2 windowPos = ImGui::GetWindowPos();
		const bool hovered = 
			windowPos.x < io.MousePos.x 
			&& windowPos.x + ASSET_SIZE > io.MousePos.x
			&& windowPos.y < io.MousePos.y 
			&& windowPos.y + ASSET_SIZE * 1.3 > io.MousePos.y;

		const bool showBackground = fileObject.m_selected || hovered;

		if (hovered)
		{
			ImGui::BeginChildFrame(id, { ASSET_SIZE, ASSET_SIZE * 1.3 }, showBackground ? 0 : ImGuiWindowFlags_NoBackground);
			if (pMetaResource)
				ImGuiHelpers::MetaResourceTooltipPanel(pMetaResource);
			if (io.MouseClicked[0])
			{
				fileObject.m_selected = !fileObject.m_selected;
			}
		}
		else
			ImGui::BeginChildFrame(id, { ASSET_SIZE, ASSET_SIZE * 1.3 }, showBackground ? 0 : ImGuiWindowFlags_NoBackground);
		

		ImVec2 uv_min = ImVec2(0.0f, 1.0f);                 // Top-left
		ImVec2 uv_max = ImVec2(1.0f, 0.0f);                 // Lower-right
		ImVec4 tint_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);   // No tint
		ImVec4 border_col = ImVec4(1.0, 1.0, 1.0, 0.10);
		ImGui::Image(*(VkDescriptorSet*)imageResource->GetImguiTextureResource(), ImVec2(ASSET_SIZE * 0.9, ASSET_SIZE * 0.9), uv_min, uv_max, tint_col, border_col);

		ImGui::TextWrapped(fileObject.m_fileObject.Name().ToCharString().Data());
		ImGui::EndChildFrame();
		ImGui::EndChild();
		if (hovered && io.MouseDoubleClicked[0])
			return true;

		return false;
	}

	bool CreateMaterialPopup(const Hail::FilePath& currentPath, String64& assetName, uint32& materialType)
	{
		ImGui::OpenPopup("Creation Window");
		if (ImGui::BeginPopupModal("Creation Window"))
		{
			ImGui::InputText("Resource Name: ", assetName.Data(), 256);

			materialType = ImGuiHelpers::GetMaterialTypeComboBox(materialType);
			if (StringLength(assetName) > 0)
			{
				if (ImGui::Button("Ok"))
				{
					ImGui::EndPopup();
					return false;
				}
			}
			ImGui::EndPopup();
		}
		return true;
	}
}

Hail::ImGuiAssetBrowser::ImGuiAssetBrowser()
{
	m_openedFileBrowser = false;
	m_creatingMaterial = false;
	m_resourceManager = nullptr;
}

void Hail::ImGuiAssetBrowser::DeInit()
{
	if (m_resourceManager)
	{
		for (uint32 iFolder = 0; iFolder < MAX_RESOURCE_FOLDER_DEPTH; iFolder++)
		{
			GrowingArray<TextureFolder>& textureFolders = m_ImGuiTextureResources[iFolder];

			for (uint32 iTextureFolder = 0; iTextureFolder < textureFolders.Size(); iTextureFolder++)
			{
				for (size_t i = 0; i < textureFolders[iTextureFolder].folderTextures.Size(); i++)
				{
					TextureContextAsset& texture = textureFolders[iTextureFolder].folderTextures[i];
					m_resourceManager->GetTextureManager()->DeleteImGuiTextureResource(texture.m_texture);
					SAFEDELETE(texture.m_texture);
				}
				textureFolders[iTextureFolder].folderTextures.DeleteAll();
			}
		}

		if (m_folderTexture.m_texture)
			m_resourceManager->GetTextureManager()->DeleteImGuiTextureResource(m_folderTexture.m_texture);
		if (m_materialIconTexture.m_texture)
			m_resourceManager->GetTextureManager()->DeleteImGuiTextureResource(m_materialIconTexture.m_texture);
	}
}

void ImGuiAssetBrowser::RenderImGuiCommands(RenderContext* pRenderContext, ImGuiFileBrowser* fileBrowser, ResourceManager* resourceManager, ImGuiContext* contextObject)
{
	m_resourceManager = resourceManager;
	InitCommonData(pRenderContext);
	if (m_openedFileBrowser)
	{
		bool closeCommandWasSent = false;
		fileBrowser->RenderImGuiCommands(closeCommandWasSent);
		if (closeCommandWasSent)
		{
			if (!m_textureFileBrowserData.objectsToSelect.Empty())
			{
				ImportTextureLogic();
			}
			if (!m_shaderFileBrowserData.objectsToSelect.Empty())
			{
				ImportShaderResourceLogic();
			}
			InitFolder(pRenderContext, m_fileSystem.GetCurrentFileDirectoryObject());
			m_openedFileBrowser = false;
		}
	}
	if (m_creatingMaterial)
	{
		m_creatingMaterial = CreateMaterialPopup(m_fileSystem.GetCurrentFilePath(), m_createResourceName, m_createMaterialType);

		if (!m_creatingMaterial)
		{
			resourceManager->GetMaterialManager()->CreateMaterial(m_fileSystem.GetCurrentFilePath(), m_createResourceName, (eMaterialType)m_createMaterialType);
			m_fileSystem.ReloadFolder(m_fileSystem.GetCurrentFilePath());
			m_createResourceName[0] = 0;
		}
	}

	ImVec2 regionAvailable = ImGui::GetContentRegionAvail();
	regionAvailable.x *= .8f;
	ImGui::BeginChild("Asset Browser", regionAvailable, true, ImGuiWindowFlags_MenuBar);

	// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Import"))
		{
			if (ImGui::MenuItem("Texture", NULL, &m_openedFileBrowser))
				fileBrowser->Init(&m_textureFileBrowserData);
			if (ImGui::MenuItem("Shader", NULL, &m_openedFileBrowser))
				fileBrowser->Init(&m_shaderFileBrowserData);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Create"))
		{
			ImGui::MenuItem("Material", NULL, &m_creatingMaterial);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
		ImGui::BeginChild("ChildL", ImVec2(ImGui::GetContentRegionAvail().x * 0.2f, ImGui::GetContentRegionAvail().y * 0.9f), false, window_flags);
	}

	ImGui::BeginDisabled();
	ImGui::Button("Create Folder");
	ImGui::EndDisabled();

	if (ImGuiHelpers::DirectoryPanelLogic(&m_fileSystem, m_fileSystem.GetBaseDepth()))
	{
		m_currentFileDirectoryOpened = m_fileSystem.GetCurrentFileDirectoryObject();
		InitFolder(pRenderContext, m_fileSystem.GetCurrentFileDirectoryObject());
	}

	ImGui::EndChild();
	ImGui::SameLine();

	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
		ImGui::BeginChild("ChildR", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.9f), true, window_flags);
	}

	if (GrowingArray<SelectAbleFileObject>* currentDirectory = m_fileSystem.GetCurrentFileDirectory())
	{
		ResourceRegistry& reg = GetResourceRegistry();
		float sizeX = ImGui::GetContentRegionAvail().x;
		if (sizeX < 1.f)
			sizeX = 1.f;
		const uint32 xRegionAvailable = sizeX < ASSET_SIZE ? sizeX : sizeX / ASSET_SIZE;
		for (size_t i = 0; i < currentDirectory->Size(); i++)
		{
			SelectAbleFileObject& currentObject = (*currentDirectory)[i];
			const bool wasSelected = currentObject.m_selected;
			if (currentObject.m_fileObject.IsDirectory())
			{
				// Add so that one can jump in directory here
				if (RenderAssetPreview(currentObject, m_folderTexture.m_texture, false))
				{
					if (m_fileSystem.SetCurrentFileDirectory(currentObject.m_fileObject))
					{
						m_currentFileDirectoryOpened = m_fileSystem.GetCurrentFileDirectoryObject();
						InitFolder(pRenderContext, m_fileSystem.GetCurrentFileDirectoryObject());
					}
					else
					{
						m_fileSystem.JumpToDepth(currentObject.m_fileObject.GetDirectoryLevel());
					}
				}
			}
			else if (StringCompare(currentObject.m_fileObject.Extension(), L"mat"))
			{
				RenderAssetPreview(currentObject, m_materialIconTexture.m_texture, reg.GetResourceMetaInformation(ResourceType::Material, m_fileSystem.GetCurrentFilePath() + currentObject.m_fileObject));

				if (!wasSelected && currentObject.m_selected)
				{
					DeselectAllObjects(currentDirectory);
					currentObject.m_selected = true;
					m_currentlySelectedMaterialResource.m_fileObject = &currentObject;
					m_resourceManager->GetMaterialManager()->LoadMaterialMetaData( m_fileSystem.GetCurrentFilePath() + currentObject.m_fileObject, m_currentlySelectedMaterialResource.m_metaResource);
					m_currentlySelectedMaterialResource.m_materialObject = m_resourceManager->GetMaterialManager()->LoadMaterialSerializeableInstance(m_fileSystem.GetCurrentFilePath() + currentObject.m_fileObject);

					contextObject->SetCurrentContextObject(ImGuiContextsType::Material, &m_currentlySelectedMaterialResource);
				}
				if (wasSelected && !currentObject.m_selected && contextObject->GetCurrentContextObject())
				{
					contextObject->DeselectContext();
					m_currentlySelectedMaterialResource.m_metaResource = MetaResource();
				}
			}
			else if (StringCompare(currentObject.m_fileObject.Extension(), L"shr"))
			{
				RenderAssetPreview(currentObject, m_materialIconTexture.m_texture, reg.GetResourceMetaInformation(ResourceType::Shader, m_fileSystem.GetCurrentFilePath() + currentObject.m_fileObject));

				if (!wasSelected && currentObject.m_selected)
				{
					DeselectAllObjects(currentDirectory);
					currentObject.m_selected = true;
					m_currentlySelectedShaderResource.m_pFileObject = &currentObject;
					m_currentlySelectedShaderResource.m_metaResource = m_resourceManager->GetMaterialManager()->LoadShaderMetaData(m_fileSystem.GetCurrentFilePath() + currentObject.m_fileObject);
					m_currentlySelectedShaderResource.m_pShader = m_resourceManager->GetMaterialManager()->GetCompiledLoadedShader(m_currentlySelectedShaderResource.m_metaResource.GetGUID());

					contextObject->SetCurrentContextObject(ImGuiContextsType::Shader, &m_currentlySelectedShaderResource);
				}
				if (wasSelected && !currentObject.m_selected && contextObject->GetCurrentContextObject())
				{
					contextObject->DeselectContext();
					m_currentlySelectedShaderResource.m_metaResource = MetaResource();
				}
			}
			if ((i + 1) % xRegionAvailable != 0)
				ImGui::SameLine();
		}
	}
	const uint32 directoryIndex = m_currentFileDirectoryOpened.GetDirectoryLevel() - m_fileSystem.GetBaseDepth();
	GrowingArray<TextureFolder>& texturesLoadedInFolder = m_ImGuiTextureResources[directoryIndex];
	for (size_t i = 0; i < texturesLoadedInFolder.Size(); i++)
	{
		TextureFolder& folder = texturesLoadedInFolder[i];
		if (m_currentFileDirectoryOpened == folder.owningFileObject)
		{
			for (size_t iTexture = 0; iTexture < folder.folderTextures.Size(); iTexture++)
			{
				const bool wasSelected = folder.folderTextures[iTexture].m_fileObject.m_selected;
				RenderAssetPreview(folder.folderTextures[iTexture].m_fileObject, folder.folderTextures[iTexture].m_texture, &folder.folderTextures[iTexture].m_texture->metaDataOfResource);

				if (ImGui::GetContentRegionAvail().x > ASSET_SIZE)
					ImGui::SameLine();
				// remove
				if (wasSelected && !folder.folderTextures[iTexture].m_fileObject.m_selected)
				{
					m_selectedTextureAssets.RemoveCyclic(folder.folderTextures[iTexture]);
					if (contextObject->GetCurrentContextType() == ImGuiContextsType::Texture)
					{
						if (!m_selectedTextureAssets.Empty())
							contextObject->SetCurrentContextObject(ImGuiContextsType::Texture, &m_selectedTextureAssets.GetLast());
						else
							contextObject->DeselectContext();
					}
				}
				// Add to selection
				if (!wasSelected && folder.folderTextures[iTexture].m_fileObject.m_selected)
				{
					// Set context object
					m_selectedTextureAssets.Add(folder.folderTextures[iTexture]);
					contextObject->SetCurrentContextObject(ImGuiContextsType::Texture, &m_selectedTextureAssets.GetLast());
				}
			}
			break;
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();


	ImGui::EndChild();
}

void Hail::ImGuiAssetBrowser::InitFileBrowser()
{
	if (m_fileBrowsersAreInited)
	{
		return;
	}
	m_fileBrowsersAreInited = true;
	m_textureFileBrowserData.allowMultipleSelection = true;
	m_textureFileBrowserData.extensionsToSearchFor = { "tga" };
	m_textureFileBrowserData.pathToBeginSearchingIn = FilePath::GetResourceSourceDirectory();
	m_shaderFileBrowserData.allowMultipleSelection = true;
	m_shaderFileBrowserData.extensionsToSearchFor = { "cmp", "vert", "amp", "msh", "frag" };
	m_shaderFileBrowserData.pathToBeginSearchingIn = FilePath::GetResourceSourceDirectory();
}

void Hail::ImGuiAssetBrowser::InitCommonData(RenderContext* pRenderContext)
{
	if (m_inited)
	{
		return;
	}
	m_inited = true;
	
	const FilePath baseResourcePath = FilePath::GetCurrentWorkingDirectory() + L"resources/";
	m_fileSystem.SetFilePathAndInit(baseResourcePath, { "txr" });
	pRenderContext->StartTransferPass();
	m_folderTexture.m_texture = m_resourceManager->GetTextureManager()->CreateImGuiTextureResource(pRenderContext, baseResourcePath.Parent() + L"editorResources/folderTexture.txr", m_resourceManager->GetRenderingResourceManager(), &m_folderTexture.m_TextureProperties);
	m_materialIconTexture.m_texture = m_resourceManager->GetTextureManager()->CreateImGuiTextureResource(pRenderContext, baseResourcePath.Parent() + L"editorResources/materialIconTexture.txr", m_resourceManager->GetRenderingResourceManager(), &m_materialIconTexture.m_TextureProperties);
	pRenderContext->EndTransferPass();

	m_currentFileDirectoryOpened = m_fileSystem.GetCurrentFileDirectoryObject();
	InitFileBrowser();
	InitFolder(pRenderContext, m_fileSystem.GetCurrentFileDirectoryObject());

	m_createMaterialType = (uint32)eMaterialType::SPRITE;
}

void Hail::ImGuiAssetBrowser::InitFolder(RenderContext* pRenderContext, const FileObject& fileObject)
{
	const GrowingArray<SelectAbleFileObject>* directory = m_fileSystem.GetFileDirectory(fileObject);
	if (!directory)
		return;

	const uint32 directoryLevel = fileObject.GetDirectoryLevel() - m_fileSystem.GetBaseDepth();
	GrowingArray<TextureFolder>& foldersAtLevel = m_ImGuiTextureResources[directoryLevel];
	RelativeFilePath path = RelativeFilePath(m_fileSystem.GetCurrentFilePath());

	//check if folder already has been initialized and created, if it has, clear it
	for (size_t i = 0; i < foldersAtLevel.Size(); i++)
	{
		if (foldersAtLevel[i].owningFileObject == fileObject)
		{
			for (size_t j = 0; j < foldersAtLevel[i].folderTextures.Size(); j++)
			{
				m_resourceManager->GetTextureManager()->DeleteImGuiTextureResource(foldersAtLevel[i].folderTextures[j].m_texture);
				SAFEDELETE(foldersAtLevel[i].folderTextures[j].m_texture);
			}
			foldersAtLevel[i].folderTextures.RemoveAll();
			foldersAtLevel.RemoveCyclicAtIndex(i);
		}
	}

	TextureFolder textureFolder;
	textureFolder.owningFileObject = fileObject;
	for (size_t fileI = 0; fileI < directory[0].Size(); fileI++)
	{
		const SelectAbleFileObject& fileObject = directory[0][fileI];

		if (fileObject.m_fileObject.IsFile() && StringCompare(fileObject.m_fileObject.Extension(), L"txr"))
		{
			TextureContextAsset textureAsset;
			pRenderContext->StartTransferPass();
			textureAsset.m_filePath = m_fileSystem.GetCurrentFilePath() + fileObject.m_fileObject;
			textureAsset.m_texture = m_resourceManager->GetTextureManager()->CreateImGuiTextureResource(pRenderContext, textureAsset.m_filePath, m_resourceManager->GetRenderingResourceManager(), &textureAsset.m_TextureProperties);
			pRenderContext->EndTransferPass();
			textureAsset.m_fileObject = fileObject;
			textureFolder.folderTextures.Add(textureAsset);
		}
	}
	foldersAtLevel.Add(textureFolder);
}

void Hail::ImGuiAssetBrowser::ImportTextureLogic()
{
	for (size_t i = 0; i < m_textureFileBrowserData.objectsToSelect.Size(); i++)
	{
		m_fileSystem.ReloadFolder(m_resourceManager->GetTextureManager()->ImportTextureResource(m_textureFileBrowserData.objectsToSelect[i]));
	}
	m_textureFileBrowserData.objectsToSelect.RemoveAll(); 
}

void Hail::ImGuiAssetBrowser::ImportShaderResourceLogic()
{
	for (size_t i = 0; i < m_shaderFileBrowserData.objectsToSelect.Size(); i++)
	{
		m_fileSystem.ReloadFolder(m_resourceManager->GetMaterialManager()->ImportShaderResource(m_shaderFileBrowserData.objectsToSelect[i]));
	}
	m_shaderFileBrowserData.objectsToSelect.RemoveAll();
}

void Hail::ImGuiAssetBrowser::DeselectAllObjects(GrowingArray<SelectAbleFileObject>* pCurrentDirectory)
{
	for (size_t i = 0; i < pCurrentDirectory->Size(); i++)
	{
		SelectAbleFileObject& currentObject = (*pCurrentDirectory)[i];
		currentObject.m_selected = false;
	}
}

#pragma optimize("", on)

