#include "Engine_PCH.h"
#include "TextureManager.h"

#include "DebugMacros.h"
#include "TextureCompiler.h"

#include "Utility\FileSystem.h"
#include "Utility\StringUtility.h"
#include "Utility\InOutStream.h"

#include "MetaResource.h"
#include "HailEngine.h"
#include "ResourceRegistry.h"
#include "Rendering\RenderContext.h"
#include "Rendering\RenderDevice.h"
#include "Resources_Textures\TextureCommons.h"

namespace
{

	//TODO: Move the memory that is used here to a temporary memory buffer
	void Read8BitStream(Hail::InOutStream& stream, void** outData, const uint32_t numberOfBytesToRead)
	{
		*outData = new uint8_t[numberOfBytesToRead];
		unsigned char* pixels = (unsigned char*)malloc(numberOfBytesToRead);
		stream.Read((char*)&pixels[0], numberOfBytesToRead);
		memcpy(*outData, pixels, numberOfBytesToRead);
		free(pixels);
	}
	void Read16BitStream(Hail::InOutStream& stream, void** outData, const uint32_t numberOfBytesToRead)
	{
		*outData = new uint16_t[numberOfBytesToRead];
		uint16_t* pixels = (uint16_t*)malloc(numberOfBytesToRead);
		stream.Read((char*)&pixels[0], numberOfBytesToRead);
		memcpy(*outData, pixels, numberOfBytesToRead);
		free(pixels);
	}
	void Read32UintBitStream(Hail::InOutStream& stream, void** outData, const uint32_t numberOfBytesToRead)
	{
		*outData = new uint32_t[numberOfBytesToRead];
		uint32_t* pixels = (uint32_t*)malloc(numberOfBytesToRead);
		stream.Read((char*)&pixels[0], numberOfBytesToRead);
		memcpy(*outData, pixels, numberOfBytesToRead);
		free(pixels);
	}
	void Read32FltBitStream(Hail::InOutStream& stream, void** outData, const uint32_t numberOfBytesToRead)
	{
		*outData = new float[numberOfBytesToRead];
		float* pixels = (float*)malloc(numberOfBytesToRead);
		stream.Read((char*)&pixels[0], numberOfBytesToRead);
		memcpy(*outData, pixels, numberOfBytesToRead);
		free(pixels);
	}
}

using namespace Hail;

void TextureManager::Init(RenderContext* pRenderContext)
{
	CreateDefaultTexture(pRenderContext);
}

void Hail::TextureManager::ClearAllResources()
{
	m_defaultTexture.m_pView->CleanupResource(m_device);
	m_defaultTexture.m_pTexture->CleanupResource(m_device);
	for (size_t i = 0; i < m_loadedTextures.Size(); i++)
	{
		m_loadedTextures[i].m_pTexture->CleanupResource(m_device);
		m_loadedTextures[i].m_pView->CleanupResource(m_device);
	}
	m_loadedTextures.RemoveAll();
}

Hail::TextureManager::TextureManager(RenderingDevice* pDevice) :m_device(pDevice)
{
}

void Hail::TextureManager::ReloadAllTextures(uint32 frameInFlight)
{
	for (uint32 i = 0; i < m_loadedTextures.Size(); i++)
	{
		if (!ReloadTextureInternal(i, frameInFlight))
		{
			H_ASSERT(false, "Failed to reload texture");
		}
	}
}

