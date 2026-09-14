#pragma once

#include "Containers\GrowingArray\GrowingArray.h"
#include "Containers\VectorOnStack\VectorOnStack.h"
#include "Transforms.h"

namespace Hail
{
	class Capability;
	// TODO: Move physics related info to its own header, leave the user facing component in this file
	enum class EPhysicShapeType
	{
		Circle,
		Line
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
		float m_mass = 1.0; // A mass < 0.f is a static object
		float m_drag = 0.01;
		float m_friction = 1.0;
		float m_bounciness = 0.20; // 0 equals no bounce and 1.0 == perfect reflection

		// TODO: 
		//uint32 m_flags = 0; // To get if the component has collided this frame and with what
		//uint32 m_tags = 0; // What am I in a gameplaySense;
	};

	struct PhysicsComponent
	{
		bool IsValid() { return m_shapeDataIndex != MAX_UINT; }

		uint32 m_shapeDataIndex = MAX_UINT;
		EPhysicShapeType m_shape = EPhysicShapeType::Circle;
		glm::vec2 m_velocity = glm::vec2(0.0);
		PhysicsCollisionProperties m_props;
	};

	struct PhysicsInfluenceComponent
	{
		glm::vec2 m_force = glm::vec2(0.0);
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
}



