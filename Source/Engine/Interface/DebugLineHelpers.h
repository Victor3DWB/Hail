#pragma once
#include "Types.h"
#include "Containers\VectorOnStack\VectorOnStack.h"
#include "GameCommands.h"

namespace Hail
{
	class ApplicationCommandPool;
	//Normalized space
	void DrawLine2D(VectorOnStack<DebugLineCommand, MAX_NUMBER_OF_DEBUG_LINES / 2, false>& poolToFill, glm::vec2 startPos, glm::vec2 endPos, Color color1 = Color::White, Color color2 = Color::White);
	void DrawLine2D(ApplicationCommandPool& poolToFill, glm::vec2 startPos, glm::vec2 endPos, bool bAffectedBy2DCamera, glm::vec4 color1 = Vec4One, glm::vec4 color2 = Vec4One, bool drawPixelSpace = false);
	void DrawLine2DPixelSpace(ApplicationCommandPool& poolToFill, glm::vec2 startPos, glm::vec2 endPos, bool bAffectedBy2DCamera, glm::vec4 color1 = Vec4One, glm::vec4 color2 = Vec4One);
	//Normalized space, compensates for the render targets aspect ratio
	void DrawLine2D(VectorOnStack<DebugLineCommand, MAX_NUMBER_OF_DEBUG_LINES / 2, false>& poolToFill, float inverseHorizontalAspectRatio, glm::vec2 startPos, float rotationRadian, float length, Color color1 = Color::White, Color color2 = Color::White);
	void DrawLine2D(ApplicationCommandPool& poolToFill, glm::vec2 startPos, float rotationRadian, float length, bool bAffectedBy2DCamera, glm::vec4 color1 = Vec4One, glm::vec4 color2 = Vec4One, bool drawPixelSpace = false);
	void DrawLine2DPixelSpace(ApplicationCommandPool& poolToFill, glm::vec2 startPos, float rotationRadian, float length, bool bAffectedBy2DCamera, glm::vec4 color1 = Vec4One, glm::vec4 color2 = Vec4One);
	//Normalized space, The Box functions compensates for the render targets aspect ratio
	void DrawBox2D(VectorOnStack<DebugLineCommand, MAX_NUMBER_OF_DEBUG_LINES / 2, false>& poolToFill, float inverseHorizontalAspectRatio, glm::vec2 pos, glm::vec2 dimensions, Color color = Color::White);
	void DrawBox2D(ApplicationCommandPool& poolToFill, glm::vec2 pos, glm::vec2 dimensions, bool bAffectedBy2DCamera, glm::vec4 color = Vec4One, bool drawPixelSpace = false);
	void DrawBox2DPixelSpace(ApplicationCommandPool& poolToFill, glm::vec2 pos, glm::vec2 dimensions, bool bAffectedBy2DCamera, glm::vec4 color = Vec4One);
	void DrawRect2D(VectorOnStack<DebugLineCommand, MAX_NUMBER_OF_DEBUG_LINES / 2, false>& poolToFill, glm::vec2 min, glm::vec2 max, Color color = Color::White);
	void DrawRect2D(ApplicationCommandPool& poolToFill, glm::vec2 min, glm::vec2 max, bool bAffectedBy2DCamera, glm::vec4 color = Vec4One, bool drawPixelSpace = false);
	void DrawRect2DPixelSpace(ApplicationCommandPool& poolToFill, glm::vec2 min, glm::vec2 max, bool bAffectedBy2DCamera, glm::vec4 color = Vec4One);
	//Normalized space, The Circle function compensates for the render targets aspect ratio
	void DrawCircle2D(VectorOnStack<DebugLineCommand, MAX_NUMBER_OF_DEBUG_LINES / 2, false>& poolToFill, float inverseHorizontalAspectRatio, glm::vec2 pos, float radius, Color color = Color::White);
	void DrawCircle2D(ApplicationCommandPool& poolToFill, glm::vec2 pos, float radius, bool bAffectedBy2DCamera, glm::vec4 color = Vec4One, bool drawPixelSpace = false);
	void DrawCircle2DPixelSpace(ApplicationCommandPool& poolToFill, glm::vec2 pos, float radius, bool bAffectedBy2DCamera, glm::vec4 color = Vec4One);
}

