#pragma once

#include "Utility\Color.h"
#include "glm\vec4.hpp"

#include "glm\mat4x4.hpp"

#include "String.hpp"

#include "Containers\GrowingArray\GrowingArray.h"
#include "Containers\VectorOnStack\VectorOnStack.h"
#include "Transforms.h"
#include "Camera.h"
#include "EngineConstants.h"

namespace Hail
{
	struct DebugLineCommand
	{
		glm::vec3 pos1;
		glm::vec3 pos2;
		Color color1;
		Color color2;
		bool bLerpCommand;
		bool bIs2D;
		bool bIsAffectedBy2DCamera;
		bool bIsNormalized;
	};

	struct DebugCircle
	{
		glm::vec2 pos; // Normalized position used in rendering
		float scale; // Pixel Size scale
		Color color;
	};

	struct DepthTypeCounter2D
	{
		int m_layer{};
		uint16 m_textCounter{};
		uint16 m_spriteCounter{};
	};
}