void TextureManager::Update(RenderContext* pRenderContext)
{
	//TODO: Add file watcher here for dynamic hot reloading

	if (m_loadRequests.Empty())
		return;

	for (uint32 i = 0; i < m_loadRequests.Size(); i++)
	{
		TextureWithView& loadedTexture = m_loadedTextures[m_loadRequests[i].loadedIndex];
		bool successfulLoad = false;
		if (CreateTextureGPUData(pRenderContext, m_loadRequests[i].compiledTextureData, loadedTexture.m_pTexture))
		{
			pRenderContext->UploadDataToTexture(loadedTexture.m_pTexture, m_loadRequests[i].compiledTextureData.compiledColorValues, 0);
			successfulLoad = true;

			TextureViewProperties props{};
			props.pTextureToView = loadedTexture.m_pTexture;
			props.viewUsage = eTextureUsage::Texture;
			props.accessQualifier = eShaderAccessQualifier::ReadOnly;
			if (!loadedTexture.m_pView->InitView(m_device, props))
			{
				successfulLoad = false;
			}
		}

		if (successfulLoad)
			GetResourceRegistry().SetResourceLoaded(ResourceType::Texture, loadedTexture.m_pTexture->m_metaResource.GetGUID());
		else
		{
			loadedTexture.m_pView->CleanupResource(m_device);
			SAFEDELETE(loadedTexture.m_pView);
			loadedTexture.m_pTexture->CleanupResource(m_device);
			GetResourceRegistry().SetResourceLoadFailed(ResourceType::Texture, loadedTexture.m_pTexture->m_metaResource.GetGUID());
			SAFEDELETE(loadedTexture.m_pTexture);
			H_ERROR(StringL::Format("Failed to create texture: %s", m_loadRequests[i].textureName));
		}

		DeleteCompiledTexture(m_loadRequests[i].compiledTextureData);
		ClearTextureGPULoadRequest(m_loadRequests[i]);
	}
	m_loadRequests.RemoveAll();
}

bool Hail::TextureManager::LoadTexture(const char* textureName)
{
	MetaResource metaData;
	//TODO: hook up meta data to engine handling of resource'
	CompiledTexture compiledTextureData = LoadTextureInternal(textureName, metaData, false);
	if (compiledTextureData.loadState == TEXTURE_LOADSTATE::LOADED_TO_RAM)
	{
		TextureWithView textureAndView{};
		textureAndView.m_pTexture = CreateTextureInternal(textureName, compiledTextureData);
		textureAndView.m_pTexture->m_index = TextureResource::g_idCounter++;
		textureAndView.m_pTexture->m_metaResource = metaData;

		textureAndView.m_pView = CreateTextureView();

		TextureViewProperties props{};
		props.pTextureToView = textureAndView.m_pTexture;
		props.viewUsage = eTextureUsage::Texture;
		props.accessQualifier = eShaderAccessQualifier::ReadOnly;
		if (!textureAndView.m_pView->InitView(m_device, props))
		{
			textureAndView.m_pView->CleanupResource(m_device);
			SAFEDELETE(textureAndView.m_pView);
		}

		if (textureAndView.m_pView && textureAndView.m_pTexture)
		{
			GetResourceRegistry().SetResourceLoaded(ResourceType::Texture, metaData.GetGUID());

			m_loadedTextures.Add(textureAndView);
			return true;
		}
		else
		{
			GetResourceRegistry().SetResourceLoadFailed(ResourceType::Texture, metaData.GetGUID());
			textureAndView.m_pTexture->CleanupResource(m_device);
			SAFEDELETE(textureAndView.m_pTexture);
		}
	}
	return false;
}

uint32 Hail::TextureManager::StagerredTextureLoad(GUID textureID)
{
	if (GetResourceRegistry().GetIsResourceLoaded(ResourceType::Texture, textureID))
	{
		for (uint32 i = 0; i < m_loadedTextures.Size(); i++)
		{
			const TextureResource* pTexture = m_loadedTextures[i].m_pTexture;
			if (pTexture->m_metaResource.GetGUID() == textureID)
				return pTexture->m_index;
		}
	}

	TextureResource* pTexture = LoadTextureRequestInternal(GetResourceRegistry().GetProjectPath(ResourceType::Texture, textureID));

	if (pTexture)
	{
		return pTexture->m_index;
	}

	return INVALID_TEXTURE_HANDLE;
}


void Hail::TextureManager::ClearTextureInternalForReload(int textureIndex, uint32 frameInFlight)
{
	m_loadedTextures[textureIndex].m_pTexture->m_validator.MarkResourceAsDirty(frameInFlight);
	if (m_loadedTextures[textureIndex].m_pTexture->m_compiledTextureData.loadState == TEXTURE_LOADSTATE::LOADED_TO_RAM)
	{
		DeleteCompiledTexture(m_loadedTextures[textureIndex].m_pTexture->m_compiledTextureData);
	}
}

