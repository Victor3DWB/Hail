#include "Engine_PCH.h"
#include "ThreadSynchronizer.h"
#include "Input\InputHandler.h"
#include "glm\common.hpp"
#include "Resources\ResourceManager.h"
#include "Rendering\SwapChain.h"
#include "Input\InputActionMap.h"
#include "EngineUtility\Sorting.h"

using namespace Hail;

void Hail::ThreadSyncronizer::Init(float tickTimer)
{
	m_engineTickRate = tickTimer;
	m_currentActiveRenderPoolWrite = 0;
	m_currentActiveRenderPoolRead = 1;
	m_currentActiveRenderPoolLastRead = 2;
	m_currentActiveAppCommandPoolWrite = 0;
	m_currentActiveAppCommandPoolRead = 1;
	m_appData.commandPoolToFill = &m_appCommandPools[m_currentActiveAppCommandPoolWrite];
}

void Hail::ThreadSyncronizer::SynchronizeAppData(InputActionMap& inputActionMap, ImGuiCommandRecorder& imguiCommandRecorder, ResourceManager& resourceManager)
{
	m_currentResolution = ResolutionFromEnum(resourceManager.GetTargetResolution());
	SwapBuffersInternal();
	m_appData.rawInputData = inputActionMap.GetRawInputMap();
	m_appData.inputActionMap = &inputActionMap;
	m_appData.imguiCommandRecorder = &imguiCommandRecorder;
	m_currentRenderTimer = 0.0f;
	m_appData.commandPoolToFill->NewFrame();
	m_appData.commandPoolToFill->horizontalAspectRatio = resourceManager.GetSwapChain()->GetTargetHorizontalAspectRatio();
	m_appData.commandPoolToFill->inverseHorizontalAspectRatio = 1.0 / m_appData.commandPoolToFill->horizontalAspectRatio;
	m_appData.commandPoolToFill->camera2D.SetResolution(m_currentResolution);
}

