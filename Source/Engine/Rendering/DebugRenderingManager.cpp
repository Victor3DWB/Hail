#include "Engine_PCH.h"
#include "DebugRenderingManager.h"

#include "HailEngine.h"
#include "RenderCommands.h"
#include "RenderContext.h"
#include "Renderer.h"
#include "Settings.h"

#include "Resources\MaterialManager.h"
#include "Resources\RenderingResourceManager.h"
#include "Resources\ResourceManager.h"
#include "Resources\ResourceRegistry.h"
#include "Resources\TextureManager.h"

#include "CommandsCommon.h"

using namespace Hail;

Hail::DebugRenderingManager::DebugRenderingManager(Renderer* pRenderer, ResourceManager* pResourceManager)
	: m_pRenderer(pRenderer)
	, m_pResourceManager(pResourceManager)
	, m_pDebugCirclePipeline(nullptr)
	, m_pDebugCircleBuffer(nullptr)
	, m_numberOfCirclesToRender(0u)
	, m_pDebugLineVertexBuffer(nullptr)
	, m_pDebugLineBuffer(nullptr)
	, m_pDebugLinePipeline(nullptr)
	, m_numberOf2DDebugLines(0u)
	, m_numberOf3DDebugLines(0u)
{
}

Hail::DebugRenderingManager::~DebugRenderingManager()
{
	H_ASSERT(m_pDebugCirclePipeline == nullptr);
	H_ASSERT(m_pDebugCircleBuffer == nullptr);

	H_ASSERT(m_pDebugLineVertexBuffer == nullptr);
	H_ASSERT(m_pDebugLineBuffer == nullptr);
	H_ASSERT(m_pDebugLinePipeline == nullptr);
}

void Hail::DebugRenderingManager::Initialize(ErrorManager* pErrorManager)
{
	GrowingArray<uint32> debugLineVertices(MAX_NUMBER_OF_DEBUG_LINES, 0);
	for (uint32 i = 0; i < MAX_NUMBER_OF_DEBUG_LINES; i++)
	{
		debugLineVertices[i] = i;
	}

	BufferProperties debugLineVertexBufferProperties;
	debugLineVertexBufferProperties.elementByteSize = sizeof(uint32_t);
	debugLineVertexBufferProperties.numberOfElements = MAX_NUMBER_OF_DEBUG_LINES;
	debugLineVertexBufferProperties.type = eBufferType::vertex;
	debugLineVertexBufferProperties.domain = eShaderBufferDomain::GpuOnly;
	debugLineVertexBufferProperties.accessQualifier = eShaderAccessQualifier::ReadOnly;
	debugLineVertexBufferProperties.updateFrequency = eShaderBufferUpdateFrequency::Once;
	m_pDebugLineVertexBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(debugLineVertexBufferProperties, "Debug Line Vertex Buffer");

	RenderContext* pContext = m_pRenderer->GetCurrentContext();
	pContext->UploadDataToBuffer(m_pDebugLineVertexBuffer, debugLineVertices.Data(), sizeof(uint32_t) * MAX_NUMBER_OF_DEBUG_LINES);

	BufferProperties debugLineProperties;
	debugLineProperties.type = eBufferType::structured;
	debugLineProperties.numberOfElements = MAX_NUMBER_OF_DEBUG_LINES;
	debugLineProperties.elementByteSize = sizeof(DebugLineData);
	debugLineProperties.domain = eShaderBufferDomain::CpuToGpu;
	debugLineProperties.accessQualifier = eShaderAccessQualifier::ReadOnly;
	debugLineProperties.updateFrequency = eShaderBufferUpdateFrequency::PerFrame;
	m_pDebugLineBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(debugLineProperties, "Debug Line Data Buffer");

	ResourceRegistry& reg = GetResourceRegistry();
	MaterialManager* pMatManager = m_pResourceManager->GetMaterialManager();

	MaterialCreationProperties lineMatProperties{};
	RelativeFilePath vertexLineProjectPath("resources/shaders/VS_DebugLines2D.shr");
	if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
		pMatManager->ImportShaderResource(vertexLineProjectPath.GetFilePath(), eShaderStage::Vertex)))
	{
		lineMatProperties.m_shaders[0].m_id = metaData->GetGUID();
		lineMatProperties.m_shaders[0].m_type = eShaderStage::Vertex;
	}

	RelativeFilePath fragmentLineProjectPath("resources/shaders/FS_DebugLines.shr");
	if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
		pMatManager->ImportShaderResource(fragmentLineProjectPath.GetFilePath(), eShaderStage::Fragment)))
	{
		lineMatProperties.m_shaders[1].m_id = metaData->GetGUID();
		lineMatProperties.m_shaders[1].m_type = eShaderStage::Fragment;
	}

	lineMatProperties.m_baseMaterialType = eMaterialType::CUSTOM;
	lineMatProperties.m_typeRenderPass = eMaterialType::CUSTOM;
	lineMatProperties.m_bIsWireFrame = true;
	m_pDebugLinePipeline = pMatManager->CreateMaterialPipeline(lineMatProperties);

	// Move over the pipelineFrom debug lines to this renderer

	BufferProperties debugPointBufferProps;
	debugPointBufferProps.elementByteSize = sizeof(DebugCircle);
	debugPointBufferProps.numberOfElements = MAX_NUMBER_OF_DEBUG_CIRCLES;
	debugPointBufferProps.type = eBufferType::structured;
	debugPointBufferProps.domain = eShaderBufferDomain::CpuToGpu;
	debugPointBufferProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
	debugPointBufferProps.updateFrequency = eShaderBufferUpdateFrequency::PerFrame;
	m_pDebugCircleBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(debugPointBufferProps, "Debug Circle Point Buffer");

	MaterialCreationProperties circleMatProperties{};
	RelativeFilePath vertexCircleProjectPath("resources/shaders/VS_SpriteParticle2D.shr");
	if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
		pMatManager->ImportShaderResource(vertexCircleProjectPath.GetFilePath(), eShaderStage::Vertex)))
	{
		circleMatProperties.m_shaders[0].m_id = metaData->GetGUID();
		circleMatProperties.m_shaders[0].m_type = eShaderStage::Vertex;
	}

	RelativeFilePath fragmentCircleProjectPath("resources/shaders/FS_DebugPoint2D.shr");
	if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
		pMatManager->ImportShaderResource(fragmentCircleProjectPath.GetFilePath(), eShaderStage::Fragment)))
	{
		circleMatProperties.m_shaders[1].m_id = metaData->GetGUID();
		circleMatProperties.m_shaders[1].m_type = eShaderStage::Fragment;
	}

	circleMatProperties.m_baseMaterialType = eMaterialType::CUSTOM;
	circleMatProperties.m_typeRenderPass = eMaterialType::FULLSCREEN_PRESENT_LETTERBOX;
	m_pDebugCirclePipeline = pMatManager->CreateMaterialPipeline(circleMatProperties);

	if (m_pDebugCirclePipeline == nullptr || m_pDebugLinePipeline == nullptr)
	{
		pErrorManager->AddErrors(EStartupErrors::InitDebugRenderer, EErrorType::Startup);
	}
}