bool Hail::TextureManager::ReloadTextureInternal(int textureIndex, uint32 frameInFlight)
{
	ClearTextureInternalForReload(textureIndex, frameInFlight);
	if (m_loadedTextures[textureIndex].m_pTexture->m_validator.IsAllFrameResourcesDirty())
	{
		//TODO: hook up meta data to engine handling of resource
		m_loadedTextures[textureIndex].m_pTexture->m_compiledTextureData = LoadTextureInternal(m_loadedTextures[textureIndex].m_pTexture->textureName, m_loadedTextures[textureIndex].m_pTexture->m_metaResource, true);
		if (m_loadedTextures[textureIndex].m_pTexture->m_compiledTextureData.loadState != TEXTURE_LOADSTATE::LOADED_TO_RAM)
		{
			//TODO fix errors here
			return false;
		}
	}
	m_loadedTextures[textureIndex].m_pTexture->m_validator.ClearFrameData(frameInFlight);
	return true;
}

CompiledTexture TextureManager::LoadTextureInternal(const char* textureName, MetaResource& metaResourceToFill, bool reloadTexture)
{
	CompiledTexture returnTexture;
	String64 inPathName = String64::Format("%s.txr", textureName);
	WString64 inPathNameW;
	FromConstCharToWChar(inPathName.Data(), inPathNameW.Data(), 64u);
	const FilePath inPath = FilePath::GetTextureCompiledDirectory() + inPathNameW.Data();
	InOutStream inStream;

	if (!inStream.OpenFile(inPath.Data(), FILE_OPEN_TYPE::READ, true) || reloadTexture)
	{
		if (!CompileTexture(textureName))
		{
			return returnTexture;
		}
	}
	inStream.OpenFile(inPath.Data(), FILE_OPEN_TYPE::READ, true);

	if (!ReadStreamInternal(returnTexture, inStream, metaResourceToFill))
		return returnTexture;

	inStream.CloseFile();

	returnTexture.loadState = TEXTURE_LOADSTATE::LOADED_TO_RAM;
	return returnTexture;
}

TextureResource* Hail::TextureManager::LoadTextureRequestInternal(const FilePath& path)
{
	// TODO move this data streaming to a dedicated load thread and do this with a request
	InOutStream inStream;

	MetaResource metaData;
	CompiledTexture compiledTextureData;
	if (!inStream.OpenFile(path, FILE_OPEN_TYPE::READ, true))
		return nullptr;

	bool bReadResult = ReadStreamInternal(compiledTextureData, inStream, metaData);
	inStream.CloseFile();
	
	if (!bReadResult)
		return nullptr;

	compiledTextureData.loadState = TEXTURE_LOADSTATE::LOADED_TO_RAM;

	TextureWithView textureAndView{};
	if (compiledTextureData.loadState == TEXTURE_LOADSTATE::LOADED_TO_RAM)
	{
		const char* pTextureName = path.Object().Name().ToCharString();
		textureAndView.m_pTexture= CreateTextureInternalNoLoad();
		textureAndView.m_pTexture->textureName = pTextureName;
		textureAndView.m_pTexture->m_metaResource = metaData;
		textureAndView.m_pTexture->m_index = TextureResource::g_idCounter++;
		textureAndView.m_pTexture->m_properties = compiledTextureData.properties;
		textureAndView.m_pView = CreateTextureView();
		m_loadRequests.Add({ pTextureName, compiledTextureData, (uint32)m_loadedTextures.Size() });

		m_loadedTextures.Add(textureAndView);
	}
	return textureAndView.m_pTexture;
}

