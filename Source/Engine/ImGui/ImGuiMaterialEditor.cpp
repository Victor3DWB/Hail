#include "Engine_PCH.h"
#include "ImGuiMaterialEditor.h"
#include "ImGuiFileBrowser.h"

#include "imgui.h"

#include "Resources\MaterialResources.h"
#include "ImGuiHelpers.h"

#include "HailEngine.h"
#include "Resources\ResourceRegistry.h"
#include "Resources\ResourceManager.h"
#include "Resources\MaterialManager.h"
#include "Resources\TextureManager.h"

namespace Hail
{
	bool g_inited = false;
	ImGuiFileBrowserData g_textureFileBrowserData;
	ImGuiFileBrowserData g_shaderFileBrowserData;

	void InitCommonData()
	{
		if (g_inited)
		{
			return;
		}
		g_inited = true;

		g_textureFileBrowserData.allowMultipleSelection = false;
		g_textureFileBrowserData.objectsToSelect.Prepare(1);
		g_textureFileBrowserData.extensionsToSearchFor = { "txr" };
		g_textureFileBrowserData.pathToBeginSearchingIn = FilePath::GetCurrentWorkingDirectory();

		g_shaderFileBrowserData.allowMultipleSelection = false;
		g_shaderFileBrowserData.objectsToSelect.Prepare(1);
		g_shaderFileBrowserData.extensionsToSearchFor = { "shr" };
		g_shaderFileBrowserData.pathToBeginSearchingIn = FilePath::GetCurrentWorkingDirectory();
	}
}

Hail::ImGuiMaterialEditor::ImGuiMaterialEditor()
{
	m_bOpenFileBrowser = false;
	m_selectedTextureSlot = 0;
}

