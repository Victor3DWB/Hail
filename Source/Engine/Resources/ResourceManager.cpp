#include "Engine_PCH.h"
#include "ResourceManager.h"
#include "glm\geometric.hpp"
#include "MathUtils.h"
#include "Renderer.h"
#include "RenderCommands.h"
#include "Timer.h"
#include "Input\InputHandler.h"

#include "ResourceRegistry.h"
#include "HailEngine.h"

#include "Resources_Materials\ShaderBufferList.h"
#include "Rendering\RenderContext.h"

#ifdef PLATFORM_WINDOWS
#include "windows\VulkanInternal\VlkResourceManager.h"
#include "windows\VulkanInternal\VlkSwapChain.h"
#include "Resources\Vulkan\VlkTextureManager.h"
#include "Resources\Vulkan\VlkMaterialManager.h"
#endif

namespace Hail
{
	Mesh CreateUnitCube();
	Mesh CreateUnitSphere();
	Mesh CreateUnitCylinder();
	void BuildCylinderTopCap(float topRadius, float height, uint32_t sliceCount, GrowingArray<VertexModel>& meshVertices, GrowingArray<uint32_t>& meshIndeces);
	void BuildCylinderBottomCap(float bottomRadius, float height, uint32_t sliceCount, GrowingArray<VertexModel>& meshVertices, GrowingArray<uint32_t>& meshIndeces);
}


Hail::ResourceManager::ResourceManager()
{
	m_unitCube = CreateUnitCube();
	m_unitSphere = CreateUnitSphere();
	m_unitCylinder = CreateUnitCylinder();
}