bool Hail::TextureManager::ReadStreamInternal(CompiledTexture& textureToFill, InOutStream& inStream, MetaResource& metaResourceToFill) const
{
	inStream.Read((char*)&textureToFill.properties, TextureHeaderSize);
	switch (ToEnum<eTextureSerializeableType>(textureToFill.properties.textureType))
	{
	case eTextureSerializeableType::R8G8B8A8_SRGB:
		Read8BitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R8G8B8_SRGB:
	{
		void* tempData = nullptr;
		Read8BitStream(inStream, &tempData, GetTextureByteSize(textureToFill.properties));
		textureToFill.properties.textureType = static_cast<uint32_t>(eTextureSerializeableType::R8G8B8A8_SRGB);
		textureToFill.compiledColorValues = new uint8_t[1 * 4 * textureToFill.properties.width * textureToFill.properties.height];
		uint32_t rgbIterator = 0;
		for (size_t i = 0; i < 4 * textureToFill.properties.width * textureToFill.properties.height; i++)
		{
			switch (i % 4)
			{
			case 0:
				((uint8_t*)textureToFill.compiledColorValues)[i] = static_cast<uint8_t*>(tempData)[rgbIterator++];
				break;
			case 1:
				((uint8_t*)textureToFill.compiledColorValues)[i] = static_cast<uint8_t*>(tempData)[rgbIterator++];
				break;
			case 2:
				((uint8_t*)textureToFill.compiledColorValues)[i] = static_cast<uint8_t*>(tempData)[rgbIterator++];
				break;
			case 3:
				((uint8_t*)textureToFill.compiledColorValues)[i] = 255;
				break;
			}
		}
		delete[] tempData;
	}
	break;
	case eTextureSerializeableType::R8G8B8A8:
		Read8BitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R8G8B8:
	{
		void* tempData = nullptr;
		Read8BitStream(inStream, &tempData, GetTextureByteSize(textureToFill.properties));
		textureToFill.properties.textureType = static_cast<uint32_t>(eTextureSerializeableType::R8G8B8A8);
		textureToFill.compiledColorValues = new uint8_t[1 * 4 * textureToFill.properties.width * textureToFill.properties.height];
		uint32_t rgbIterator = 0;
		for (size_t i = 0; i < 4 * textureToFill.properties.width * textureToFill.properties.height; i++)
		{
			switch (i % 4)
			{
			case 0:
				((uint8_t*)textureToFill.compiledColorValues)[i] = static_cast<uint8_t*>(tempData)[rgbIterator++];
				break;
			case 1:
				((uint8_t*)textureToFill.compiledColorValues)[i] = static_cast<uint8_t*>(tempData)[rgbIterator++];
				break;
			case 2:
				((uint8_t*)textureToFill.compiledColorValues)[i] = static_cast<uint8_t*>(tempData)[rgbIterator++];
				break;
			case 3:
				((uint8_t*)textureToFill.compiledColorValues)[i] = 255;
				break;
			}
		}
		delete[] tempData;
	}
	break;
	case eTextureSerializeableType::R16G16B16A16:
		Read16BitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R16G16B16:
		Read16BitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R16:
		Read16BitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R32G32B32A32:
		Read32UintBitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R32G32B32:
		Read32UintBitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	case eTextureSerializeableType::R32:
		Read32UintBitStream(inStream, &textureToFill.compiledColorValues, GetTextureByteSize(textureToFill.properties));
		break;
	default:
		return false;
		break;
	}
	const uint64 sizeLeft = inStream.GetFileSize() - inStream.GetFileSeekPosition();
	if (sizeLeft != 0)
	{
		metaResourceToFill.Deserialize(inStream);
	}

	textureToFill.properties.format = SerializeableTextureTypeToTextureFormat((eTextureSerializeableType)textureToFill.properties.textureType);

	return true;
}