void Hail::ThreadSyncronizer::TransferGameCommandsToRenderCommands(ResourceManager& resourceManager)
{
	ApplicationCommandPool& poolToTransferFrom = m_appCommandPools[m_currentActiveAppCommandPoolRead];
	// Filling data from the read pool
	RenderCommandPool& renderPoolReadToFill = m_renderCommandPools[m_currentActiveRenderPoolRead];
	
	// temp
	renderPoolReadToFill.playerPosition = poolToTransferFrom.playerPosition;

	renderPoolReadToFill.camera2D = poolToTransferFrom.camera2D;
	renderPoolReadToFill.camera3D = poolToTransferFrom.camera3D;
	//prepare data structures for the pool we are going to fill
	renderPoolReadToFill.m_batches.Clear();
	renderPoolReadToFill.m_layersBatchOffset.Clear();
	renderPoolReadToFill.m_2DRenderCommands.Clear();
	renderPoolReadToFill.m_spriteData.Clear();
	renderPoolReadToFill.m_textData.Clear();
	renderPoolReadToFill.m_debugLineCommands.Clear();
	renderPoolReadToFill.m_debugCircles.Clear();

	uint16 textCounter = 0u;
	uint16 spriteCounter = 0u;
	uint32 batchCounter = 0u;
	for (uint16 iLayer = 0; iLayer < poolToTransferFrom.m_depthTypeCounters.Size(); iLayer++)
	{
		const DepthTypeCounter2D& depthTypeCounter = poolToTransferFrom.m_depthTypeCounters[iLayer];

		H_ASSERT(depthTypeCounter.m_spriteCounter || depthTypeCounter.m_textCounter);
		renderPoolReadToFill.m_layersBatchOffset.Add(batchCounter);

		uint32 currentMaterialID{MAX_UINT - 1};
		uint16 numberOfInstancesInBatch = 0;
		const int32 spriteCounterBeforeBatching = spriteCounter;

		for (uint16 iSpriteC = 0; iSpriteC < depthTypeCounter.m_spriteCounter; iSpriteC++)
		{
			numberOfInstancesInBatch++;
			// Prepare all sprite data we need from application data
			const uint16 iSpriteCIndex = spriteCounterBeforeBatching + iSpriteC;
			GameCommand_Sprite& spriteCToTransfer = poolToTransferFrom.m_spriteCommands[iSpriteCIndex];

			RenderCommand2DBase& commandToAdd = renderPoolReadToFill.m_2DRenderCommands.Add();
			commandToAdd.m_dataIndex = renderPoolReadToFill.m_spriteData.Size();
			RenderData_Sprite& dataToAdd = renderPoolReadToFill.m_spriteData.Add();
			resourceManager.SpriteRenderDataFromGameCommand(spriteCToTransfer, commandToAdd, dataToAdd);

			const uint32 nextSpriteIndex = Math::Min(iSpriteCIndex + 1, (depthTypeCounter.m_spriteCounter + spriteCounterBeforeBatching) - 1);
			const uint32 nextMaterialID = poolToTransferFrom.m_spriteCommands[nextSpriteIndex].materialInstanceID;
			if (spriteCToTransfer.materialInstanceID != nextMaterialID)
			{
				currentMaterialID = poolToTransferFrom.m_spriteCommands[iSpriteCIndex].materialInstanceID;

				Batch2DInfo& spriteBatch = renderPoolReadToFill.m_batches.Add();
				spriteBatch.m_type = eCommandType::Sprite;
				spriteBatch.m_instanceOffset = spriteCounter + textCounter;
				spriteBatch.m_numberOfInstances = numberOfInstancesInBatch;
				batchCounter++;
				spriteCounter += numberOfInstancesInBatch;
				numberOfInstancesInBatch = 0;
			}
		}

		if (numberOfInstancesInBatch)
		{

			Batch2DInfo& spriteBatch = renderPoolReadToFill.m_batches.Add();
			spriteBatch.m_type = eCommandType::Sprite;
			spriteBatch.m_instanceOffset = spriteCounter + textCounter;
			spriteBatch.m_numberOfInstances = numberOfInstancesInBatch;
			batchCounter++;

			spriteCounter += numberOfInstancesInBatch;
			numberOfInstancesInBatch = 0;
		}

		if (depthTypeCounter.m_textCounter == 0u)
			continue;

		Batch2DInfo& textBatch = renderPoolReadToFill.m_batches.Add();
		textBatch.m_type = eCommandType::Text;
		textBatch.m_instanceOffset = spriteCounter + textCounter;
		textBatch.m_numberOfInstances = depthTypeCounter.m_textCounter;
		batchCounter++;
		for (uint16 iTextC = 0; iTextC < depthTypeCounter.m_textCounter; iTextC++)
		{
			const uint16 iTextCAdjusted = textCounter + iTextC;

			GameCommand_Text& textCToTransfer = poolToTransferFrom.m_textCommands[iTextCAdjusted];

			RenderCommand2DBase& commandToAdd = renderPoolReadToFill.m_2DRenderCommands.Add();
			commandToAdd.m_dataIndex = renderPoolReadToFill.m_textData.Size();
			commandToAdd.m_color = textCToTransfer.color;
			commandToAdd.m_transform = textCToTransfer.transform;
			commandToAdd.m_index_materialIndex_flags.u = textCToTransfer.index;
			// The last bit is set to 1 or 0 for if the data should be lerped or not
			commandToAdd.m_index_materialIndex_flags.u |= (textCToTransfer.bLerpCommand ? LerpCommandFlagMask : 0);

			RenderData_Text& dataToAdd = renderPoolReadToFill.m_textData.Add();
			dataToAdd.text = textCToTransfer.text;
			dataToAdd.textSize = textCToTransfer.textSize;
		}
		textCounter += depthTypeCounter.m_textCounter;
	}

	for (uint16 i = 0; i < poolToTransferFrom.m_debugLineCommands.Size(); i++)
		renderPoolReadToFill.m_debugLineCommands.Add(poolToTransferFrom.m_debugLineCommands[i]);

	for (uint16 i = 0; i < poolToTransferFrom.m_debugCircleCommands.Size(); i++)
		renderPoolReadToFill.m_debugCircles.Add(poolToTransferFrom.m_debugCircleCommands[i]);

	// Transfer to the current write render pool the data we do not need to lerp each frame
	RenderCommandPool& writeRenderPool = GetRenderPool();
	writeRenderPool.m_spriteData.Clear();
	writeRenderPool.m_spriteData.AddN_NoConstruction(renderPoolReadToFill.m_spriteData.Size());
	writeRenderPool.m_textData.Clear();
	writeRenderPool.m_textData.AddN_NoConstruction(renderPoolReadToFill.m_textData.Size());
	writeRenderPool.m_batches.Clear();
	writeRenderPool.m_batches.AddN_NoConstruction(renderPoolReadToFill.m_batches.Size());
	writeRenderPool.m_layersBatchOffset.Clear();
	writeRenderPool.m_layersBatchOffset.AddN_NoConstruction(renderPoolReadToFill.m_layersBatchOffset.Size());

	// TODO make a CopyN function
	for (uint32 i = 0; i < renderPoolReadToFill.m_spriteData.Size(); i++)
		writeRenderPool.m_spriteData[i] = renderPoolReadToFill.m_spriteData[i];

	for (uint32 i = 0; i < renderPoolReadToFill.m_textData.Size(); i++)
		writeRenderPool.m_textData[i] = renderPoolReadToFill.m_textData[i];

	for (uint32 i = 0; i < renderPoolReadToFill.m_batches.Size(); i++)
		writeRenderPool.m_batches[i] = renderPoolReadToFill.m_batches[i];

	for (uint32 i = 0; i < renderPoolReadToFill.m_layersBatchOffset.Size(); i++)
		writeRenderPool.m_layersBatchOffset[i] = renderPoolReadToFill.m_layersBatchOffset[i];

}

