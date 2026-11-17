#pragma once

#include "Containers\GrowingArray\GrowingArray.h"
#include "Containers\VectorOnStack\VectorOnStack.h"
#include "Transforms.h"
#include "Utility\Color.h"

class asITypeInfo;
class asIScriptObject;

namespace Hail
{
	class Capability;
	// TODO: Move physics related info to its own header, leave the user facing component in this file
	enum class EPhysicShapeType
	{
		Circle,
		Line,
		Count
	};

	constexpr const char* pEPhysicShapeTypeNames[(uint32)EPhysicShapeType::Count] =
	{
		"Circle",
		"Line"
	};

	struct PhysicsLineData
	{
		float m_rotation = 0.0;
		float m_length = 1.0;
	};

	struct PhysicsCircleData
	{
		float m_radius = 1.0;
	};

	struct PhysicsCollisionProperties
	{
		float m_mass = 1.0;
		float m_drag = 0.001;
		float m_friction = 0.5;
		float m_bounciness = 0.20; // 0 equals no bounce and 1.0 == perfect reflection

		float m_angularVelocity = 0.0f;
		float m_angularAcceleration = 0.0f;
		float m_momentOfInertia = 0.0f;
		float m_inverseMomentOfInertia = 0.0f;

		float m_torque = 0.0f;

		enum EFlags
		{
			Static = 1 << 0,
			GameplayDriven = 1 << 1, // When the gameplay drives the movement and the physics is for collision detection
		};

		uint32 m_flags = 0; // 
		//uint32 m_tags = 0; // What am I in a gameplay sense;
	};

	struct PhysicsComponent
	{
		bool IsValid() { return m_shapeDataIndex != MAX_UINT; }

		uint32 m_shapeDataIndex = MAX_UINT;
		EPhysicShapeType m_shape = EPhysicShapeType::Circle;
		glm::vec2 m_velocity = glm::vec2(0.0);
		PhysicsCollisionProperties m_props;
	};

	struct AngelscriptCapabilityComponent
	{
		struct AngelscriptCapabilityData
		{
			String64 m_capabilityName;
			uint32 m_capabilityId;
			// The number this capability was created under, used for hot-reloading
			uint32 m_loadVersion;
		};
		VectorOnStack<AngelscriptCapabilityData, 8> m_capabilities;
	};

	// One entry for each created Component defined in Angelscript that an Entity registers
	struct AngelscriptComponent
	{
		asIScriptObject* m_pObject;
		uint32 m_typeId;
	};

	struct SpriteComponent
	{
		glm::vec4 m_uvTR_BL = { 0.0, 0.0, 1.0, 1.0 };
		glm::vec2 m_pivot = { 0.5f, 0.5f };
		Color m_color = Color({ 1.0f, 1.0f, 1.0f, 1.0f });
		// Sprite to render with this command
		uint32_t m_materialInstanceID = MAX_UINT;
		int16 m_layer{ 0 };
		bool m_bSizeRelativeToRenderTarget = true;
		// true the scale will be in normalized size, so a size of 0.1 will always stay 0.1 of the screen percentage
		// false the scale will be of the texture size on the render target, so the texture will be scaled by the scale, 
		// false best used with UI or elements that should not scale with screen resolution
		bool m_bIsAffectedBy2DCamera = true;
	};
}



