#pragma once

#include "CommonComponents.h"
#include "ComponentArray.h"

namespace Hail
{
	class ApplicationCommandPool;

	struct PhysicsInfluenceComponent;

	struct PhysicsConstants
	{
		float m_gravity = 9.81f;
	};

	struct PhysicsUpdatePayload
	{
		float m_deltaTime;
		bool m_bShowDebugSymbols;
		EntityManager* m_pEntityManager; // For fetching transforms
		// State is handled by separating the components instead of a check at runtime for each component
		ComponentArray<PhysicsComponent>* m_pPhysicComponents;
		ApplicationCommandPool* p_commandPoolToFill;
	};

	class PhysicsSystem
	{
	public:
		void UpdatePhysicsWorld(PhysicsUpdatePayload& updatePayload);

		PhysicsComponent CreatePhysicsLineComponent(PhysicsLineData lineData, PhysicsCollisionProperties props);
		PhysicsComponent CreatePhysicsCircleComponent(PhysicsCircleData circleData, PhysicsCollisionProperties props);

		//TODO: Return components

	private:

		void ResolveCircleVCircleComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity);
		void ResolveCircleVLineComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity);
		void ResolveLineVLineComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity);

		void CollisionSolverLoopIteration(ComponentArray<PhysicsComponent>& physicsComponents);

		void ResolveCollisionInternal(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity, float distance, const glm::vec2& normalizedDirectionToCollision, float penetrationDistance);

		GrowingArray<PhysicsCircleData> m_circleData;
		GrowingArray<PhysicsLineData> m_lineData;
		PhysicsConstants m_constants;
	};

	//void UpdatePhysicsComponents(PhysicsComponent* pPhysicsCompsToUpdate, PhysicsInfluenceComponent* pPhysicsInfluenceCompoonents);
	// PhysicInfluenceComponent --> TransferToPhysics w-> PhysicComponents -> UpdatePhysics --> TransferToTransforms
	// MainGameplayLogic w-> PhysicInfluenceComponent --> TransformsToRenderCommands
}