bool TextureManager::CompileTexture(const char* textureName)
{
	RecursiveFileIterator fileIterator = RecursiveFileIterator(FilePath::GetTextureResourceSourceDirectory());
	bool foundTexture = false;

	FilePath currentPath;
	String64 filenameStr;
	while (fileIterator.IterateOverFolderRecursively())
	{
		currentPath = fileIterator.GetCurrentPath();
		if (currentPath.IsFile())
		{
			currentPath = fileIterator.GetCurrentPath();
			const FileObject& currentFileObject = currentPath.Object();
			FromWCharToConstChar(currentFileObject.Name(), filenameStr, 64);
			if (StringCompare(filenameStr, textureName) 
				&& StringCompare(L"tga", currentFileObject.Extension()) 
				|| StringCompare(L"TGA", currentFileObject.Extension()))
			{
				foundTexture = true;
				break;
			}
		}
	}

	if (!foundTexture)
	{
		Debug_PrintConsoleConstChar(StringL::Format("Could not find texture : %s", textureName));
		return false;
	}
	FilePath projectPath = TextureCompiler::CompileSpecificTGATexture(currentPath, GuidZero);
	if (projectPath.IsValid())
	{
		GetResourceRegistry().AddToRegistry(projectPath, ResourceType::Texture);
		return true;
	}
	return false;
}

void Hail::TextureManager::ClearTextureGPULoadRequest(TextureGPULoadRequest& requestToClear)
{
	requestToClear.textureName.Clear();
	requestToClear.compiledTextureData = CompiledTexture();
	requestToClear.loadedIndex = MAX_UINT;
}

TextureResource* Hail::TextureManager::GetTexture(uint32 index)
{
	for (size_t i = 0; i < m_loadedTextures.Size(); i++)
	{
		if (m_loadedTextures[i].m_pTexture->m_index == index)
			return m_loadedTextures[i].m_pTexture;
	}

	return nullptr;
}

TextureView* Hail::TextureManager::GetTextureView(uint32 index)
{
	for (size_t i = 0; i < m_loadedTextures.Size(); i++)
	{
		if (m_loadedTextures[i].m_pTexture->m_index == index)
			return m_loadedTextures[i].m_pView;
	}

	return nullptr;
}

TextureView* Hail::TextureManager::GetEngineTextureView(eDecorationSets setDomainToGet, uint32 bindingIndex, uint32 frameInFlight)
{
	// TODO: Assert on setDomain
	if (setDomainToGet == eDecorationSets::GlobalDomain)
	{

	}
	else if (setDomainToGet == eDecorationSets::MaterialTypeDomain)
	{
		return m_materialTextures[bindingIndex][frameInFlight].m_pView;
	}
	return nullptr;
}

void Hail::TextureManager::RegisterEngineTexture(TextureResource* pTexture, TextureView* pTextureView, eDecorationSets setDomain, uint32 bindingIndex, uint32 frameInFlight)
{
	// TODO: Assert on setDomain
	if (setDomain == eDecorationSets::GlobalDomain)
	{

	}
	else if (setDomain == eDecorationSets::MaterialTypeDomain)
	{
		TextureWithView textureAndView{};
		textureAndView.m_pTexture = pTexture;
		textureAndView.m_pView = pTextureView;
		m_materialTextures[bindingIndex][frameInFlight] = textureAndView;
	}
}

TextureResource* Hail::TextureManager::CreateTexture(RenderContext* pRenderContext, const char* name, CompiledTexture& compiledTextureData)
{
	TextureResource* pTexture = CreateTextureInternalNoLoad();
	pTexture->textureName = name;
	pTexture->m_index = TextureResource::g_idCounter++;
	pTexture->m_properties = compiledTextureData.properties;
	H_ASSERT(CreateTextureGPUData(pRenderContext, compiledTextureData, pTexture), "Failed to create texture");
	pRenderContext->UploadDataToTexture(pTexture, compiledTextureData.compiledColorValues, 0);
	return pTexture;
}

TextureResource* Hail::TextureManager::CreateTexture(RenderContext* pRenderContext, const char* name, TextureProperties& textureProperties)
{
	TextureResource* pTexture = CreateTextureInternalNoLoad();
	pTexture->textureName = name;
	pTexture->m_index = TextureResource::g_idCounter++;
	pTexture->m_properties = textureProperties;
	CompiledTexture compiledTextureData{};
	compiledTextureData.properties = textureProperties;
	compiledTextureData.loadState = TEXTURE_LOADSTATE::LOADED_TO_RAM;
	compiledTextureData.compiledColorValues = nullptr;
	H_ASSERT(CreateTextureGPUData(pRenderContext, compiledTextureData, pTexture), "Failed to create texture");
	pRenderContext->TransferTextureLayout(pTexture, textureProperties.accessQualifier, 0u);
	return pTexture;
}