void Hail::ThreadSyncronizer::SynchronizeRenderData(float frameDeltaTime)
{
	m_currentRenderTimer += frameDeltaTime;

	LerpRenderBuffers();
}

void Transform2DLineFromPixelSpaceToNormalizedSpace(glm::vec3& start, glm::vec3& end, const glm::uvec2& screenResoltuion)
{
	glm::vec2 transformedPosition1 = glm::vec2(start) / glm::vec2(screenResoltuion);
	glm::vec2 transformedPosition2 = glm::vec2(end) / glm::vec2(screenResoltuion);
	transformedPosition1 += 0.5f;
	transformedPosition2 += 0.5f;
	start.x = transformedPosition1.x;
	start.y = transformedPosition1.y;
	end.x = transformedPosition2.x;
	end.y = transformedPosition2.y;
}

void Hail::ThreadSyncronizer::PrepareApplicationData()
{
	ApplicationCommandPool* pPool = m_appData.commandPoolToFill;
	Camera2D& camera = pPool->camera2D;
	camera.SetResolution(m_currentResolution);

	//Temp:
	pPool->playerPosition = glm::vec2(pPool->playerPosition.x / m_currentResolution.x + 0.5f, pPool->playerPosition.y / m_currentResolution.y + 0.5f);
	// Sorting the game data 
	DepthTypeCounter2D* depthCounterList = pPool->m_depthTypeCounters.Data();
	Sorting::LinearBubbleDepthTypeCounter(&depthCounterList, pPool->m_depthTypeCounters.Size());

	GameCommand_Sprite* spriteList = pPool->m_spriteCommands.Data();
	Sorting::LinearBubbleSpriteCommand(&spriteList, pPool->m_spriteCommands.Size());

	GameCommand_Text* textList = pPool->m_textCommands.Data();
	if (!pPool->m_textCommands.Empty())
		Sorting::LinearBubbleTextDepth(&textList, pPool->m_textCommands.Size());

	for (size_t i = 0; i < pPool->m_spriteCommands.Size(); i++)
	{
		GameCommand_Sprite& sprite = pPool->m_spriteCommands[i];
		if (sprite.bIsAffectedBy2DCamera)
		{
			camera.TransformToCameraSpace(sprite.transform);
		}
	}
	for (size_t i = 0; i < pPool->m_textCommands.Size(); i++)
	{
		GameCommand_Text& textC = pPool->m_textCommands[i];
		if (!textC.bNormalizedPosition)
			camera.TransformToCameraSpace(textC.transform);
	}
	for (size_t i = 0; i < pPool->m_debugLineCommands.Size(); i++)
	{
		DebugLineCommand& line = pPool->m_debugLineCommands[i];
		if (line.bIs2D)
		{
			if (line.bIsAffectedBy2DCamera)
			{
				if (line.bIsNormalized)
					camera.TransformLineToCameraSpaceFromNormalizedSpace(line.pos1, line.pos2);
				else
					camera.TransformLineToCameraSpaceFromPixelSpace(line.pos1, line.pos2);
			}
			else if (!line.bIsNormalized)
				Transform2DLineFromPixelSpaceToNormalizedSpace(line.pos1, line.pos2, camera.GetResolution());
		}
	}
}

