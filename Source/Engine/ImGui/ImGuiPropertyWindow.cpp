#include "Engine_PCH.h"
#include "ImGuiPropertyWindow.h"

#include "imgui.h"
#include "ImGuiContext.h"

#include "HailEngine.h"
#include "Resources\ResourceRegistry.h"

#include "Utility\StringUtility.h"

#include "Resources\TextureManager.h"
#include "Resources\ResourceManager.h"
#include "Resources\MaterialManager.h"
#include "ImGuiHelpers.h"


using namespace Hail;

namespace
{
	bool LocalDoesTypeRequire2Shaders(eMaterialType materialType)
	{
		switch (materialType)
		{
		case eMaterialType::SPRITE:
		case eMaterialType::MODEL3D:
		case eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
			return true;
		case eMaterialType::COUNT:
			break;
		default:
			break;
		}
	}

	void RenderShaderProperties(CompiledShader* pShader)
	{
		ImGui::Separator();
		ImGui::Text("Shader Name: %s", pShader->shaderName.Data());
		ImGui::Text("Shader Type: %s", ImGuiHelpers::GetShaderTypeFromEnum((eShaderStage)pShader->header.shaderType));
	}

	ImGuiPropertyWindowReturnValue RenderTextureProperty(Hail::ImGuiContext* context)
	{
		ImGui::Text("Texture Asset:");
		ImGui::Separator();
		if (!context->GetCurrentContextObject())
			return ImGuiPropertyWindowReturnValue::NoOp;

		TextureContextAsset& texture = *(TextureContextAsset*)context->GetCurrentContextObject();

		ImGui::Text("Name: %s", texture.m_fileObject.m_fileObject.Name().ToCharString());
		ImGui::Text("Texture Format: %s", GetSerializeableTextureTypeAsText((eTextureSerializeableType)texture.m_TextureProperties.textureType));
		ImGui::Text("Width: %i Height: %i", texture.m_TextureProperties.width, texture.m_TextureProperties.height);
		if (ImGui::Button("Reload Texture"))
			return ImGuiPropertyWindowReturnValue::ReloadResource;
	}

	ImGuiPropertyWindowReturnValue RenderMaterialProperty(Hail::ImGuiContext* context)
	{
		MaterialResourceContextObject& material = *(MaterialResourceContextObject*)context->GetCurrentContextObject();
		ImGui::Text("Material Asset\nName: %s", material.m_fileObject->m_fileObject.Name().ToCharString());

		ImGui::Text("Material Type: %s", ImGuiHelpers::GetMaterialTypeStringFromEnum(material.m_materialObject.m_baseMaterialType));
		ImGui::Text("Blend Mode: %s", ImGuiHelpers::GetMaterialBlendModeFromEnum(material.m_materialObject.m_blendMode));

		MaterialManager* pMatManager = context->GetResourceManager()->GetMaterialManager();
		const uint32 numberOfShaders = LocalDoesTypeRequire2Shaders(material.m_materialObject.m_baseMaterialType) ? 2 : 1;
		for (size_t iShader = 0; iShader < numberOfShaders; iShader++)
		{
			if (material.m_materialObject.m_shaders[iShader].m_id == GuidZero)
			{
				RenderShaderProperties(pMatManager->GetDefaultCompiledLoadedShader(material.m_materialObject.m_shaders[iShader].m_type));
			}
			else
			{
				CompiledShader* pShader = pMatManager->GetCompiledLoadedShader(material.m_materialObject.m_shaders[iShader].m_id);
				if (!pShader)
				{
					pShader = pMatManager->LoadShaderResource(material.m_materialObject.m_shaders[iShader].m_id);
					if (!pShader)
						pShader = pMatManager->GetDefaultCompiledLoadedShader(material.m_materialObject.m_shaders[iShader].m_type);
				}
				RenderShaderProperties(pShader);
			}
		}
		ImGui::Separator();
		for (size_t i = 0; i < MAX_TEXTURE_HANDLES; i++)
		{
			ImGui::Text("Texture slot %i:", i + 1);
			ImGui::SameLine();
			if (material.m_materialObject.m_textureHandles[i] == GuidZero)
			{
				ImGui::Text("No texture");
				continue;
			}
			const FilePath texturePath = GetResourceRegistry().GetProjectPath(ResourceType::Texture, material.m_materialObject.m_textureHandles[i]);
			if (!texturePath.IsValid())
				continue;
			ImGui::Text("%s", texturePath.Object().Name().ToCharString());
		}

		ImGui::Separator();

		if (ImGui::Button("Edit Material"))
			return ImGuiPropertyWindowReturnValue::OpenMaterialWindow;

		return ImGuiPropertyWindowReturnValue::NoOp;
	}

	ImGuiPropertyWindowReturnValue RenderShaderProperty(Hail::ImGuiContext* context)
	{
		ShaderResourceContextObject& shader = *(ShaderResourceContextObject*)context->GetCurrentContextObject();

		if (!shader.m_pShader)
		{
			ImGui::Text("Shader Not Loaded");
			ImGui::Text("Shader Asset\nName: %s", shader.m_pFileObject->m_fileObject.Name().ToCharString());
			return ImGuiPropertyWindowReturnValue::NoOp;
		}


		ImGui::Text("Shader Asset\nName: %s", shader.m_pFileObject->m_fileObject.Name().ToCharString());
		ImGui::Text("Shader Type: %s", ImGuiHelpers::GetShaderTypeFromEnum((eShaderStage)shader.m_pShader->header.shaderType));

		return ImGuiPropertyWindowReturnValue::NoOp;
	}
}


ImGuiPropertyWindowReturnValue ImGuiPropertyWindow::RenderImGuiCommands(ImGuiFileBrowser* fileBrowser, ImGuiContext* context)
{
	ImGui::BeginChild("Properties", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_MenuBar);
	ImGuiPropertyWindowReturnValue returnResult = ImGuiPropertyWindowReturnValue::NoOp;

	if (context->GetCurrentContextType() == ImGuiContextsType::None)
		ImGui::Text("No item selected");
	else if (context->GetCurrentContextType() == ImGuiContextsType::Texture)
		returnResult = RenderTextureProperty(context);
	else if (context->GetCurrentContextType() == ImGuiContextsType::Material)
		returnResult = RenderMaterialProperty(context);
	else if (context->GetCurrentContextType() == ImGuiContextsType::Shader)
		RenderShaderProperty(context);

	ImGui::EndChild();
	return returnResult;
}