TextureView* Hail::TextureManager::CreateTextureView(const RelativeFilePath filepath, RenderContext* pRenderContext)
{
	ResourceRegistry& registry = GetResourceRegistry();
	const FilePath finalPath = filepath.GetFilePath();
	if (const MetaResource* pResource = registry.GetResourceMetaInformation(ResourceType::Texture, finalPath))
	{
		if (registry.GetIsResourceLoaded(ResourceType::Texture, pResource->GetGUID()))
		{
			for (uint32 i = 0; i < m_loadedTextures.Size(); i++)
			{
				const TextureWithView& textureView = m_loadedTextures[i];

				if (textureView.m_pTexture->m_metaResource.GetGUID() == pResource->GetGUID())
					return textureView.m_pView;
			}
			H_ASSERT(false, "Texture should be loaded in the texture manager, bug in the application");
		}
		else
		{
			H_ASSERT(LoadTextureRequestInternal(finalPath), "The texture does exist, but failed to create a load request.");
			Update(pRenderContext);
			H_ASSERT(m_loadedTextures.GetLast().m_pTexture->m_metaResource.GetGUID() == pResource->GetGUID(), "Logic error in the application if the last request is not the one we just created.");
			return m_loadedTextures.GetLast().m_pView;
		}
	}

	// Texture is not loaded, compile texture here, Fail if we get here in the final build
	FilePath textureBaseResourcePath = FilePath::GetTextureResourceSourceDirectory();
	textureBaseResourcePath = textureBaseResourcePath + finalPath.Object();
	textureBaseResourcePath.ReplaceExtension(L"tga");
	FilePath projectPath = ImportTextureResource(textureBaseResourcePath);
	if (projectPath.IsValid())
	{
		return CreateTextureView(filepath, pRenderContext);
	}
	return nullptr;
}


FilePath Hail::TextureManager::ImportTextureResource(const FilePath& filepath) const
{
	FilePath projectPath = TextureCompiler::CompileSpecificTGATexture(filepath, GuidZero);
	if (projectPath.IsValid())
		GetResourceRegistry().AddToRegistry(projectPath, ResourceType::Texture);
	else
	{
		H_ERROR(StringL::Format("Failed to load texture: %s", filepath.Object().Name().ToCharString()));
	}
	return projectPath;
}


bool Hail::TextureManager::ReloadImportTextureResource(const FilePath& filePath)
{
	ResourceRegistry& registry = GetResourceRegistry();
	const MetaResource* pTextureMetaResource = registry.GetResourceMetaInformation(ResourceType::Texture, filePath);
	if (pTextureMetaResource)
		return ReloadImportTextureResource(pTextureMetaResource->GetGUID());

	return false;
}

bool Hail::TextureManager::ReloadImportTextureResource(GUID textureID)
{
	ResourceRegistry& registry = GetResourceRegistry();

	if (!registry.GetIsResourceImported(ResourceType::Texture, textureID))
		return false;

	if (!registry.IsResourceOutOfDate(ResourceType::Texture, textureID))
		return false;

	FilePath sourcePath = registry.GetSourcePath(ResourceType::Texture, textureID);
	if (!sourcePath.IsValid())
		return false;

	FilePath projectPath = TextureCompiler::CompileSpecificTGATexture(sourcePath, textureID);
	return projectPath.IsValid();

	// TODO: Reload the resource for the rendering here:
}


TextureView* Hail::TextureManager::CreateTextureView(TextureViewProperties& viewProps)
{
	TextureView* pView = CreateTextureView();
	H_ASSERT(pView->InitView(m_device, viewProps));
	return pView;
}