void Hail::ThreadSyncronizer::SwapBuffersInternal()
{
	const uint32_t lastRead = m_currentActiveRenderPoolLastRead;
	m_currentActiveRenderPoolLastRead = m_currentActiveRenderPoolRead;
	m_currentActiveRenderPoolRead = m_currentActiveRenderPoolWrite;
	m_currentActiveRenderPoolWrite = lastRead;

	m_currentActiveAppCommandPoolRead = m_currentActiveAppCommandPoolWrite;
	m_currentActiveAppCommandPoolWrite = (m_currentActiveAppCommandPoolWrite + 1) % 2;

	m_appData.commandPoolToFill = &m_appCommandPools[m_currentActiveAppCommandPoolWrite];
}

void Hail::ThreadSyncronizer::LerpRenderBuffers()
{
	float tValue = m_currentRenderTimer / m_engineTickRate;
	const RenderCommandPool& readPool = m_renderCommandPools[m_currentActiveRenderPoolRead];
	const RenderCommandPool& lastReadPool = m_renderCommandPools[m_currentActiveRenderPoolLastRead];
	RenderCommandPool& writePool = GetRenderPool();

	writePool.camera3D = Camera::LerpCamera(readPool.camera3D, lastReadPool.camera3D, tValue);
	writePool.camera2D.SetResolution(readPool.camera2D.GetResolution());
	writePool.camera2D.SetZoom(Math::Lerp(readPool.camera2D.GetZoom(), lastReadPool.camera2D.GetZoom(), tValue));
	writePool.camera2D.SetPosition(glm::mix(readPool.camera2D.GetPosition(), lastReadPool.camera2D.GetPosition(), tValue));

	writePool.m_2DRenderCommands.Clear();
	writePool.m_2DRenderCommands.AddN_NoConstruction(readPool.m_2DRenderCommands.Size());
	writePool.m_debugLineCommands.Clear();
	writePool.m_debugLineCommands.AddN_NoConstruction(readPool.m_debugLineCommands.Size());
	writePool.m_debugCircles.Clear();
	writePool.m_debugCircles.AddN_NoConstruction(readPool.m_debugCircles.Size());

	for (uint32 i2DInstanceID = 0; i2DInstanceID < readPool.m_2DRenderCommands.Size(); i2DInstanceID++)
	{
		const RenderCommand2DBase& readCommand = readPool.m_2DRenderCommands[i2DInstanceID];

		if ((readCommand.m_index_materialIndex_flags.u & LerpCommandFlagMask) == 0)
		{
			writePool.m_2DRenderCommands[i2DInstanceID] = readCommand;
			H_ASSERT(writePool.m_2DRenderCommands[i2DInstanceID].m_dataIndex != MAX_UINT);
			continue;
		}
		
		RenderCommand2DBase& writeCommand = writePool.m_2DRenderCommands[i2DInstanceID];
		//search for correct index, might be performant dumb dumb, but lets improve this when needed
		bool foundCommand = false;
		if (i2DInstanceID < lastReadPool.m_2DRenderCommands.Size())
		{
			if (readCommand.m_index_materialIndex_flags.u == lastReadPool.m_2DRenderCommands[i2DInstanceID].m_index_materialIndex_flags.u)
			{
				foundCommand = true;
				LerpRenderCommand2DBase(writeCommand, readCommand, lastReadPool.m_2DRenderCommands[i2DInstanceID], tValue);
			}
			else
			{
				uint16 iMissingCommand = Math::Min(uint16(i2DInstanceID + 1u), uint16(lastReadPool.m_2DRenderCommands.Size() - 1u));
				for (; iMissingCommand < lastReadPool.m_2DRenderCommands.Size(); ++iMissingCommand)
				{
					if (lastReadPool.m_2DRenderCommands[iMissingCommand].m_index_materialIndex_flags.u > readCommand.m_index_materialIndex_flags.u)
						break;

					if (readCommand.m_index_materialIndex_flags.u == lastReadPool.m_2DRenderCommands[iMissingCommand].m_index_materialIndex_flags.u)
					{
						foundCommand = true;
						LerpRenderCommand2DBase(writeCommand, readCommand, lastReadPool.m_2DRenderCommands[iMissingCommand], tValue);
						break;
					}
				}
			}
		}

		if (!foundCommand)
			writeCommand = readCommand;
	}

	Lerp3DModels(tValue);
	LerpDebugLines(tValue);
	LerpDebugCircles(tValue);
}