bool Hail::ResourceManager::InitResources(RenderingDevice* renderingDevice, RenderContext* pRenderContext, eResolutions targetRes, eResolutions startupWindowRes, ErrorManager* pErrorManager)
{
	m_renderDevice = renderingDevice;

#ifdef PLATFORM_WINDOWS
	m_textureManager = new VlkTextureResourceManager(m_renderDevice);
	m_materialManager = new VlkMaterialManager();
	m_renderingResourceManager = new VlkRenderingResourceManager();
	m_swapChain = new VlkSwapChain(m_textureManager);
#endif
	SetTargetResolution(targetRes);
	SetWindowResolution(startupWindowRes);

	m_swapChain->Init(m_renderDevice);
	if (!m_renderingResourceManager->Init(m_renderDevice, m_swapChain))
	{
		pErrorManager->AddString("Failed to Initialize Rendering Resource Manager.");
	}
	m_textureManager->Init(pRenderContext);
	m_materialManager->Init(m_renderDevice, m_textureManager, m_renderingResourceManager, m_swapChain, pErrorManager);
	m_pMainPassFrameBufferTexture = m_textureManager->FrameBufferTexture_Create("MainRenderPass", ResolutionFromEnum(m_targetResolution), eTextureFormat::R8G8B8A8_UNORM, TEXTURE_DEPTH_FORMAT::D16_UNORM);
	
	if (!m_pMainPassFrameBufferTexture)
	{
		pErrorManager->AddString("Failed to create main frame buffer for rendering.");
		pErrorManager->AddErrors(EStartupErrors::InitDefaultMaterial, EErrorType::Startup);
		return false;
	}

	for (uint32_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
	{
		m_textureManager->RegisterEngineTexture(m_pMainPassFrameBufferTexture->GetColorTexture(i), m_pMainPassFrameBufferTexture->GetColorTextureView(i), eDecorationSets::MaterialTypeDomain, (uint32)eMaterialTextures::FullscreenPassTarget, i);
	}

	for (uint32_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
	{
		if(!m_materialManager->InitDefaultMaterial(eMaterialType::SPRITE, m_pMainPassFrameBufferTexture, false, i))
		{
			pErrorManager->AddString("Failed to load sprite material.");
			pErrorManager->AddErrors(EStartupErrors::InitDefaultMaterial, EErrorType::Startup);
			return false;
		}
		if(!m_materialManager->InitDefaultMaterial(eMaterialType::FULLSCREEN_PRESENT_LETTERBOX, m_swapChain->GetFrameBufferTexture(), false, i))
		{
			pErrorManager->AddString("Failed to load Fullscreen present material.");
			pErrorManager->AddErrors(EStartupErrors::InitDefaultMaterial, EErrorType::Startup);
			return false;
		}
		if(!m_materialManager->InitDefaultMaterial(eMaterialType::MODEL3D, m_pMainPassFrameBufferTexture, false, i))
		{
			pErrorManager->AddString("Failed to load Model3D material.");
			pErrorManager->AddErrors(EStartupErrors::InitDefaultMaterial, EErrorType::Startup);
			return false;
		}
	}
	m_materialManager->InitDefaultMaterialInstances();

	return true;
}

void Hail::ResourceManager::ClearAllResources(RenderingDevice* pRenderDevice)
{
	m_textureManager->ClearAllResources();
	m_materialManager->ClearAllResources();
	m_renderingResourceManager->ClearAllResources();
	m_pMainPassFrameBufferTexture->ClearResources(pRenderDevice);
	SAFEDELETE(m_pMainPassFrameBufferTexture);
	m_swapChain->DestroySwapChain(pRenderDevice);
}

void Hail::ResourceManager::SetTargetResolution(eResolutions targetResolution)
{
	m_swapChain->SetTargetResolution(ResolutionFromEnum(targetResolution));
	m_targetResolution = targetResolution;
}

void Hail::ResourceManager::SetWindowResolution(eResolutions targetResolution)
{
	m_swapChain->SetWindowResolution(ResolutionFromEnum(targetResolution));
}

void Hail::ResourceManager::SetReloadOfAllResources()
{
	if (m_reloadEverything || m_reloadAllTextures)
	{
		return;
	}
	m_reloadEverything = true;
	m_reloadFrameCounter = 0;
}

void Hail::ResourceManager::SetReloadOfAllTextures()
{
	if (m_reloadEverything || m_reloadAllTextures)
	{
		return;
	}
	m_reloadAllTextures = true;
	m_reloadFrameCounter = 0;
}

void Hail::ResourceManager::ReloadResources()
{
	if (!m_reloadEverything && !m_reloadAllTextures)
	{
		return;
	}

	m_frameInFlightIndex = m_swapChain->GetFrameInFlight();
	if (m_reloadEverything)
	{
		m_textureManager->ReloadAllTextures(m_frameInFlightIndex);
		m_materialManager->ReloadAllMaterials(m_frameInFlightIndex);
	}
	if (m_reloadAllTextures)
	{
		m_textureManager->ReloadAllTextures(m_frameInFlightIndex);
		m_materialManager->ReloadAllMaterialInstances(m_frameInFlightIndex);
	}

	if (++m_reloadFrameCounter == MAX_FRAMESINFLIGHT)
	{
		m_reloadEverything = false;
		m_reloadAllTextures = false;
	}
}

void Hail::ResourceManager::LoadMaterialResource(GUID guid)
{
	if (guid == GuidZero)
	{
		H_ERROR("Loading material failed, guid is zero.");
		return;
	}

	ResourceRegistry& reg = GetResourceRegistry();
	if (!reg.GetIsResourceImported(ResourceType::Material, guid))
		return;

	if (!m_materialManager->StaggeredMaterialLoad(guid))
	{
		// TODO: Add resource load error type for registry.
		//reg.SetResourceLoadError
		H_ERROR(StringL::Format("Failed to load material: %s", reg.GetResourceName(ResourceType::Material, guid).Data()));
		reg.SetResourceLoadFailed(ResourceType::Material, guid);
	}
	else
	{
		reg.SetResourceUnloaded(ResourceType::Material, guid);
	}
}

Hail::uint32 Hail::ResourceManager::GetMaterialInstanceHandle(GUID guid) const
{
	return m_materialManager->GetMaterialInstanceHandle(guid);
}

void Hail::ResourceManager::UpdateRenderBuffers(RenderCommandPool& renderPool, RenderContext* pRenderContext, Timer* timer)
{
	// Upload texture data to the GPU
	pRenderContext->StartTransferPass();
	m_textureManager->Update(pRenderContext);
	m_materialManager->Update();

	BufferObject* instance2DBuffer = m_renderingResourceManager->GetGlobalBuffer(eDecorationSets::MaterialTypeDomain, eBufferType::structured, (uint32)eMaterialBuffers::instanceBuffer2D);
	pRenderContext->UploadDataToBuffer(instance2DBuffer, renderPool.m_2DRenderCommands.Data(), sizeof(RenderCommand2DBase) * renderPool.m_2DRenderCommands.Size());

	BufferObject* pSpriteDataBuffer = m_renderingResourceManager->GetGlobalBuffer(eDecorationSets::MaterialTypeDomain, eBufferType::structured, (uint32)eMaterialBuffers::spriteDataBuffer);
	pRenderContext->UploadDataToBuffer(pSpriteDataBuffer, renderPool.m_spriteData.Data(), sizeof(RenderData_Sprite) * renderPool.m_spriteData.Size());
	
	//Common GPU buffers___

	const float deltaTime = timer->GetDeltaTime();
	const float totalTime = timer->GetTotalTime();

	PerFrameUniformBuffer perFrameData{};

	perFrameData.totalTime_horizonLevel.x = totalTime;
	perFrameData.totalTime_horizonLevel.y = 100.f;
	perFrameData.mainRenderResolution = m_swapChain->GetRenderTargetResolution();
	perFrameData.mainWindowResolution = m_swapChain->GetSwapChainResolution();
	perFrameData.renderTargetRes = m_swapChain->GetTargetResolution();
	perFrameData.deltaTime_cameraZoom = glm::vec2(deltaTime, renderPool.camera2D.GetZoom());
	perFrameData.cameraPos = renderPool.camera2D.GetNormalizedPosition();
	perFrameData.mousePos = glm::vec2((float)GetInputHandler().GetInputMap().mouse.mousePos.x / (float)perFrameData.mainWindowResolution.x,
		(float)GetInputHandler().GetInputMap().mouse.mousePos.y / (float)perFrameData.mainWindowResolution.y);

	const bool lmbDown = GetInputHandler().GetInputMap().mouse.keys[(uint32)eMouseMapping::LMB] == (uint8)eInputState::Down;
	const bool rmbDown = GetInputHandler().GetInputMap().mouse.keys[(uint32)eMouseMapping::RMB] == (uint8)eInputState::Down;
	perFrameData.mouseRMBLMBDeltas = glm::vec2(lmbDown, rmbDown);

	perFrameData.playerPosition = renderPool.playerPosition;

	BufferObject* perFrameDataBuffer = m_renderingResourceManager->GetGlobalBuffer(eDecorationSets::GlobalDomain, eBufferType::uniform, (uint32)eGlobalUniformBuffers::frameData);
	pRenderContext->UploadDataToBuffer(perFrameDataBuffer, &perFrameData, sizeof(perFrameData));

	TutorialUniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(1.0f) + totalTime * 0.15f, glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = Transform3D::GetMatrix(renderPool.camera3D.GetTransform());
	ubo.proj = glm::perspective(glm::radians(renderPool.camera3D.GetFov()), 
		(float)(perFrameData.mainRenderResolution.x) / (float)(perFrameData.mainRenderResolution.y), 
		renderPool.camera3D.GetNear(), renderPool.camera3D.GetFar());
	ubo.proj[1][1] *= -1;
	BufferObject* modelViewProjectionBuffer = m_renderingResourceManager->GetGlobalBuffer(eDecorationSets::MaterialTypeDomain, eBufferType::uniform, (uint32)eInstanceUniformBuffer::modelViewProjectionData);
	pRenderContext->UploadDataToBuffer(modelViewProjectionBuffer, &ubo, sizeof(ubo));

	PerCameraUniformBuffer perCameraData{};
	perCameraData.proj = ubo.proj;
	perCameraData.view = ubo.view;
	BufferObject* perViewDataBuffer = m_renderingResourceManager->GetGlobalBuffer(eDecorationSets::GlobalDomain, eBufferType::uniform, (uint32)eGlobalUniformBuffers::viewData);
	pRenderContext->UploadDataToBuffer(perViewDataBuffer, &perCameraData, sizeof(perCameraData));
	pRenderContext->EndTransferPass();
}

void Hail::ResourceManager::ClearFrameData()
{
}

void Hail::ResourceManager::SetSwapchainTargetResolution(glm::uvec2 targetResolution)
{
	m_swapChain->SetTargetResolution(targetResolution);
}

void Hail::ResourceManager::SpriteRenderDataFromGameCommand(const GameCommand_Sprite& commandToCreateFrom, RenderCommand2DBase& baseCommandToFill, RenderData_Sprite& dataToFill)
{
	const TextureResource* defaultTexture = m_textureManager->GetDefaultTexture().m_pTexture;
	const glm::vec2 renderResolution = m_swapChain->GetTargetResolution();
	const MaterialInstance& materialInstance = m_materialManager->GetMaterialInstance(commandToCreateFrom.materialInstanceID, eMaterialType::SPRITE);
	const TextureResource& texture = materialInstance.m_textureHandles[0] != INVALID_TEXTURE_HANDLE ? *m_textureManager->GetTexture(materialInstance.m_textureHandles[0]) : *defaultTexture;
	const float textureAspectRatio = (float)texture.m_properties.width / (float)texture.m_properties.height;

	glm::vec2 spriteScale = commandToCreateFrom.transform.GetScale();
	const glm::vec2 spriteSizeMultiplier = commandToCreateFrom.bSizeRelativeToRenderTarget ? glm::vec2(1.0, 1.0) : commandToCreateFrom.transform.GetScale();
	if (commandToCreateFrom.bSizeRelativeToRenderTarget)
	{
		spriteScale.x *= m_swapChain->GetHorizontalAspectRatio();
	}
	else
	{
		const glm::vec2 scaleMultiplier = glm::vec2(texture.m_properties.width, texture.m_properties.height) / renderResolution.y;
		spriteScale = (spriteScale * 2.0f) * scaleMultiplier;
	}
	const float cutoutThreshhold = materialInstance.m_blendMode == eBlendMode::None ? 0.0f : (float)materialInstance.m_cutoutThreshold / 256.f;

	// The last bit is set to 1 or 0 for if the data should be lerped or not
	baseCommandToFill.m_color = commandToCreateFrom.color;
	baseCommandToFill.m_transform = commandToCreateFrom.transform;
	baseCommandToFill.m_transform.SetScale(spriteScale);
	baseCommandToFill.m_index_materialIndex_flags.u = commandToCreateFrom.index;
	uint32 maskedValue = commandToCreateFrom.materialInstanceID << 16;
	baseCommandToFill.m_index_materialIndex_flags.u |= (maskedValue | LerpCommandFlagMask | IsSpriteFlagMask);

	dataToFill.uvTR_BL = commandToCreateFrom.uvTR_BL;
	dataToFill.pivot_sizeMultiplier = { commandToCreateFrom.pivot.x, commandToCreateFrom.pivot.y, spriteSizeMultiplier.x, spriteSizeMultiplier.y };
	dataToFill.cutoutThreshold_padding = { cutoutThreshhold, 0.f, 0.f, 0.f };

}

Hail::Mesh Hail::CreateUnitCube()
{
	Mesh mesh{};
	float w = 50;
	float h = 50;
	float d = 50;

	//Create Vertices
	const int amountOfVertices = 24;
	mesh.vertices.Prepare(24);
	mesh.vertices.Fill();
	VertexModel v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24;

	glm::vec3 toBinormal;
	//Back
	v1.pos = { -w, -h, -d };
	v1.color = { 0.5, 0.5, 0, 1.0 };
	v1.texCoord1 = { 0, 1 };
	v1.norm = { 0, 0, -1 };
	v1.tangent = { 1, 0, 0 };
	toBinormal = { v1.norm.x, v1.norm.y, v1.norm.z };
	toBinormal = glm::cross(toBinormal, { v1.tangent.x, v1.tangent.y, v1.tangent.z });
	v1.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v2.pos = { -w, h, -d };
	v2.color = { 0.5, 0.5, 0, 1.0 };
	v2.texCoord1 = { 0, 0 };
	v2.norm = { 0, 0, -1 };
	v2.tangent = { 1, 0, 0 };
	toBinormal = { v2.norm.x, v2.norm.y, v2.norm.z };
	toBinormal = glm::cross(toBinormal, { v2.tangent.x, v2.tangent.y, v2.tangent.z });
	v2.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v3.pos = { w, h, -d };
	v3.color = { 0.5, 0.5, 0, 1.0 };
	v3.texCoord1 = { 1, 0 };
	v3.norm = { 0, 0, -1 };
	v3.tangent = { 1, 0, 0 };
	toBinormal = { v3.norm.x, v3.norm.y, v3.norm.z };
	toBinormal = glm::cross(toBinormal, { v3.tangent.x, v3.tangent.y, v3.tangent.z });
	v3.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v4.pos = { w, -h, -d };
	v4.color = { 0.5, 0.5, 0, 1.0 };
	v4.texCoord1 = { 1, 1 };
	v4.norm = { 0, 0, -1 };
	v4.tangent = { 1, 0, 0 };
	toBinormal = { v4.norm.x, v4.norm.y, v4.norm.z };
	toBinormal = glm::cross(toBinormal, { v4.tangent.x, v4.tangent.y, v4.tangent.z });
	v4.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	//Front
	v5.pos = { -w, -h, d };
	v5.color = { 0.5, 0.5, 1.0, 1.0 };
	v5.texCoord1 = { 1, 1 };
	v5.norm = { 0, 0, 1 };
	v5.tangent = { -1, 0, 0 };
	toBinormal = { v5.norm.x, v5.norm.y, v5.norm.z };
	toBinormal = glm::cross(toBinormal, { v5.tangent.x, v5.tangent.y, v5.tangent.z });
	v5.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v6.pos = { w, -h, d };
	v6.color = { 0.5, 0.5, 1.0, 1.0 };
	v6.texCoord1 = { 0, 1 };
	v6.norm = { 0, 0, 1 };
	v6.tangent = { -1, 0, 0 };
	toBinormal = { v6.norm.x, v6.norm.y, v6.norm.z };
	toBinormal = glm::cross(toBinormal, { v6.tangent.x, v6.tangent.y, v6.tangent.z });
	v6.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v7.pos = { w, h, d };
	v7.color = { 0.5, 0.5, 1.0, 1.0 };
	v7.texCoord1 = { 0, 0 };
	v7.norm = { 0, 0, 1 };
	v7.tangent = { -1, 0, 0 };
	toBinormal = { v7.norm.x, v7.norm.y, v7.norm.z };
	toBinormal = glm::cross(toBinormal, { v7.tangent.x, v7.tangent.y, v7.tangent.z });
	v7.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v8.pos = { -w, h, d };
	v8.color = { 0.5, 0.5, 1.0, 1.0 };
	v8.texCoord1 = { 1, 0 };
	v8.norm = { 0, 0, 1 };
	v8.tangent = { -1, 0, 0 };
	toBinormal = { v8.norm.x, v8.norm.y, v8.norm.z };
	toBinormal = glm::cross(toBinormal, { v8.tangent.x, v8.tangent.y, v8.tangent.z });
	v8.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	//Top
	v9.pos = { -w, h, -d };
	v9.color = { 0.5, 1.0, 0.5, 1.0 };
	v9.texCoord1 = { 0, 1 };
	v9.norm = { 0, 1, 0 };
	v9.tangent = { 1, 0, 0 };
	toBinormal = { v9.norm.x, v9.norm.y, v9.norm.z };
	toBinormal = glm::cross(toBinormal, { v9.tangent.x, v9.tangent.y, v9.tangent.z });
	v9.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v10.pos = { -w, h, d };
	v10.color = { 0.5, 1.0, 0.5, 1.0 };
	v10.texCoord1 = { 0, 0 };
	v10.norm = { 0, 1, 0 };
	v10.tangent = { 1, 0, 0 };
	toBinormal = { v10.norm.x, v10.norm.y, v10.norm.z };
	toBinormal = glm::cross(toBinormal, { v10.tangent.x, v10.tangent.y, v10.tangent.z });
	v10.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v11.pos = { w, h, d };
	v11.color = { 0.5, 1.0, 0.5, 1.0 };
	v11.texCoord1 = { 1, 0 };
	v11.norm = { 0, 1, 0 };
	v11.tangent = { 1, 0, 0 };
	toBinormal = { v11.norm.x, v11.norm.y, v11.norm.z };
	toBinormal = glm::cross(toBinormal, { v11.tangent.x, v11.tangent.y, v11.tangent.z });
	v11.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v12.pos = { w, h, -d };
	v12.color = { 0.5, 1.0, 0.5, 1.0 };
	v12.texCoord1 = { 1, 1 };
	v12.norm = { 0, 1, 0 };
	v12.tangent = { 1, 0, 0 };
	toBinormal = { v12.norm.x, v12.norm.y, v12.norm.z };
	toBinormal = glm::cross(toBinormal, { v12.tangent.x, v12.tangent.y, v12.tangent.z });
	v12.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	//Bot
	v13.pos = { -w, -h, -d };
	v13.color = { 0.5, 0, 0.5, 1.0 };
	v13.texCoord1 = { 1, 1 };
	v13.norm = { 0, -1, 0 };
	v13.tangent = { -1, 0, 0 };
	toBinormal = { v13.norm.x, v13.norm.y, v13.norm.z };
	toBinormal = glm::cross(toBinormal, { v13.tangent.x, v13.tangent.y, v13.tangent.z });
	v13.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v14.pos = { w, -h, -d };
	v14.color = { 0.5, 0, 0.5, 1.0 };
	v14.texCoord1 = { 0, 1 };
	v14.norm = { 0, -1, 0 };
	v14.tangent = { -1, 0, 0 };
	toBinormal = { v14.norm.x, v14.norm.y, v14.norm.z };
	toBinormal = glm::cross(toBinormal, { v14.tangent.x, v14.tangent.y, v14.tangent.z });
	v14.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v15.pos = { w, -h, d };
	v15.color = { 0.5, 0, 0.5, 1.0 };
	v15.texCoord1 = { 0, 0 };
	v15.norm = { 0, -1, 0 };
	v15.tangent = { -1, 0, 0 };
	toBinormal = { v15.norm.x, v15.norm.y, v15.norm.z };
	toBinormal = glm::cross(toBinormal, { v15.tangent.x, v15.tangent.y, v15.tangent.z });
	v15.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v16.pos = { -w, -h, d };
	v16.color = { 0.5, 0, 0.5, 1.0 };
	v16.texCoord1 = { 1, 0 };
	v16.norm = { 0, -1, 0 };
	v16.tangent = { -1, 0, 0 };
	toBinormal = { v16.norm.x, v16.norm.y, v16.norm.z };
	toBinormal = glm::cross(toBinormal, { v16.tangent.x, v16.tangent.y, v16.tangent.z });
	v16.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	//Left
	v17.pos = { -w, -h, d };
	v17.color = { 0, 0.5, 0.5, 1.0 };
	v17.texCoord1 = { 0, 1 };
	v17.norm = { -1, 0, 0 };
	v17.tangent = { 0, 0, -1 };
	toBinormal = { v17.norm.x, v17.norm.y, v17.norm.z };
	toBinormal = glm::cross(toBinormal, { v17.tangent.x, v17.tangent.y, v17.tangent.z });
	v17.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v18.pos = { -w, h, d };
	v18.color = { 0, 0.5, 0.5, 1.0 };
	v18.texCoord1 = { 0, 0 };
	v18.norm = { -1, 0, 0 };
	v18.tangent = { 0, 0, -1 };
	toBinormal = { v18.norm.x, v18.norm.y, v18.norm.z };
	toBinormal = glm::cross(toBinormal, { v18.tangent.x, v18.tangent.y, v18.tangent.z });
	v18.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v19.pos = { -w, h, -d };
	v19.color = { 0, 0.5, 0.5, 1.0 };
	v19.texCoord1 = { 1, 0 };
	v19.norm = { -1, 0, 0 };
	v19.tangent = { 0, 0, -1 };
	toBinormal = { v19.norm.x, v19.norm.y, v19.norm.z };
	toBinormal = glm::cross(toBinormal, { v19.tangent.x, v19.tangent.y, v19.tangent.z });
	v19.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v20.pos = { -w, -h, -d };
	v20.color = { 0, 0.5, 0.5, 1.0 };
	v20.texCoord1 = { 1, 1 };
	v20.norm = { -1, 0, 0 };
	v20.tangent = { 0, 0, -1 };
	toBinormal = { v20.norm.x, v20.norm.y, v20.norm.z };
	toBinormal = glm::cross(toBinormal, { v20.tangent.x, v20.tangent.y, v20.tangent.z });
	v20.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	//Right
	v21.pos = { w, -h, -d };
	v21.color = { 1.0, 0.5, 0.5, 1.0 };
	v21.texCoord1 = { 0, 1 };
	v21.norm = { 1, 0, 0 };
	v21.tangent = { 0, 0, 1 };
	toBinormal = { v21.norm.x, v21.norm.y, v21.norm.z };
	toBinormal = glm::cross(toBinormal, { v21.tangent.x, v21.tangent.y, v21.tangent.z });
	v21.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v22.pos = { w, h, -d };
	v22.color = { 1.0, 0.5, 0.5, 1.0 };
	v22.texCoord1 = { 0, 0 };
	v22.norm = { 1, 0, 0 };
	v22.tangent = { 0, 0, 1 };
	toBinormal = { v22.norm.x, v22.norm.y, v22.norm.z };
	toBinormal = glm::cross(toBinormal, { v22.tangent.x, v22.tangent.y, v22.tangent.z });
	v22.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v23.pos = { w, h, d };
	v23.color = { 1.0, 0.5, 0.5, 1.0 };
	v23.texCoord1 = { 1, 0 };
	v23.norm = { 1, 0, 0 };
	v23.tangent = { 0, 0, 1 };
	toBinormal = { v23.norm.x, v23.norm.y, v23.norm.z };
	toBinormal = glm::cross(toBinormal, { v23.tangent.x, v23.tangent.y, v23.tangent.z });
	v23.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	v24.pos = { w, -h, d };
	v24.color = { 1.0, 0.5, 0.5, 1.0 };
	v24.texCoord1 = { 1, 1 };
	v24.norm = { 1, 0, 0 };
	v24.tangent = { 0, 0, 1 };
	toBinormal = { v24.norm.x, v24.norm.y, v24.norm.z };
	toBinormal = glm::cross(toBinormal, { v24.tangent.x, v24.tangent.y, v24.tangent.z });
	v24.biTangent = { toBinormal.x, toBinormal.y, toBinormal.z };

	mesh.vertices[0] = (v1);
	mesh.vertices[1] = (v2);
	mesh.vertices[2] = (v3);
	mesh.vertices[3] = (v4);
	mesh.vertices[4] = (v5);
	mesh.vertices[5] = (v6);
	mesh.vertices[6] = (v7);
	mesh.vertices[7] = (v8);
	mesh.vertices[8] = (v9);
	mesh.vertices[9] = (v10);
	mesh.vertices[10] = (v11);
	mesh.vertices[11] = (v12);
	mesh.vertices[12] = (v13);
	mesh.vertices[13] = (v14);
	mesh.vertices[14] = (v15);
	mesh.vertices[15] = (v16);
	mesh.vertices[16] = (v17);
	mesh.vertices[17] = (v18);
	mesh.vertices[18] = (v19);
	mesh.vertices[19] = (v20);
	mesh.vertices[20] = (v21);
	mesh.vertices[21] = (v22);
	mesh.vertices[22] = (v23);
	mesh.vertices[23] = (v24);

	mesh.indices =
	{
		0,1,2,0,2,3,
		4,5,6,4,6,7,
		8,9,10,8,10,11,
		12,13,14,12,14,15,
		16,17,18,16,18,19,
		20,21,22,20,22,23
	};

	mesh.min = { -50.0, -50.0, -50.0 };
	mesh.max = { 50.0, 50.0, 50.0 };
	return mesh;
}

Hail::Mesh Hail::CreateUnitSphere()
{
	Mesh mesh;
	float radius = 50.0f;
	uint32_t sliceCount = 12;
	uint32_t stackCount = 12;

	float phiStep = Math::PIf / stackCount;
	float thetaStep = 2.0f * Math::PIf / sliceCount;

	mesh.vertices;
	mesh.vertices.Prepare(40);
	VertexModel topVertex;
	topVertex.pos = { 0, radius, 0 };
	topVertex.color = { 1.0, 1.0,1.0,1.0 };
	topVertex.texCoord1 = { 0, 1 };
	mesh.vertices.Add(topVertex);

	for (uint32_t i = 1; i <= stackCount - 1; i++)
	{
		float phi = i * phiStep;
		for (uint32_t j = 0; j <= sliceCount; j++) {
			float theta = j * thetaStep;
			glm::vec3 p =
			{
				(radius * sinf(phi) * cosf(theta)),
				(radius * cosf(phi)),
				(radius * sinf(phi) * sinf(theta))
			};
			glm::vec3 tangent =
			{
				-radius * sinf(phi) * sinf(theta),
				0,
				radius * sinf(phi) * cosf(theta)
			};
			glm::normalize(tangent);
			glm::vec3 n = { p.x, p.y, p.z };
			glm::normalize(n);
			glm::vec3 binormal;
			binormal = glm::cross(tangent, n);
			glm::normalize(binormal);
			glm::vec2 uv = { theta / (Math::PIf * 2), phi / Math::PIf };

			VertexModel vertex;
			vertex.pos = p;
			vertex.color = { p.x * 1.0, p.y * 1.0 , p.z * 1.0 , 1.0 };
			vertex.texCoord1 = uv;
			vertex.norm = { n.x, n.y, n.z };
			vertex.biTangent = { binormal.x, binormal.y, binormal.z };;
			vertex.tangent = { tangent.x, tangent.y, tangent.z };;
			mesh.vertices.Add(vertex);
		}
	}
	VertexModel botVertex;
	botVertex.pos = { 0, -radius, 0 };
	botVertex.color = { 0, 0,0,1.0 };
	botVertex.texCoord1 = { 0, 1 };
	botVertex.norm = { 0, -1, 0 };
	botVertex.tangent = { 1, 0, 0 };
	glm::vec3 tangent = { 1, 0, 0 };
	glm::vec3 binormal = glm::cross(tangent, { 0, -1, 0 });
	botVertex.biTangent = { binormal.x, binormal.y, binormal.z };

	mesh.vertices.Add(botVertex);

	
	mesh.indices.Prepare(mesh.vertices.Size() * 3);

	for (uint32_t i = 1; i <= sliceCount; i++) {
		mesh.indices.Add(0);
		mesh.indices.Add(i + 1);
		mesh.indices.Add(i);
	}
	uint32_t baseIndex = 1;
	uint32_t ringVertexCount = sliceCount + 1;
	for (uint32_t i = 0; i < stackCount - 2; i++) {
		for (uint32_t j = 0; j < sliceCount; j++) {
			mesh.indices.Add(baseIndex + i * ringVertexCount + j);
			mesh.indices.Add(baseIndex + i * ringVertexCount + j + 1);
			mesh.indices.Add(baseIndex + (i + 1) * ringVertexCount + j);

			mesh.indices.Add(baseIndex + (i + 1) * ringVertexCount + j);
			mesh.indices.Add(baseIndex + i * ringVertexCount + j + 1);
			mesh.indices.Add(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}
	uint32_t southPoleIndex = mesh.vertices.Size() - 1;
	baseIndex = southPoleIndex - ringVertexCount;
	for (uint32_t i = 0; i < sliceCount; i++) {
		mesh.indices.Add(southPoleIndex);
		mesh.indices.Add(baseIndex + i);
		mesh.indices.Add(baseIndex + i + 1);
	}
	mesh.min = { -50.0, -50.0, -50.0 };
	mesh.max = { 50.0, 50.0, 50.0 };
	return mesh;
}

Hail::Mesh Hail::CreateUnitCylinder()
{
	Mesh mesh;
	float bottomRadius, topRadius, height;
	uint32_t sliceCount, stackCount;
	bottomRadius = 50.0;
	topRadius = bottomRadius;
	height = 100.0;
	sliceCount = 16;
	stackCount = 8;
	float stackHeight = height / (float)stackCount;
	float radiusStep = (topRadius - bottomRadius) / stackCount;
	uint32_t ringCount = stackCount + 1;

	//Vertices
	GrowingArray<VertexModel>& vertices = mesh.vertices;
	vertices.Prepare(40);
	for (uint32_t i = 0; i < ringCount + 1; i++)
	{
		float y = -0.5f * height + i * stackHeight;
		float r = bottomRadius + i * radiusStep;

		float dTheta = 2.0f * Math::PIf / sliceCount;
		for (uint32_t j = 0; j <= sliceCount; j++)
		{
			VertexModel vertex;

			float c = cosf(j * dTheta);
			float s = sinf(j * dTheta);

			vertex.pos = { r * c, y, r * s };
			vertex.texCoord1.x = (float)j / sliceCount;
			vertex.texCoord1.y = 1.0f - (float)i / stackCount;
			vertex.color = { r * c * 1.0, y * 1.0, r * s * 1.0, 1.0 };
			vertex.tangent = { -s, 0.0f, c };
			float dr = bottomRadius - topRadius;
			vertex.biTangent = { dr * c, -height, dr * s };

			glm::vec3 normal = { vertex.tangent.x,vertex.tangent.y,vertex.tangent.z };

			normal = glm::cross(normal, { vertex.biTangent.x,vertex.biTangent.y,vertex.biTangent.z });
			vertex.norm = { normal.x, normal.y, normal.z };

			vertices.Add(vertex);
		}
	}

	//Indices
	GrowingArray<uint32_t>& vertexIndices = mesh.indices;
	vertexIndices.Prepare(vertices.Size() * 3);
	uint32_t ringVertexCount = sliceCount + 1;
	for (uint32_t i = 0; i < stackCount; i++)
	{
		for (uint32_t j = 0; j < sliceCount; j++)
		{
			vertexIndices.Add(i * ringVertexCount + j);
			vertexIndices.Add((i + 1) * ringVertexCount + j);
			vertexIndices.Add((i + 1) * ringVertexCount + j + 1);

			vertexIndices.Add(i * ringVertexCount + j);
			vertexIndices.Add((i + 1) * ringVertexCount + j + 1);
			vertexIndices.Add(i * ringVertexCount + j + 1);
		}
	}

	BuildCylinderTopCap(topRadius, height, sliceCount, vertices, vertexIndices);
	BuildCylinderBottomCap(bottomRadius, height, sliceCount, vertices, vertexIndices);
	mesh.min = { -50.0, -50.0, -50.0 };
	mesh.max = { 50.0, 50.0, 50.0 };
	return mesh;
}

void Hail::BuildCylinderTopCap(float topRadius, float height, uint32_t sliceCount, GrowingArray<VertexModel>& meshVertices, GrowingArray<uint32_t>& meshIndeces)
{
	uint32_t baseIndex = (uint32_t)meshVertices.Size();

	float y = 0.5f * height;
	float dTheta = 2.0f * Math::PIf / sliceCount;
	for (uint32_t i = 0; i <= sliceCount; i++)
	{
		float x = topRadius * cosf(i * dTheta);
		float z = topRadius * sinf(i * dTheta);
		float u = x / height + 0.5f;
		float v = z / height + 0.5f;
		VertexModel vertex;
		vertex.pos = { x, y, z };
		vertex.texCoord1 = { u,v };
		vertex.color = { x * 1.0 * y, 1.0 * y, z * 1.0 * y, 1.0 };
		vertex.tangent = { 0,0,1 };
		vertex.biTangent = { 1,0,0 };
		vertex.norm = { 0,1,0 };

		meshVertices.Add(vertex);
	}
	//Cap Center Vertex
	VertexModel topMidVertex;
	topMidVertex.pos = { 0.0f, y, 0.0f };
	topMidVertex.texCoord1 = { 0.5f, 0.5f };
	topMidVertex.color = { 1.0 * y, 1.0 * y, 1.0 * y,1.0 };
	topMidVertex.tangent = { 0,0,1 };
	topMidVertex.biTangent = { 1,0,0 };
	topMidVertex.norm = { 0,1,0 };
	meshVertices.Add(topMidVertex);
	uint32_t centerIndex = (uint32_t)meshVertices.Size() - 1;

	for (uint32_t i = 0; i < sliceCount; i++)
	{
		meshIndeces.Add(centerIndex);
		meshIndeces.Add(baseIndex + i + 1);
		meshIndeces.Add(baseIndex + i);
	}

}

void Hail::BuildCylinderBottomCap(float bottomRadius, float height, uint32_t sliceCount, GrowingArray<VertexModel>& meshVertices, GrowingArray<uint32_t>& meshIndeces)
{
	uint32_t baseIndex = (uint32_t)meshVertices.Size();

	float y = -0.5f * height;
	float dTheta = 2.0f * Math::PIf / sliceCount;
	for (uint32_t i = 0; i <= sliceCount; i++)
	{
		float x = bottomRadius * cosf(i * dTheta);
		float z = bottomRadius * sinf(i * dTheta);
		float u = x / height + 0.5f;
		float v = z / height + 0.5f;
		VertexModel vertex;
		vertex.pos = { x, y, z };
		vertex.texCoord1 = { u,v };
		vertex.color = { x * 1.0 * y, 0 , z * 1.0 * y, 1.0 };
		vertex.tangent = { 0,0,1 };
		vertex.biTangent = { -1,0,0 };
		vertex.norm = { 0,-1,0 };
		meshVertices.Add(vertex);
	}
	//Cap Center Vertex
	VertexModel botMiddleVertex;
	botMiddleVertex.pos = { 0.0f, y, 0.0f };
	botMiddleVertex.texCoord1 = { 0.5f, 0.5f };
	botMiddleVertex.color = { 1.0 * y, 0, 1.0 * y ,1.0 };
	botMiddleVertex.tangent = { 0,0,1 };
	botMiddleVertex.biTangent = { -1,0,0 };
	botMiddleVertex.norm = { 0,-1,0 };
	meshVertices.Add(botMiddleVertex);
	uint32_t centerIndex = (uint32_t)meshVertices.Size() - 1;

	for (uint32_t i = 0; i < sliceCount; i++)
	{
		meshIndeces.Add(centerIndex);
		meshIndeces.Add(baseIndex + i);
		meshIndeces.Add(baseIndex + i + 1);
	}
}