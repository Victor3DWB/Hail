#include "Engine_PCH.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "ShaderCompiler.h"
#include "ShaderReflection.h"

#include "DebugMacros.h"
#include "Utility\InOutStream.h"
#include "HailEngine.h"
#include "ResourceRegistry.h"
#include "Rendering\SwapChain.h"

#include "ImGui\ImGuiContext.h"
#include "Hashing\xxh64_en.hpp"

#include "Rendering\RenderDevice.h"

namespace Hail
{

	void MaterialManager::Init(RenderingDevice* renderingDevice, TextureManager* textureResourceManager, RenderingResourceManager* renderingResourceManager, SwapChain* swapChain, ErrorManager* pErrorManager)
	{
		m_MaterialTypeObjects.Fill(nullptr);
		m_renderDevice = renderingDevice;
		m_textureManager = textureResourceManager;
		m_swapChain = swapChain;
		m_renderingResourceManager = renderingResourceManager;
		m_pErrorManager = pErrorManager;
		m_loadedShaders[0].Prepare(32u);
		m_loadedShaders[(uint32)eShaderStage::Fragment].Prepare(32u);
	}

	void MaterialManager::Update()
	{
		ResourceRegistry& reg = GetResourceRegistry();
		const uint32 frameInFlight = m_swapChain->GetFrameInFlight();
		uint32 i = 0u;
		for (; i < m_loadRequests.Size(); ++i)
		{
			uint32 numberOfTexturesAreHandled = 0u;
			for (uint32 iTexture = 0u; iTexture < MAX_TEXTURE_HANDLES; iTexture++)
			{
				if (m_loadRequests[i].textureHandles[iTexture] != GuidZero)
				{
					bool bIsTextureHandled = false;
					const eResourceState state = reg.GetResourceState(ResourceType::Texture, m_loadRequests[i].textureHandles[iTexture]);

					if (state == eResourceState::Invalid)
					{
						m_loadRequests[i].materialInstance.m_textureHandles[iTexture] = INVALID_TEXTURE_HANDLE;
						H_ASSERT(m_loadRequests[i].numberOfTexturesInMaterial != 0u);
						m_loadRequests[i].numberOfTexturesInMaterial--;
					}
					else if (state == eResourceState::Loaded)
					{
						bIsTextureHandled = true;
					}

					if (bIsTextureHandled)
						numberOfTexturesAreHandled++;
				}
			}

			// We check if all textures are handled that the instance care about
			if (numberOfTexturesAreHandled != m_loadRequests[i].numberOfTexturesInMaterial)
				continue;

			m_loadRequests[i].materialInstance.m_instanceIdentifier = m_materialsInstanceData.Size();
			m_materialsInstanceData.Add(m_loadRequests[i].materialInstance);
			ResourceValidator instanceValidator = ResourceValidator();
			instanceValidator.MarkResourceAsDirty(frameInFlight);
			m_materialsInstanceValidationData.Add(instanceValidator);
			if (InitMaterialInstance(m_loadRequests[i].materialInstance.m_materialType, m_loadRequests[i].materialInstance))
			{
				reg.SetResourceLoaded(ResourceType::Material, m_loadRequests[i].materialInstance.m_id);
			}
			else
			{
				m_materialsInstanceValidationData.RemoveLast();
				m_materialsInstanceData.RemoveLast();
				reg.SetResourceLoadFailed(ResourceType::Material, m_loadRequests[i].materialInstance.m_id);
			}

			m_loadRequests.RemoveCyclicAtIndex(i);
			i--;
		}

		//if (bFinishedLoadingMaterial)
		//{
		//	m_loadRequests.RemoveCyclicAtIndex(i);
		//	i--;
		//	bFinishedLoadingMaterial = false;
		//}
	}

	uint64 LocalGetShaderHash(String64 shader)
	{
		return xxh64::hash(shader.Data(), shader.Length(), 1337);
	}


	uint64 LocalGetCombinedShaderHash(CompiledShader* shader1, CompiledShader* shader2)
	{
		// TODO assert if no shader
		uint64 shader1Hash = 0;
		uint64 shader2Hash = 0;

		if (shader1)
			shader1Hash = shader1->m_nameHash;
		if (shader2)
			shader2Hash = shader2->m_nameHash;

		const uint64 combinedShaderHash = (shader1Hash & 0xffff0000) | (shader2Hash & 0xffff);
		H_ASSERT(combinedShaderHash, "No shader added, incorrect code.");
		return combinedShaderHash;
	}

	uint64 LocalGetCombinedShaderHash(String64 shader1, String64 shader2)
	{
		// TODO assert if no shader
		uint64 shader1Hash = 0;
		uint64 shader2Hash = 0;

		if (!shader1.Empty())
		{
			shader1Hash = xxh64::hash(shader1.Data(), shader1.Length(), 1337);
		}
		if (!shader2.Empty())
		{
			shader2Hash = xxh64::hash(shader2.Data(), shader2.Length(), 1337);
		}
		const uint64 combinedShaderHash = (shader1Hash & 0xffff0000) | (shader2Hash & 0xffff);
		H_ASSERT(combinedShaderHash, "No shader added, incorrect code.");
		return combinedShaderHash;
	}