namespace 
{
	void LerpMeshCommand(const RenderData_Mesh& readMesh, const RenderData_Mesh& lastReadMesh, RenderData_Mesh& writeMesh, float t)
	{
		writeMesh.transform = Transform3D::LerpTransforms_t(readMesh.transform, lastReadMesh.transform, t);
		writeMesh.color = Color::Lerp(readMesh.color, lastReadMesh.color, t);
		writeMesh.meshID = readMesh.meshID;
		writeMesh.index = readMesh.index;
		writeMesh.materialInstanceID = readMesh.materialInstanceID;
		writeMesh.bLerpCommand = readMesh.bLerpCommand;
	}

	void LerpDebugLine(const DebugLineCommand& readLine, const DebugLineCommand& lastReadLine, DebugLineCommand& writeLine, float t)
	{
		writeLine.color1 = Color::Lerp(readLine.color1, lastReadLine.color1, t);
		writeLine.color2 = Color::Lerp(readLine.color2, lastReadLine.color2, t);
		writeLine.pos1 = glm::mix(readLine.pos1, lastReadLine.pos1, t);
		writeLine.pos2 = glm::mix(readLine.pos2, lastReadLine.pos2, t);
		writeLine.bIs2D = readLine.bIs2D;
		writeLine.bLerpCommand = readLine.bLerpCommand;
		writeLine.bIsAffectedBy2DCamera = readLine.bIsAffectedBy2DCamera;
	}
}


