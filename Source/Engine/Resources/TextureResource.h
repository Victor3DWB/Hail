#pragma once
#include "Engine/EngineConstants.h"
#include "Resources_Textures\TextureCommons.h"
#include "ResourceCommon.h"

#include "MetaResource.h"

namespace Hail
{
	class RenderingDevice;
	class TextureManager;

	class SamplerObject
	{
	public:
		virtual void Init(RenderingDevice* pDevice, SamplerProperties props) = 0;
		virtual void CleanupResource(RenderingDevice* pDevice) = 0;
		SamplerProperties m_props;
	};

	class TextureResource
	{
	public:
		virtual void CleanupResource(RenderingDevice* device) = 0;
		virtual void CleanupResourceForReload(RenderingDevice* device, uint32 frameInFligth) = 0;
		virtual uint32 GetCurrentStageUsage() = 0;
		bool Init(RenderingDevice* pDevice);

		StringL textureName;
		uint32_t m_index = MAX_UINT;
		CompiledTexture m_compiledTextureData;
		TextureProperties m_properties;
		ResourceValidator m_validator;
		MetaResource m_metaResource;

		// The state with which this resource memory is held on the GPU
		eShaderAccessQualifier m_accessQualifier;

	protected: 

		friend class TextureManager;

		virtual bool InternalInit(RenderingDevice* pDevice) = 0;

		static uint32 g_idCounter;
	};

	struct TextureViewProperties
	{
		TextureResource* pTextureToView;
		eTextureUsage viewUsage;
		eShaderAccessQualifier accessQualifier;
		uint32 lastBoundShaderStages = 0u;
	};

	class TextureView
	{
	public:
		virtual void CleanupResource(RenderingDevice* pDevice) = 0;

		virtual bool InitView(RenderingDevice* pDevice, TextureViewProperties properties) = 0;
		const TextureViewProperties& GetProps() const { return m_props; }
	protected:
		uint32 m_textureIndex{ MAX_UINT };
		TextureViewProperties m_props;
	};

	// Only use for ImGui data
	class ImGuiTextureResource
	{
	public:

		virtual void* GetImguiTextureResource() = 0;
	
		uint32 m_width = 0;
		uint32 m_height = 0;
		String64 textureName;
		MetaResource metaDataOfResource;
	};

}

