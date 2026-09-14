#pragma once

#include "CommandsCommon.h"

namespace Hail
{
	struct GameCommand_Sprite
	{
		Transform2D transform;
		Color color = Color({ 1.0f, 1.0f, 1.0f, 1.0f });
		uint32 index;
		glm::vec2 pivot = { 0.5f, 0.5f };
		glm::vec4 uvTR_BL = { 0.0, 0.0, 1.0, 1.0 };
		uint32_t materialInstanceID = MAX_UINT;
		bool bSizeRelativeToRenderTarget = true;
		int m_layer{ 0 };
		bool bLerpCommand = true;
		// true the scale will be in normalized size, so a size of 0.1 will always stay 0.1 of the screen percentage
		// false the scale will be of the texture size on the render target, so the texture will be scaled by the scale, 
		// false best used with UI or elements that should not scale with screen resolution
		bool bIsAffectedBy2DCamera = true;
	};

	struct GameCommand_Text
	{
		Transform2D transform; // 20 Bytes
		Color color; // 12 Bytes
		uint32 index;
		StringLW text; // TODO: kolla hur jag sparar strängen, kanske ha en stor sträng pool bara för text commands, som är en circular buffer
		uint16 textSize;
		int m_layer{ 0 };
		bool bLerpCommand{ true };
		bool bNormalizedPosition {false};
	};

	struct GameCommand_Mesh
	{
		Transform3D transform;
		Color color;
		uint32_t meshID = 0;
		uint32_t materialInstanceID = 0;
		uint16_t index = 0;
		bool bLerpCommand = true;
	};

	class ApplicationCommandPool
	{
	public:
		float horizontalAspectRatio{}; // if this is 16 / 9 = 1.77
		float inverseHorizontalAspectRatio{};
		Camera camera3D;
		Camera2D camera2D;


		void AddDebugLine(const DebugLineCommand& debugLineToAdd);
		void AddSpriteCommand(const GameCommand_Sprite& spriteToAdd);
		void AddTextCommand(const GameCommand_Text& textToAdd);
		// Debug circles should be normalized, or add conversion to camera space and the like
		void AddDebugCircle(DebugCircle circleToAdd);
		void NewFrame();

		VectorOnStack<GameCommand_Mesh, 1024, false> m_meshCommands;
		// temp code:
		glm::vec2 playerPosition = glm::vec2(0.f);
	private:
		friend class ThreadSyncronizer;
		// TODO gör detta till ett hashset som drivs av depth index
		VectorOnStack<DepthTypeCounter2D, MAX_NUMBER_OF_2D_RENDER_COMMANDS> m_depthTypeCounters; 
		VectorOnStack<GameCommand_Sprite, MAX_NUMBER_OF_SPRITES, false> m_spriteCommands;
		VectorOnStack<GameCommand_Text, MAX_NUMBER_OF_TEXT_COMMANDS, false> m_textCommands;
		VectorOnStack<DebugLineCommand, MAX_NUMBER_OF_DEBUG_LINES / 2, false> m_debugLineCommands;
		VectorOnStack<DebugCircle, MAX_NUMBER_OF_DEBUG_CIRCLES, false> m_debugCircleCommands;
	};
}