	bool MaterialManager::InitDefaultMaterial(eMaterialType type, FrameBufferTexture* frameBufferToBindToMaterial, bool reloadShader, uint32 frameInFlight)
	{
		if (m_materials[(uint32_t)type].Empty())
		{
			Material* pMaterial = CreateUnderlyingMaterial();
			pMaterial->m_pPipeline->m_shaderStages = 0u;
			String64 shaderNames[2];
			switch (type)
			{
			case eMaterialType::SPRITE:
				shaderNames[0] = "VS_Sprite";
				shaderNames[1] = "FS_Sprite";
				pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaderNames[0], eShaderStage::Vertex, reloadShader));
				pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaderNames[1], eShaderStage::Fragment, reloadShader));
				break;
			case eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
				shaderNames[0] = "VS_fullscreenPass";
				shaderNames[1] = "FS_fullscreenPass";
				pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaderNames[0], eShaderStage::Vertex, reloadShader));
				pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaderNames[1], eShaderStage::Fragment, reloadShader));
				break;
			case eMaterialType::MODEL3D:
				shaderNames[0] = "VS_triangle";
				shaderNames[1] = "FS_triangle";
				pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaderNames[0], eShaderStage::Vertex, reloadShader));
				pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaderNames[1], eShaderStage::Fragment, reloadShader));
				break;
			case eMaterialType::COUNT:
				break;
			default:
				break;
			}
			CompiledShader* shaders[2];
			shaders[0] = nullptr;
			shaders[1] = nullptr;
			for (size_t i = 0; i < pMaterial->m_pPipeline->m_pShaders.Size(); i++)
			{
				if (!pMaterial->m_pPipeline->m_pShaders[i])
				{
					H_ASSERT(false, "Failed to load default shader.");
					m_pErrorManager->AddString(StringL::Format("Failed to load default shader: %s", shaderNames[i]));
					return false;
				}
				if (pMaterial->m_pPipeline->m_pShaders[i]->loadState != eShaderLoadState::LoadedToRAM)
				{
					m_pErrorManager->AddString(StringL::Format("Failed to load default shader: %s in to RAM", shaderNames[i]));
					H_ASSERT(false, "Failed to load default shader.");
					return false;
				}
				shaders[i] = pMaterial->m_pPipeline->m_pShaders[i];
				m_defaultShaders[(uint32)type].Add(pMaterial->m_pPipeline->m_pShaders[i]);
				pMaterial->m_pPipeline->m_shaderStages |= (1 << pMaterial->m_pPipeline->m_pShaders[i]->header.shaderType);
			}
			uint32 combinedShaderHash = LocalGetCombinedShaderHash(shaders[0], shaders[1]);
			const uint64 materialSortKey = GetMaterialSortValue(pMaterial->m_pPipeline->m_type, pMaterial->m_pPipeline->m_blendMode, combinedShaderHash);
			pMaterial->m_pPipeline->m_sortKey = materialSortKey;
			pMaterial->m_pPipeline->m_type = type;
			pMaterial->m_pPipeline->m_typeRenderPass = type;
			pMaterial->m_pPipeline->m_bUseTypePasses = true;
			m_materials[(uint32_t)type].Add(pMaterial);
		}

		// TODO: look over how we bind the framebuffer when making a render graph
		BindFrameBuffer(type, frameBufferToBindToMaterial);

		if (InitMaterialInternal(m_materials[(uint32_t)type].GetLast(), frameInFlight))
		{
			m_materials[(uint32_t)type][0]->m_validator.ClearFrameData(frameInFlight);
			return true;
		}
		m_pErrorManager->AddString("Failed to Initialize material.");
		return false;
	}

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
		return false;
	}

	struct ShaderMetaData
	{
		ShaderProperties m_serializeableType;
		String64 m_name;
	};

	VectorOnStack<ShaderMetaData, 2> LocalGetDefaultShadersFromType(eMaterialType materialType)
	{
		VectorOnStack<ShaderMetaData, 2> shaders;
		switch (materialType)
		{
		case eMaterialType::SPRITE:
			shaders.Add({ eShaderStage::Vertex, GuidZero, "VS_Sprite"});
			shaders.Add({ eShaderStage::Fragment, GuidZero, "FS_Sprite" });
			break;
		case eMaterialType::MODEL3D:
			shaders.Add({ eShaderStage::Vertex, GuidZero, "VS_triangle" });
			shaders.Add({ eShaderStage::Fragment, GuidZero, "FS_triangle" });
			break;
		case eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
			shaders.Add({ eShaderStage::Vertex, GuidZero, "VS_fullscreenPass" });
			shaders.Add({ eShaderStage::Fragment, GuidZero, "FS_fullscreenPass" });
			break;
		case eMaterialType::COUNT:
			break;
		default:
			break;
		}
		return shaders;
	}

	ShaderMetaData LocalGetDefaultVertexShaderForMaterial(eMaterialType matType)
	{
		const eShaderStage type = eShaderStage::Vertex;
		String64 shaderString;
		switch (matType)
		{
		case Hail::eMaterialType::SPRITE:
			shaderString = "VS_Sprite";
			break;
		case Hail::eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
			shaderString = "VS_fullscreenPass";
			break;
		case Hail::eMaterialType::MODEL3D:
			shaderString = "VS_triangle";
			break;
		case Hail::eMaterialType::COUNT:
		default:
			break;
		}
		return { type, GuidZero, shaderString };
	}
	ShaderMetaData LocalGetDefaultFragmentShaderForMaterial(eMaterialType matType)
	{
		const eShaderStage type = eShaderStage::Fragment;
		String64 shaderString;
		switch (matType)
		{
		case Hail::eMaterialType::SPRITE:
			shaderString = "FS_Sprite";
			break;
		case Hail::eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
			shaderString = "FS_fullscreenPass";
			break;
		case Hail::eMaterialType::MODEL3D:
			shaderString = "FS_triangle";
			break;
		case Hail::eMaterialType::COUNT:
		default:
			break;
		}
		return { type, GuidZero, shaderString };
	}

	ShaderMetaData LocalGetDefaultPairShaderFromShaderMaterialTypePair(eShaderStage shaderStage, eMaterialType matType)
	{
		switch (shaderStage)
		{
		case Hail::eShaderStage::Vertex:
			return LocalGetDefaultFragmentShaderForMaterial(matType);
		case Hail::eShaderStage::Fragment:
			return LocalGetDefaultVertexShaderForMaterial(matType);
		case Hail::eShaderStage::Mesh:
			return LocalGetDefaultFragmentShaderForMaterial(matType);
		case Hail::eShaderStage::Compute:
		case Hail::eShaderStage::Amplification:
		case Hail::eShaderStage::None:
			break;
		default:
			break;
		}
		H_ASSERT(false, "Should not get here.");
		return { eShaderStage::None, GuidZero, "" };
	}

	VectorOnStack<ShaderMetaData, 2> LocalGetShadersForMaterial(eMaterialType materialType, ShaderProperties shader1, ShaderProperties shader2, bool createDefaultMaterial)
	{
		VectorOnStack<ShaderMetaData, 2> shaders;
		if (shader1.m_id == GuidZero && shader2.m_id == GuidZero)
		{
			if (!createDefaultMaterial)
			{
				H_ERROR("creating an incomplete material without shaders.");
				return shaders;
			}
			shaders = LocalGetDefaultShadersFromType(materialType);
		}
		else
		{
			ResourceRegistry& resourceRegistry = GetResourceRegistry();
			if (shader1.m_id != GuidZero && resourceRegistry.GetIsResourceImported(ResourceType::Shader, shader1.m_id))
			{
				shaders.Add({ shader1.m_type, shader1.m_id, resourceRegistry.GetResourceName(ResourceType::Shader, shader1.m_id) });
			}
			if (shader2.m_id != GuidZero && resourceRegistry.GetIsResourceImported(ResourceType::Shader, shader2.m_id))
			{
				shaders.Add({ shader2.m_type, shader2.m_id, resourceRegistry.GetResourceName(ResourceType::Shader, shader2.m_id) });
			}
		}

		if (shaders.Size() == 1 && LocalDoesTypeRequire2Shaders(materialType))
		{
			shaders.Add(LocalGetDefaultPairShaderFromShaderMaterialTypePair(shaders[0].m_serializeableType.m_type, materialType));

			if (shaders[0].m_serializeableType.m_type == eShaderStage::Fragment)
			{
				ShaderMetaData fragmentShader = shaders[0];
				shaders[0] = shaders[1];
				shaders[1] = fragmentShader;
			}
		}
		return shaders;
	}

	uint32 MaterialManager::LoadMaterialFromSerializedData(const SerializeableMaterial& loadedMaterialInstance)
	{
		// Get Shader pair or individual shader for the material.
		VectorOnStack<ShaderMetaData, 2> shaders = LocalGetShadersForMaterial(loadedMaterialInstance.m_baseMaterialType, loadedMaterialInstance.m_shaders[0], loadedMaterialInstance.m_shaders[1], true);

		if (shaders.Empty())
		{
			return MAX_UINT;
		}

		// Get the material sort value to see if the material is loaded.
		uint64 combinedShaderHash = LocalGetCombinedShaderHash(shaders[0].m_name, shaders.Size() == 1 ? "" : shaders[1].m_name);
		const uint64 materialSortKey = GetMaterialSortValue(loadedMaterialInstance.m_baseMaterialType, loadedMaterialInstance.m_blendMode, combinedShaderHash);
		for (size_t i = 0; i < m_materials[(uint8)loadedMaterialInstance.m_baseMaterialType].Size(); i++)
		{
			Material& loadedMaterial = *m_materials[(uint8)loadedMaterialInstance.m_baseMaterialType][i];
			if (loadedMaterial.m_pPipeline->m_sortKey == materialSortKey)
			{
				return i;
			}
		}

		// Otherwise we create a new material
		Material* pMaterial = CreateUnderlyingMaterial();
		pMaterial->m_pPipeline->m_sortKey = materialSortKey;
		pMaterial->m_pPipeline->m_type = loadedMaterialInstance.m_baseMaterialType;
		pMaterial->m_pPipeline->m_typeRenderPass = loadedMaterialInstance.m_baseMaterialType;
		pMaterial->m_pPipeline->m_blendMode = loadedMaterialInstance.m_blendMode;
		pMaterial->m_pPipeline->m_bUseTypePasses = true;
		pMaterial->m_pPipeline->m_shaderStages = 0u;
		for (size_t i = 0; i < shaders.Size(); i++)
		{
			pMaterial->m_pPipeline->m_pShaders.Add(LoadShader(shaders[i].m_name, shaders[i].m_serializeableType.m_type, false));
			if (pMaterial->m_pPipeline->m_pShaders[i] == nullptr)
			{
				SAFEDELETE(pMaterial);
				return MAX_UINT;
			}
			pMaterial->m_pPipeline->m_shaderStages |= (1 << (uint8)shaders[i].m_serializeableType.m_type);
		}

		bool result = true;
		for (size_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
		{
			if (InitMaterialInternal(pMaterial, i))
			{
				pMaterial->m_validator.ClearFrameData(i);
			}
			else
			{
				result &= false;
				pMaterial->CleanupResource(*m_renderDevice);
				break;
			}
		}

		if (!result)
		{
			SAFEDELETE(pMaterial);
			return MAX_UINT;
		}
		const uint32 materialIndex = m_materials[(uint32_t)loadedMaterialInstance.m_baseMaterialType].Size();
		m_materials[(uint32_t)loadedMaterialInstance.m_baseMaterialType].Add(pMaterial);
		return materialIndex;
	}

	Material* MaterialManager::GetMaterial(eMaterialType materialType, uint32 materialIndex)
	{
		return m_materials[(uint32)materialType][materialIndex];
	}

	Material* MaterialManager::GetDefaultMaterial(eMaterialType materialType)
	{
		return GetMaterial(materialType, 0u);
	}

	MaterialPipeline* MaterialManager::CreateMaterialPipeline(MaterialCreationProperties props)
	{
		// Get Shader pair or individual shader for the material.
		VectorOnStack<ShaderMetaData, 2> shaders = LocalGetShadersForMaterial(props.m_typeRenderPass, props.m_shaders[0], props.m_shaders[1], false);

		if (shaders.Empty())
		{
			return nullptr;
		}

		uint64 combinedShaderHash = LocalGetCombinedShaderHash(shaders[0].m_name, shaders.Size() == 1 ? "" : shaders[1].m_name);
		const uint64 materialSortKey = GetMaterialSortValue(props.m_baseMaterialType, props.m_blendMode, combinedShaderHash);

		MaterialPipeline* pMatPipeline = CreateUnderlyingMaterialPipeline();
		Pipeline* pPipeline = pMatPipeline->m_pPipeline;
		pPipeline->m_sortKey = materialSortKey;
		pPipeline->m_type = props.m_baseMaterialType;
		pPipeline->m_blendMode = props.m_blendMode;
		pPipeline->m_bIsWireFrame = props.m_bIsWireFrame;
		pPipeline->m_shaderStages = 0u;
		for (size_t i = 0; i < shaders.Size(); i++)
		{
			pPipeline->m_pShaders.Add(LoadShader(shaders[i].m_name, shaders[i].m_serializeableType.m_type, false));
			if (pPipeline->m_pShaders[i] == nullptr)
			{
				SAFEDELETE(pPipeline);
				return nullptr;
			}
			pPipeline->m_shaderStages |= (1 << (uint8)shaders[i].m_serializeableType.m_type);
		}
		pPipeline->m_bIsCompute = props.m_shaders[0].m_type == eShaderStage::Compute;
		pPipeline->m_bUseTypePasses = props.m_bUsesMaterialTypeData;
		pPipeline->m_typeRenderPass = props.m_typeRenderPass;

		if (pPipeline->m_pShaders[0]->header.shaderType == (uint32)eShaderStage::Vertex)
		{
			pPipeline->m_bUsesVertexBuffer = pPipeline->m_pShaders[0]->reflectedShaderData.m_shaderInputs.Size() > 0u;
		}

		bool result = true;
		for (size_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
		{
			//TODO: Fortsätt att kolla här! :D 
			if (InitMaterialPipelineInternal(pMatPipeline, i))
			{
				pMatPipeline->m_validator.ClearFrameData(i);
			}
			else
			{
				result &= false;
				pMatPipeline->CleanupResource(*m_renderDevice);
				break;
			}
		}

		if (!result)
		{
			SAFEDELETE(pMatPipeline);
			return nullptr;
		}
		return pMatPipeline;
	}

	const MaterialInstance& MaterialManager::GetMaterialInstance(uint32_t instanceID, eMaterialType materialType)
	{
		if (instanceID >= m_materialsInstanceData.Size())
		{
			if (materialType == eMaterialType::SPRITE)
				return m_defaultSpriteMaterialInstance;
			if (materialType == eMaterialType::MODEL3D)
				return m_default3DMaterialInstance;
		}

		return m_materialsInstanceData[instanceID];
	}

	bool MaterialManager::InitMaterialInstance(eMaterialType materialType, MaterialInstance instanceData)
	{
		bool internalCreationResult = true;
		for (uint32_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
		{
			internalCreationResult &= InitMaterialInstance(instanceData.m_instanceIdentifier, i);
		}
		return internalCreationResult;
	}

	Hail::uint32 MaterialManager::GetMaterialInstanceHandle(GUID guid) const
	{
		if (guid == GuidZero)
			return MAX_UINT;

		for (uint32 i = 0; i < m_materialsInstanceData.Size(); i++)
		{
			if (guid == m_materialsInstanceData[i].m_id)
				return m_materialsInstanceData[i].m_instanceIdentifier;
		}

		return MAX_UINT;
	}

	bool MaterialManager::InitMaterialInstance(uint32 instanceID, uint32 frameInFlight)
	{
		if (InitMaterialInstanceInternal(m_materialsInstanceData[instanceID], frameInFlight, false))
		{
			m_materialsInstanceValidationData[instanceID].ClearFrameData(frameInFlight); 
			return true;
		}
		return false;
	}

	bool MaterialManager::ReloadAllMaterials(uint32 frameInFlight)
	{
		bool result = true;
		for (uint32 i = 0; i < (uint32)(eMaterialType::COUNT); i++)
		{
			for (uint32 iMaterial = 0; iMaterial < m_materials[i].Size(); iMaterial++)
			{
				// TODO assert on the pointer here 
				const bool firstFrameOfReload = m_materials[i][iMaterial]->m_validator.GetIsResourceDirty() == false;
				if (firstFrameOfReload)
				{
					ClearHighLevelMaterial(m_materials[i][iMaterial], frameInFlight);
				}
				ClearMaterialInternal(m_materials[i][iMaterial], frameInFlight);
			}
			if (!InitDefaultMaterial((eMaterialType)i, nullptr, true, frameInFlight))
			{
				H_ASSERT(false, "Failed to reload default material.");
			}
		}

		for (uint32 i = 0; i < m_materialsInstanceData.Size(); i++)
		{
			m_materialsInstanceValidationData[i].MarkResourceAsDirty(frameInFlight);
			if (InitMaterialInstanceInternal(m_materialsInstanceData[i], frameInFlight, false))
			{
				m_materialsInstanceValidationData[i].ClearFrameData(frameInFlight);
				result = false;
			}
		}

		return result;
	}

	bool MaterialManager::ReloadAllMaterialInstances(uint32 frameInFlight)
	{
		CheckMaterialInstancesToReload(frameInFlight);
		bool result = true;
		for (uint32 i = 0; i < m_materialsInstanceData.Size(); i++)
		{
			if (m_materialsInstanceValidationData[i].GetIsFrameDataDirty(frameInFlight))
			{
				if (InitMaterialInstanceInternal(m_materialsInstanceData[i], frameInFlight, false))
				{
					m_materialsInstanceValidationData[i].ClearFrameData(frameInFlight);
					result = false;
				}
			}
		}
		return false;
	}

	void MaterialManager::ClearHighLevelMaterial(Material* pMaterial, uint32 frameInFlight)
	{
		//TODO make more proper and robust validation later 
		pMaterial->m_validator.MarkResourceAsDirty(frameInFlight);
	}

	void MaterialManager::CheckMaterialInstancesToReload(uint32 frameInFlight)
	{
		//TODO: potentionally quite slow, but is this a problem as it is only done for reload? 
		const GrowingArray<TextureManager::TextureWithView>& textures = m_textureManager->GetLoadedTextures();
		for (size_t textureID = 0; textureID < textures.Size(); textureID++)
		{
			//If the texture is marked dirty this frame
			if (textures[textureID].m_pTexture->m_validator.GetIsResourceDirty())
			{
				for (uint32 instance = 0; instance < m_materialsInstanceData.Size(); instance++)
				{
					//if we are not already reloading this texture
					if (m_materialsInstanceValidationData[instance].GetIsResourceDirty())
					{
						continue;
					}
					for (uint32 instanceTexture = 0; instanceTexture < MAX_TEXTURE_HANDLES; instanceTexture++)
					{
						if(m_materialsInstanceData[instance].m_textureHandles[instanceTexture] == textureID)
						{
							m_materialsInstanceValidationData[instance].MarkResourceAsDirty(frameInFlight);
							break;
						}
					}
				}
			}
		}
	}

	void MaterialManager::InitDefaultMaterialInstances()
	{
		m_defaultSpriteMaterialInstance.m_instanceIdentifier = MAX_UINT;
		m_defaultSpriteMaterialInstance.m_materialIndex = 0;// GetMaterialSortValue(eMaterialType::SPRITE, eBlendMode::None, TempLocalGetShaderHash(eMaterialType::SPRITE));
		m_defaultSpriteMaterialsInstanceValidationData.MarkResourceAsDirty(0);

		m_default3DMaterialInstance.m_instanceIdentifier = MAX_UINT;
		m_default3DMaterialInstance.m_materialIndex = 0;// GetMaterialSortValue(eMaterialType::MODEL3D, eBlendMode::None, TempLocalGetShaderHash(eMaterialType::MODEL3D));
		m_default3DMaterialInstance.m_materialType = eMaterialType::MODEL3D;
		m_default3DMaterialsInstanceValidationData.MarkResourceAsDirty(0);
		for (size_t frameInFlight = 0; frameInFlight < MAX_FRAMESINFLIGHT; frameInFlight++)
		{
			if (!InitMaterialInstanceInternal(m_defaultSpriteMaterialInstance, frameInFlight, true))
			{
				H_ASSERT(false, "Failed to create default sprite material instance");
			}
			if (!InitMaterialInstanceInternal(m_default3DMaterialInstance, frameInFlight, true))
			{
				H_ASSERT(false, "Failed to create default mesh material instance");
			}
		}

	}

	bool MaterialManager::LoadMaterialFromSerializeableInstanceGUID(const GUID guid)
	{
		ResourceRegistry& reg = GetResourceRegistry();
		
		const SerializeableMaterial matData = LoadMaterialSerializeableInstance(reg.GetProjectPath(ResourceType::Material, guid));

		uint32 materialIndex = LoadMaterialFromSerializedData(matData);
		if (materialIndex == MAX_UINT)
		{
			H_ERROR(StringL::Format("Failed to load material: %s", reg.GetResourceName(ResourceType::Material, guid)));
			return false;
		}

		MaterialInstance instance;
		instance.m_id = guid;
		instance.m_materialType = matData.m_baseMaterialType;
		instance.m_blendMode = matData.m_blendMode;
		if (matData.m_baseMaterialType == eMaterialType::SPRITE)
		{
			instance.m_cutoutThreshold = matData.m_extraData;
		}

		instance.m_textureHandles.Fill(INVALID_TEXTURE_HANDLE);

		for (uint32 i = 0; i < MAX_TEXTURE_HANDLES; i++)
		{
			if (matData.m_textureHandles[i] != GuidZero)
				instance.m_textureHandles[i] = m_textureManager->StagerredTextureLoad(matData.m_textureHandles[i]);
		}

		instance.m_instanceIdentifier = m_materialsInstanceData.Size();
		instance.m_materialIndex = materialIndex;
		m_materialsInstanceData.Add(instance);
		ResourceValidator instanceValidator = ResourceValidator();
		instanceValidator.MarkResourceAsDirty(0);
		m_materialsInstanceValidationData.Add(instanceValidator);

		if (!InitMaterialInstance(matData.m_baseMaterialType, instance))
		{
			m_materialsInstanceValidationData.RemoveLast();
			m_materialsInstanceData.RemoveLast();
			return false;
		}
		return true;
	}

	bool MaterialManager::StaggeredMaterialLoad(const GUID guid)
	{
		ResourceRegistry& reg = GetResourceRegistry();

		const SerializeableMaterial matData = LoadMaterialSerializeableInstance(reg.GetProjectPath(ResourceType::Material, guid));

		uint32 materialIndex = LoadMaterialFromSerializedData(matData);
		if (materialIndex == MAX_UINT)
		{
			H_ERROR(StringL::Format("Failed to load material: %s", reg.GetResourceName(ResourceType::Material, guid)));
			return false;
		}

		MaterialInstance instance;
		instance.m_id = guid;
		instance.m_materialType = matData.m_baseMaterialType;
		instance.m_blendMode = matData.m_blendMode;
		if (matData.m_baseMaterialType == eMaterialType::SPRITE)
		{
			instance.m_cutoutThreshold = matData.m_extraData;
		}

		instance.m_textureHandles.Fill(INVALID_TEXTURE_HANDLE);

		uint32 numberOfTextures = 0u;

		for (uint32 i = 0; i < MAX_TEXTURE_HANDLES; i++)
		{
			if (matData.m_textureHandles[i] != GuidZero)
			{
				instance.m_textureHandles[i] = m_textureManager->StagerredTextureLoad(matData.m_textureHandles[i]);
				numberOfTextures++;
			}
		}

		instance.m_materialIndex = materialIndex;

		MaterialLoadRequest loadRequest;
		loadRequest.materialInstance = instance;
		loadRequest.textureHandles = matData.m_textureHandles;
		loadRequest.numberOfTexturesInMaterial = numberOfTextures;
		m_loadRequests.Add(loadRequest);

		reg.SetResourceUnloaded(ResourceType::Material, guid);
	}

	MaterialTypeObject* MaterialManager::GetTypeData(Pipeline* pPipeline)
	{
		H_ASSERT(pPipeline, "Invalid pipeline");

		if (pPipeline->m_bUseTypePasses)
		{
			return m_MaterialTypeObjects[(int)pPipeline->m_type];
		}
		return pPipeline->m_pTypeObject;
	}

	//TODO: Add relative path support in the shader output
	CompiledShader* MaterialManager::LoadShader(const char* shaderName, eShaderStage shaderStage, bool reloadShader, const char* shaderExtension)
	{
		const uint64 shaderHash = LocalGetShaderHash(shaderName);
		if (shaderStage != eShaderStage::None)
		{
			for (uint32 i = 0; i < m_loadedShaders[(uint32)shaderStage].Size(); i++)
			{
				if (m_loadedShaders[(uint32)shaderStage][i].m_nameHash == shaderHash)
					return &m_loadedShaders[(uint32)shaderStage][i];
			}
		}
		else
		{
			if (shaderStage == eShaderStage::None)
			{
				shaderStage = ShaderCompiler::CheckShaderType(shaderExtension);
			}
		}
		String64 combinedName = String64::Format("%s.shr", shaderName);
		WString64 shaderNameW;
		FromConstCharToWChar(combinedName.Data(), shaderNameW.Data(), 64);
		FilePath compiledShaderPath = FilePath::GetShaderCompiledDirectory() + shaderNameW;
		InOutStream inStream;

		const bool doesCompiledShaderExist = inStream.OpenFile(compiledShaderPath, FILE_OPEN_TYPE::READ, true);

		if (doesCompiledShaderExist)
		{
			MetaResource shaderMetaResource;
			shaderMetaResource.Deserialize(inStream);
			inStream.SeekToStart();
			ResourceRegistry& resourceRegistry = GetResourceRegistry();
			if (resourceRegistry.IsResourceOutOfDate(ResourceType::Shader, shaderMetaResource.GetGUID()))
			{
				inStream.CloseFile();
				reloadShader = true;
			}
		}
		else
		{
			m_pErrorManager->AddString(StringL::Format("Could not find compiled shader: %s", combinedName.Data()));
		}

		if (!doesCompiledShaderExist || reloadShader)
		{
			InitCompiler();
			if (!m_compiler->CompileSpecificShader(shaderName, shaderStage))
			{
				DeInitCompiler();
				H_ERROR(StringL::Format("Failed to load shader: %s", shaderName));
				StringL pathOfCompiledShader;
				pathOfCompiledShader.Reserve(compiledShaderPath.Length());
				FromWCharToConstChar(compiledShaderPath.Data(), pathOfCompiledShader.Data(), compiledShaderPath.Length());
				m_pErrorManager->AddString(StringL::Format("Failed to compile shader: %s, Compiled Shader path: %s", shaderName, pathOfCompiledShader.Data()));
				return nullptr;
			}
			DeInitCompiler();
		}

		if (!inStream.GetIsFileOpened() && !inStream.OpenFile(compiledShaderPath, FILE_OPEN_TYPE::READ, true))
		{
			StringL failedPath;
			failedPath.Reserve(compiledShaderPath.Length());
			FromWCharToConstChar(compiledShaderPath.Data(), failedPath.Data(), compiledShaderPath.Length());
			m_pErrorManager->AddString(StringL::Format("Failed to open shader path: %s", failedPath.Data()));
			return nullptr;
		}

		CompiledShader& shader = m_loadedShaders[(uint32)shaderStage].Add();

		shader.m_metaData.Deserialize(inStream);
		inStream.Read((char*)&shader.header, sizeof(ShaderHeader));

		shader.compiledCode = new char[shader.header.sizeOfShaderData];
		inStream.Read((char*)shader.compiledCode, sizeof(char) * shader.header.sizeOfShaderData);
		inStream.CloseFile();
		shader.shaderName = shaderName;
		ParseShader(shader.reflectedShaderData, shaderStage, shader.shaderName.Data(), shader.compiledCode, shader.header.sizeOfShaderData);

		shader.loadState = eShaderLoadState::LoadedToRAM;
		shader.m_nameHash = shaderHash;

		if (shaderStage == eShaderStage::Compute)
		{
			glm::uvec3 workGroupSize = shader.reflectedShaderData.m_entryDecorations.workGroupSize;
			const uint32 workGroupTotalInvocations = workGroupSize.x * workGroupSize.y * workGroupSize.z;
			if (workGroupTotalInvocations >= m_renderDevice->GetDeviceLimits().m_maxComputeWorkGroupInvocations)
			{
				H_ERROR(StringL::Format("Local shader %s invocation size %u is larger than allowed. Limit is: %u", shader.shaderName.Data(), workGroupTotalInvocations, m_renderDevice->GetDeviceLimits().m_maxComputeWorkGroupInvocations));
				shader.reflectedShaderData.m_bIsValid = false;
			}
		}

		// Add the created shader to the registry
		ResourceRegistry& resourceRegistry = GetResourceRegistry();
		resourceRegistry.AddToRegistry(compiledShaderPath, ResourceType::Shader);

		if (!shader.reflectedShaderData.m_bIsValid)
		{
			shader.loadState = eShaderLoadState::Unloaded;
			SAFEDELETE_ARRAY(shader.compiledCode);
			resourceRegistry.SetResourceLoadFailed(ResourceType::Shader, shader.m_metaData.GetGUID());

			return nullptr;
		}
		resourceRegistry.SetResourceLoaded(ResourceType::Shader, shader.m_metaData.GetGUID());

		return &shader;
	}

	void MaterialManager::InitCompiler()
	{
		if (m_compiler)
		{
			return;
		}
		m_compiler = new ShaderCompiler();
	#ifdef PLATFORM_WINDOWS
		m_compiler->Init(SHADERLANGUAGETARGET::GLSL);
	#elif PLATFORM_OSX
		compiler.Init(SHADERLANGUAGETARGET::METAL);
	#endif 
	}

	void MaterialManager::DeInitCompiler()
	{
		SAFEDELETE(m_compiler)
	}

	ResourceValidator& MaterialManager::GetDefaultMaterialValidator(eMaterialType type)
	{
		if (type == eMaterialType::SPRITE)
			return m_defaultSpriteMaterialsInstanceValidationData;
		if (type == eMaterialType::MODEL3D)
			return m_default3DMaterialsInstanceValidationData;
	}

	void MaterialManager::ClearAllResources()
	{
		m_loadRequests.RemoveAll();

		for (uint32 i = 0; i < (uint32)eMaterialType::COUNT; i++)
		{
			if (m_MaterialTypeObjects[i])
			{
				m_MaterialTypeObjects[i]->CleanupResource(*m_renderDevice);
				SAFEDELETE(m_MaterialTypeObjects[i]);
			}

			for (uint32 iMat = 0; iMat < m_materials[i].Size(); iMat++)
			{
				m_materials[i][iMat]->CleanupResource(*m_renderDevice);
			}
		}
	}

	FilePath MaterialManager::CreateMaterial(const FilePath& outPath, const String64& name, eMaterialType type) const
	{
		FilePath finalPath = outPath + FileObject(name.Data(), "mat", outPath);
		InOutStream outStream;
		outStream.OpenFile(finalPath, FILE_OPEN_TYPE::WRITE, true);


		outStream.CloseFile();
		outStream.OpenFile(finalPath, FILE_OPEN_TYPE::APPENDS, true);
		finalPath.UpdateCommonFileData();
		
		MetaResource metaResource;
		metaResource.ConstructResourceAndID(finalPath, finalPath, GuidZero);
		metaResource.Serialize(outStream);

		SerializeableMaterial resource{};
		switch (type)
		{
		case Hail::eMaterialType::SPRITE:
			resource.m_shaders[0].m_type = eShaderStage::Vertex;
			resource.m_shaders[1].m_type = eShaderStage::Fragment;
			break;
		case Hail::eMaterialType::MODEL3D:
			resource.m_shaders[0].m_type = eShaderStage::Vertex;
			resource.m_shaders[1].m_type = eShaderStage::Fragment;
			break;
		case Hail::eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
		case Hail::eMaterialType::COUNT:
			H_ERROR(StringL::Format("Create material type is incorrect for material: %s", name));
			break;
		default:
			break;
		}
		outStream.Write(&resource, sizeof(SerializeableMaterial));
		GetResourceRegistry().AddToRegistry(finalPath, ResourceType::Material);

		return finalPath;
	}

	void MaterialManager::ExportMaterial(MaterialResourceContextObject& materialToExport)
	{
		FilePath finalPath = materialToExport.m_metaResource.GetProjectFilePath().GetFilePath();
		InOutStream outStream;
		outStream.OpenFile(finalPath, FILE_OPEN_TYPE::READ_WRITE, true);
		outStream.SeekToStart();
		// First write meta resource, always do that for resources.
		materialToExport.m_metaResource.Serialize(outStream);
		outStream.Write(&materialToExport.m_materialObject, sizeof(SerializeableMaterial));
		outStream.CloseFile();
	}

	void MaterialManager::LoadMaterialMetaData(const FilePath& materialPath, MetaResource& resourceToFill)
	{
		if (!StringCompare(materialPath.Object().Extension(), L"mat"))
			return;

		InOutStream inStream;
		if (!inStream.OpenFile(materialPath.Data(), FILE_OPEN_TYPE::READ, true))
			return;
		resourceToFill.Deserialize(inStream);
	}

	SerializeableMaterial MaterialManager::LoadMaterialSerializeableInstance(const FilePath& materialPath)
	{
		SerializeableMaterial returnResource;
		if (!StringCompare(materialPath.Object().Extension(), L"mat"))
			return returnResource;

		InOutStream inStream;
		if (!inStream.OpenFile(materialPath.Data(), FILE_OPEN_TYPE::READ, true))
			return returnResource;

		MetaResource materialMetaResource;
		materialMetaResource.Deserialize(inStream);
		inStream.Read(&returnResource, sizeof(SerializeableMaterial));
		return returnResource;
	}

	MetaResource MaterialManager::LoadShaderMetaData(const FilePath& shaderPath)
	{
		InOutStream inStream;
		if (!inStream.OpenFile(shaderPath.Data(), FILE_OPEN_TYPE::READ, true))
			return MetaResource();
		MetaResource returnResource;
		returnResource.Deserialize(inStream);
		return returnResource;
	}

	CompiledShader* MaterialManager::GetDefaultCompiledLoadedShader(eShaderStage shaderStage)
	{
		return &m_loadedShaders[(uint32)shaderStage][0];
	}

	CompiledShader* MaterialManager::GetCompiledLoadedShader(GUID shaderGUID)
	{
		// TODO: better data structure
		for (uint32 iType = 0; iType < (uint32)eShaderStage::None; iType++)
		{
			for (uint32 i = 0; i < m_loadedShaders[iType].Size(); i++)
			{
				if (m_loadedShaders[iType][i].m_metaData.GetGUID() == shaderGUID)
				{
					return &m_loadedShaders[iType][i];
				}
			}
		}

		return nullptr;
	}

	CompiledShader* MaterialManager::LoadShaderResource(GUID shaderGUID)
	{
		ResourceRegistry& reg = GetResourceRegistry();
		const FilePath shaderSourcePath = reg.GetSourcePath(ResourceType::Shader, shaderGUID);
		return LoadShader(shaderSourcePath.Object().Name().ToCharString(), eShaderStage::None, false, shaderSourcePath.Object().Extension().ToCharString());
	}

	FilePath MaterialManager::ImportShaderResource(const FilePath& filepath)
	{
		CompiledShader* pShader = LoadShader(filepath.Object().Name().ToCharString(), eShaderStage::None, false, filepath.Object().Extension().ToCharString());
		if (pShader)
		{
			return pShader->m_metaData.GetProjectFilePath().GetFilePath();
		}
		return FilePath();
	}

	FilePath MaterialManager::ImportShaderResource(const FilePath& filepath, eShaderStage shaderStage, bool bForceReload)
	{
		CompiledShader* pShader = LoadShader(filepath.Object().Name().ToCharString(), shaderStage, bForceReload, filepath.Object().Extension().ToCharString());
		if (pShader)
		{
			return pShader->m_metaData.GetProjectFilePath().GetFilePath();
		}
		return FilePath();
	}

	const bool MaterialManager::IsShaderValidWithMaterialType(eMaterialType materialType, eShaderStage shaderStage, ReflectedShaderData& shaderDataToCheck) const
	{
		const ReflectedShaderData* defaultShader = GetDefaultShaderData(materialType, shaderStage);
		if (!defaultShader)
			return false;

		if (defaultShader->m_globalMaterialDecorations.Size() != shaderDataToCheck.m_globalMaterialDecorations.Size())
		{
			return false;
		}

		for (size_t i = 0; i < defaultShader->m_globalMaterialDecorations.Size(); i++)
		{
			if (shaderDataToCheck.m_globalMaterialDecorations.Find(defaultShader->m_globalMaterialDecorations[i]) == -1)
			{
				return false;
			}
		}
		return true;
	}

	const ReflectedShaderData* MaterialManager::GetDefaultShaderData(eMaterialType materialType, eShaderStage shaderStage) const
	{
		if (m_defaultShaders[(uint32)materialType][0]->header.shaderType == (uint8)shaderStage)
		{
			return &m_defaultShaders[(uint32)materialType][0]->reflectedShaderData;
		}
		if (m_defaultShaders[(uint32)materialType].Size() > 1 && m_defaultShaders[(uint32)materialType][1]->header.shaderType == (uint8)shaderStage)
		{
			return &m_defaultShaders[(uint32)materialType][1]->reflectedShaderData;
		}
		return nullptr;
	}
}
