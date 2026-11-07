#include "Game_PCH.h"

#include "SpriteSystem.h"
#include "CommonComponents.h"
#include "Interface/DebugLineHelpers.h"

namespace Hail
{

	void UpdateSpriteComponents(SpriteUpdatePayload& spriteUpdatePayload)
	{
		H_ASSERT(spriteUpdatePayload.m_pSpriteComponents);

		const ComponentArray<SpriteComponent>& spriteComponents = *spriteUpdatePayload.m_pSpriteComponents;
		uint32 numberOfSpriteComponents = spriteComponents.GetSize();

		for (size_t iCurrentComponent = 0; iCurrentComponent < numberOfSpriteComponents; iCurrentComponent++)
		{
			const SpriteComponent& spriteComponent = *spriteComponents.GetComponentFromIndex(iCurrentComponent);
			const Entity& entity = spriteComponents.GetEntityFromComponentIndex(iCurrentComponent);

			GameCommand_Sprite spriteCommand;
			spriteCommand.transform = entity.m_transform;
			spriteCommand.index = entity.GetId();
			spriteCommand.uvTR_BL = spriteComponent.m_uvTR_BL;
			spriteCommand.pivot = spriteComponent.m_pivot;
			spriteCommand.color = spriteComponent.m_color;
			spriteCommand.materialInstanceID = spriteComponent.m_materialInstanceID;
			spriteCommand.m_layer = spriteComponent.m_layer;
			spriteCommand.bSizeRelativeToRenderTarget = spriteComponent.m_bSizeRelativeToRenderTarget;
			spriteCommand.bIsAffectedBy2DCamera = spriteComponent.m_bIsAffectedBy2DCamera;

			spriteUpdatePayload.p_commandPoolToFill->AddSpriteCommand(spriteCommand);
		}

	}


}