void Hail::DebugRenderingManager::Cleanup()
{
	H_ASSERT(m_pDebugCirclePipeline);
	m_pDebugCirclePipeline->CleanupResource(*m_pRenderer->GetRenderingDevice());
	SAFEDELETE(m_pDebugCirclePipeline);

	H_ASSERT(m_pDebugCircleBuffer);
	m_pDebugCircleBuffer->CleanupResource(m_pRenderer->GetRenderingDevice());
	SAFEDELETE(m_pDebugCircleBuffer);

	H_ASSERT(m_pDebugLineVertexBuffer);
	m_pDebugLineVertexBuffer->CleanupResource(m_pRenderer->GetRenderingDevice());
	SAFEDELETE(m_pDebugLineVertexBuffer);

	H_ASSERT(m_pDebugLineBuffer);
	m_pDebugLineBuffer->CleanupResource(m_pRenderer->GetRenderingDevice());
	SAFEDELETE(m_pDebugLineBuffer);

	H_ASSERT(m_pDebugLinePipeline);
	m_pDebugLinePipeline->CleanupResource(*m_pRenderer->GetRenderingDevice());
	SAFEDELETE(m_pDebugLinePipeline);
}

void Hail::DebugRenderingManager::Prepare(RenderCommandPool& poolOfCommands)
{
	RenderContext* pContext = m_pRenderer->GetCurrentContext();
	m_numberOfCirclesToRender = poolOfCommands.m_debugCircles.Size();

	m_debugLineData.Clear();
	m_numberOf2DDebugLines = poolOfCommands.m_debugLineCommands.Size();
	for (size_t iDebugLine = 0; iDebugLine < m_numberOf2DDebugLines; iDebugLine++)
	{
		const DebugLineCommand& debugLine = poolOfCommands.m_debugLineCommands[iDebugLine];

		DebugLineData line;
		// TODO: Make sure the positions get set without clipspace adjustments for 3D lines
		line.posAndIs2D = glm::vec4((debugLine.pos1.x - 0.5) * 2.0, (debugLine.pos1.y - 0.5) * 2.0, debugLine.pos1.z, debugLine.bIs2D ? 0.0f : 1.0f);
		line.color = debugLine.color1.GetColorVec4();
		m_debugLineData.Add(line);

		line.posAndIs2D = glm::vec4((debugLine.pos2.x - 0.5) * 2.0, (debugLine.pos2.y - 0.5) * 2.0, debugLine.pos2.z, debugLine.bIs2D ? 0.0f : 1.0f);
		line.color = debugLine.color2.GetColorVec4();
		m_debugLineData.Add(line);
	}

	pContext->StartTransferPass();
	pContext->UploadDataToBuffer(m_pDebugLineBuffer, m_debugLineData.Data(), sizeof(DebugLineData) * m_debugLineData.Size());

	pContext->UploadDataToBuffer(m_pDebugCircleBuffer, poolOfCommands.m_debugCircles.Data(), m_numberOfCirclesToRender * sizeof(DebugCircle));
	pContext->EndTransferPass();
}

void Hail::DebugRenderingManager::Render()
{
	RenderContext* pContext = m_pRenderer->GetCurrentContext();

	if (m_numberOfCirclesToRender)
	{
		pContext->BindVertexBuffer(nullptr, nullptr);
		pContext->SetBufferAtSlot(m_pDebugCircleBuffer, 0);
		pContext->BindMaterial(m_pDebugCirclePipeline->m_pPipeline);

		pContext->RenderInstances(m_numberOfCirclesToRender, 0);
	}

	const uint32_t numberOfLines = m_numberOf2DDebugLines * 2u;
	if (numberOfLines)
	{
		pContext->BindVertexBuffer(m_pDebugLineVertexBuffer, nullptr);
		pContext->SetBufferAtSlot(m_pDebugLineBuffer, 0);
		pContext->BindMaterial(m_pDebugLinePipeline->m_pPipeline);
		pContext->RenderDebugLines(numberOfLines);
	}

}