void Hail::TextureManager::LoadTextureMetaData(const FilePath& filePath, MetaResource& metaResourceToFill)
{
	if (!StringCompare(filePath.Object().Extension(), L"txr"))
		return;

	InOutStream inStream;
	if (!inStream.OpenFile(filePath.Data(), FILE_OPEN_TYPE::READ, true))
		return;

	CompiledTexture textureToFill;
	inStream.Read((char*)&textureToFill.properties, TextureHeaderSize);
	inStream.Seek(GetTextureByteSize(textureToFill.properties), 1);

	const uint64 sizeLeft = inStream.GetFileSize() - inStream.GetFileSeekPosition();
	if (sizeLeft != 0)
	{
		metaResourceToFill.Deserialize(inStream);
	}
}

void Hail::TextureManager::CreateDefaultTexture(RenderContext* pRenderContext)
{
	const uint8 widthHeight = 16;

	CompiledTexture compiledTextureData;
	compiledTextureData.properties.height = widthHeight;
	compiledTextureData.properties.width = widthHeight;
	compiledTextureData.properties.textureType = (uint32)eTextureSerializeableType::R8G8B8A8_SRGB;
	compiledTextureData.compiledColorValues = new uint8[GetTextureByteSize(compiledTextureData.properties)];
	compiledTextureData.loadState = TEXTURE_LOADSTATE::LOADED_TO_RAM;
	struct Color
	{
		uint8 r = 255;
		uint8 g = 255;
		uint8 b = 255;
		uint8 a = 255;
	};

	Color color1;
	color1.r = 89;
	color1.g = 104;
	color1.b = 128;

	Color color2;
	color2.r = 41;
	color2.g = 51;
	color2.b = 42;

	//multiply with 4, so we cover each color
	for (uint8 x = 0; x < widthHeight * 4; x = x + 4)
	{
		const uint8 xIndexValue = (x / 4) % 4;
		for (uint8 y = 0; y < widthHeight * 4; y = y + 4)
		{
			const uint8 yIndexValue = ((y * widthHeight) / 4) % 64;
			if (xIndexValue == 0 || xIndexValue == 1)
			{
				if ((yIndexValue == 0 || yIndexValue == 16))
					memcpy(&((uint8*)compiledTextureData.compiledColorValues)[x + y * widthHeight], &color1, sizeof(Color));
				else
					memcpy(&((uint8*)compiledTextureData.compiledColorValues)[x + y * widthHeight], &color2, sizeof(Color));
			}
			else
			{
				if ((yIndexValue == 32 || yIndexValue == 48))
					memcpy(&((uint8*)compiledTextureData.compiledColorValues)[x + y * widthHeight], &color1, sizeof(Color));
				else
					memcpy(&((uint8*)compiledTextureData.compiledColorValues)[x + y * widthHeight], &color2, sizeof(Color));
			}

		}
	}

	pRenderContext->StartTransferPass();

	m_defaultTexture.m_pTexture = CreateTextureInternalNoLoad();
	m_defaultTexture.m_pTexture->textureName = "Default Texture";
	m_defaultTexture.m_pTexture->m_index = MAX_UINT - 1;
	m_defaultTexture.m_pTexture->m_properties.height = widthHeight;
	m_defaultTexture.m_pTexture->m_properties.width = widthHeight;
	m_defaultTexture.m_pTexture->m_properties.textureType = compiledTextureData.properties.textureType;
	m_defaultTexture.m_pTexture->m_properties.format = SerializeableTextureTypeToTextureFormat(eTextureSerializeableType::R8G8B8A8_SRGB);
	H_ASSERT(CreateTextureGPUData(pRenderContext, compiledTextureData, m_defaultTexture.m_pTexture), "Failed to create default texture");
	pRenderContext->UploadDataToTexture(m_defaultTexture.m_pTexture, compiledTextureData.compiledColorValues, 0);
	DeleteCompiledTexture(compiledTextureData);
	m_defaultTexture.m_pView = CreateTextureView();
	TextureViewProperties props{};
	props.pTextureToView = m_defaultTexture.m_pTexture;
	props.viewUsage = eTextureUsage::Texture;
	props.accessQualifier = eShaderAccessQualifier::ReadOnly;
	H_ASSERT(m_defaultTexture.m_pView->InitView(m_device, props));

	pRenderContext->EndTransferPass();
}