void Hail::ThreadSyncronizer::Lerp3DModels(float tValue)
{
	const RenderCommandPool& readPool = m_renderCommandPools[m_currentActiveRenderPoolRead];
	const RenderCommandPool& lastReadPool = m_renderCommandPools[m_currentActiveRenderPoolLastRead];
	//Lerp sprites
	const uint32_t numberOfModels = readPool.m_meshData.Size();
	const uint32_t lastReadNumberOfModels = lastReadPool.m_meshData.Size();
	for (uint16_t mesh = 0; mesh < numberOfModels; mesh++)
	{
		const RenderData_Mesh& readMesh = readPool.m_meshData[mesh];
		RenderData_Mesh& writeMesh = GetRenderPool().m_meshData[mesh];

		if (mesh >= lastReadNumberOfModels)
		{
			writeMesh = readMesh;
			continue;
		}

		const RenderData_Mesh& lastReadMesh = lastReadPool.m_meshData[mesh];
		if (readMesh.bLerpCommand)
		{
			if (readMesh.index == lastReadMesh.index)
			{
				LerpMeshCommand(readMesh, lastReadMesh, writeMesh, tValue);
			}
			else
			{
				//search for correct index
				//Switch to use an ordered heap or something like that later to increase performance.
				bool foundMesh = false;
				for (uint16_t missingMesh = mesh; missingMesh < lastReadNumberOfModels; missingMesh++)
				{
					if (readMesh.index == lastReadPool.m_meshData[missingMesh].index)
					{
						foundMesh = true;
						LerpMeshCommand(readMesh, lastReadPool.m_meshData[missingMesh], writeMesh, tValue);
						break;
					}
				}
				if (foundMesh == false)
				{
					writeMesh = readMesh;
				}
			}
		}
		else
		{
			writeMesh = readMesh;
		}
	}
}

void Hail::ThreadSyncronizer::LerpDebugLines(float tValue)
{
	const RenderCommandPool& readPool = m_renderCommandPools[m_currentActiveRenderPoolRead];
	const RenderCommandPool& lastReadPool = m_renderCommandPools[m_currentActiveRenderPoolLastRead];
	//TODO: Add proper support for 3D lines and sort these lists
	const uint32_t numberOfLines = readPool.m_debugLineCommands.Size();
	const uint32_t lastReadNumberOfLines = lastReadPool.m_debugLineCommands.Size();
	for (uint16_t iLine = 0; iLine < numberOfLines; iLine++)
	{
		const DebugLineCommand& readLine = readPool.m_debugLineCommands[iLine];
		DebugLineCommand& writeLine = GetRenderPool().m_debugLineCommands[iLine];

		if (iLine >= lastReadNumberOfLines)
		{
			writeLine = readLine;
			continue;
		}

		const DebugLineCommand& lastReadLine = lastReadPool.m_debugLineCommands[iLine];
		if (lastReadLine.bLerpCommand)
		{
			LerpDebugLine(readLine, lastReadLine, writeLine, tValue);
		}
		else
		{
			writeLine = readLine;
		}
	}
}

void Hail::ThreadSyncronizer::LerpDebugCircles(float tValue)
{
	const RenderCommandPool& readPool = m_renderCommandPools[m_currentActiveRenderPoolRead];
	const RenderCommandPool& lastReadPool = m_renderCommandPools[m_currentActiveRenderPoolLastRead];
	const uint32_t numberOfCircles = readPool.m_debugCircles.Size();
	const uint32_t lastReadNumberOfCircles = lastReadPool.m_debugCircles.Size();
	for (uint16_t iLine = 0; iLine < numberOfCircles; iLine++)
	{
		const DebugCircle& readCircle = readPool.m_debugCircles[iLine];
		DebugCircle& writeCircle = GetRenderPool().m_debugCircles[iLine];

		if (iLine >= lastReadNumberOfCircles)
		{
			writeCircle = readCircle;
			continue;
		}

		const DebugCircle& lastReadCircle = lastReadPool.m_debugCircles[iLine];
		writeCircle.pos = glm::mix(readCircle.pos, lastReadCircle.pos, tValue);
		writeCircle.scale = Math::Lerp(readCircle.scale, lastReadCircle.scale, tValue);
		writeCircle.color = Color::Lerp(readCircle.color, lastReadCircle.color, tValue);
	}
}