void Hail::ImGuiMaterialEditor::RenderImGuiCommands(ImGuiFileBrowser* fileBrowser, ResourceManager& resourceManager, ImGuiContext* contextObject, bool* closeButton)
{
	InitCommonData();
	ImGui::Begin("Material Editor", closeButton);
	ImGui::Text("Material Name: %s", m_materialToEdit.m_fileObject->m_fileObject.Name().ToCharString());

	if (m_materialToEdit.m_materialObject.m_baseMaterialType == eMaterialType::COUNT)
	{
		ImGui::End();
		return;
	}

	bool finishedBrowsing = false;
	MetaResource selectedShaderMetaResource;
	if (m_bOpenFileBrowser)
	{
		fileBrowser->RenderImGuiCommands(finishedBrowsing);
		if (finishedBrowsing)
		{
			if (!g_textureFileBrowserData.objectsToSelect.Empty())
			{
				MetaResource textureMetaData;
				resourceManager.GetTextureManager()->LoadTextureMetaData(g_textureFileBrowserData.objectsToSelect[0], textureMetaData);
				m_materialToEdit.m_materialObject.m_textureHandles[m_selectedTextureSlot] = textureMetaData.GetGUID();
				m_selectedTextureSlot = 0;
				g_textureFileBrowserData.objectsToSelect.RemoveAll();
			}
			if (!g_shaderFileBrowserData.objectsToSelect.Empty())
			{
				selectedShaderMetaResource = resourceManager.GetMaterialManager()->LoadShaderMetaData(g_shaderFileBrowserData.objectsToSelect[0]);
				g_shaderFileBrowserData.objectsToSelect.RemoveAll();
			}
			m_bOpenFileBrowser = false;
		}
	}

	ImGui::Separator();

	m_materialToEdit.m_materialObject.m_baseMaterialType = (eMaterialType)ImGuiHelpers::GetMaterialTypeComboBox((uint32)m_materialToEdit.m_materialObject.m_baseMaterialType);

	if (ImGui::BeginCombo("Blend Mode", ImGuiHelpers::GetMaterialBlendModeFromEnum(m_materialToEdit.m_materialObject.m_blendMode)))
	{

		for (uint8 i = 0; i < (uint8)eBlendMode::COUNT; i++)
		{
			bool is_selected = (eBlendMode)i == m_materialToEdit.m_materialObject.m_blendMode;
			if (ImGui::Selectable(ImGuiHelpers::GetMaterialBlendModeFromEnum((eBlendMode)i), is_selected))
			{
				m_materialToEdit.m_materialObject.m_blendMode = (eBlendMode)i;
				is_selected = true;
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	// cutout threshold
	{
		const bool hasCutout = m_materialToEdit.m_materialObject.m_blendMode != eBlendMode::None;
		float cutoutThreshold = (m_materialToEdit.m_materialObject.m_extraData & 0xff) / 255.f;
		if (!hasCutout)
			ImGui::BeginDisabled();
		ImGui::SliderFloat("Cutout threshhold", &cutoutThreshold, 0.f, 1.f);
		uint8 cutoutValue = (uint8)(cutoutThreshold * 255.f);
		m_materialToEdit.m_materialObject.m_extraData = m_materialToEdit.m_materialObject.m_extraData & 0xff00 | cutoutValue;
		if (!hasCutout)
			ImGui::EndDisabled();
	}

	ImGui::Separator();
	MaterialManager* pMaterialManager = resourceManager.GetMaterialManager();
	uint32 numberOfTextureSlots = 0;
	VectorOnStack<ShaderDecoration, 8> textures;
	for (size_t i = 0; i < 2; i++)
	{
		CompiledShader* pShaderData = nullptr;
		if (m_materialToEdit.m_materialObject.m_shaders[i].m_id != GuidZero)
		{
			pShaderData = pMaterialManager->GetCompiledLoadedShader(m_materialToEdit.m_materialObject.m_shaders[i].m_id);
		}
		if (m_materialToEdit.m_materialObject.m_shaders[i].m_id == GuidZero || !pShaderData)
		{
			pShaderData = pMaterialManager->GetDefaultCompiledLoadedShader(m_materialToEdit.m_materialObject.m_shaders[i].m_type);
		}

		if (m_selectedShader == i && selectedShaderMetaResource.GetGUID() != GuidZero)
		{
			CompiledShader* pSelectedShader = pMaterialManager->GetCompiledLoadedShader(selectedShaderMetaResource.GetGUID());

			if (!pSelectedShader)
			{
				pSelectedShader = pMaterialManager->LoadShaderResource(selectedShaderMetaResource.GetGUID());
			}

			if (pSelectedShader && pMaterialManager->IsShaderValidWithMaterialType(m_materialToEdit.m_materialObject.m_baseMaterialType,
				(eShaderStage)pSelectedShader->header.shaderType, pSelectedShader->reflectedShaderData))
			{
				pShaderData = pSelectedShader;
				m_materialToEdit.m_materialObject.m_shaders[i].m_id = selectedShaderMetaResource.GetGUID();
			}
			else
			{
				// TODO: show error
			}

			m_selectedShader = 0;
		}

		const VectorOnStack<uint32, 16>& shaderTexturesIndices = pShaderData->reflectedShaderData.m_setDecorations[InstanceDomain][(uint32)eDecorationType::SampledImage].m_indices;
		const StaticArray<ShaderDecoration, 16>& shaderTextures = pShaderData->reflectedShaderData.m_setDecorations[InstanceDomain][(uint32)eDecorationType::SampledImage].m_decorations;
		for (size_t iTexture = 0; iTexture < shaderTexturesIndices.Size(); iTexture++)
		{
			textures.Add(shaderTextures[shaderTexturesIndices[iTexture]]);
			numberOfTextureSlots++;
		}
		ImGui::Text("Shader Type: %s", ImGuiHelpers::GetShaderTypeFromEnum((eShaderStage)pShaderData->header.shaderType));
		ImGui::Text("Shader Name: %s", pShaderData->shaderName.Data());
		if (ImGui::Button(String64::Format("Change Shader: %s", ImGuiHelpers::GetShaderTypeFromEnum((eShaderStage)pShaderData->header.shaderType))) && fileBrowser->Init(&g_shaderFileBrowserData))
		{
			m_bOpenFileBrowser = true;
			m_selectedShader = i;
		}
		ImGui::Separator();
	}

	for (size_t i = 0; i < numberOfTextureSlots; i++)
	{
		
		ImGui::Text("Texture Binding point: %i", textures[i].m_bindingLocation);
		ImGui::SameLine();
		if (m_materialToEdit.m_materialObject.m_textureHandles[i] == GuidZero)
			ImGui::Text("None");
		else
			ImGui::Text(GetResourceRegistry().GetProjectPath(ResourceType::Texture, m_materialToEdit.m_materialObject.m_textureHandles[i]).Object().Name().ToCharString());
		ImGui::SameLine();
		if (ImGui::Button(String64::Format("Replace Texture %i", i)) && fileBrowser->Init(&g_textureFileBrowserData))
		{
			m_bOpenFileBrowser = true;
			m_selectedTextureSlot = i;
		}
	}

	if (ImGui::Button("Save Material"))
	{
		resourceManager.GetMaterialManager()->ExportMaterial(m_materialToEdit);
		if (contextObject->GetCurrentContextObject() && contextObject->GetCurrentContextType() == ImGuiContextsType::Material)
		{
			MaterialResourceContextObject& material = *(MaterialResourceContextObject*)contextObject->GetCurrentContextObject();
			if (material.m_metaResource.GetGUID() == m_materialToEdit.m_metaResource.GetGUID())
				contextObject->SetCurrentContextObject(ImGuiContextsType::Material,&m_materialToEdit);
		}
	}
	//TODO: reload to original state
	//ImGui::SameLine();
	//if (ImGui::Button("Reload Material"))
	//{
	//
	//}

	ImGui::End();
}

void Hail::ImGuiMaterialEditor::SetMaterialAsset(ImGuiContext* context)
{
	if (context->GetCurrentContextType() != ImGuiContextsType::Material || !context->GetCurrentContextObject())
		return;

	m_materialToEdit = *(MaterialResourceContextObject*)context->GetCurrentContextObject();

}

