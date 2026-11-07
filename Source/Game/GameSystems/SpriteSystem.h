#pragma once

#include "CommonComponents.h"
#include "ComponentArray.h"

namespace Hail
{
	class ApplicationCommandPool;

	struct SpriteUpdatePayload
	{
		// State is handled by separating the components instead of a check at runtime for each component
		const ComponentArray<SpriteComponent>* m_pSpriteComponents;
		ApplicationCommandPool* p_commandPoolToFill;
	};

	void UpdateSpriteComponents(SpriteUpdatePayload& spriteUpdatePayload);
}



