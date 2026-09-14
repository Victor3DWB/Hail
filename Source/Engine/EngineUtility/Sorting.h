#pragma once

#include "Types.h"

namespace Hail
{
	struct DepthTypeCounter2D;
	struct GameCommand_Sprite;
	struct GameCommand_Text;

	struct SpatialIndexLookup;

	namespace Sorting
	{
		void LinearBubbleDepthTypeCounter(DepthTypeCounter2D** pListToSort, uint32 listCapacity);
		void LinearBubbleSpriteCommand(GameCommand_Sprite** pListToSort, uint32 listCapacity);
		void LinearBubbleTextDepth(GameCommand_Text** pListToSort, uint32 listCapacity);
		void LinearBubbleParticleLookup(SpatialIndexLookup** pListToSort, uint32 listCapacity);
	}

}