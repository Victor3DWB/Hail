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

	struct PhysicsCollisionPair
	{
		EntityHandle m_entityA;
		EntityHandle m_entityB;

		bool operator==(const PhysicsCollisionPair& other) const
		{
			return m_entityA == other.m_entityA && m_entityB == other.m_entityB;
		}
	};

	struct PhysicsCollisionInfo
	{
		glm::vec2 m_contactPoint;
		glm::vec2 m_contactNormal;

		float m_penetrationDepth;
	};

	enum class EPhysicsCollisionEventType
	{
		Exit,
		Enter,
		Stay,
		Count
	};

	constexpr const char* pECollisionEventTypeNames[(uint32)EPhysicsCollisionEventType::Count] =
	{
		"Exit",
		"Enter",
		"Stay"
	};

	struct PhysicsCollisionEvent
	{
		PhysicsCollisionPair m_collisionPair;
		PhysicsCollisionInfo m_collisionInfo;
		EPhysicsCollisionEventType m_type;
	};

	struct PhysicsUpdatePayload
	{
		float m_deltaTime;
		bool m_bShowDebugSymbols;
		// State is handled by separating the components instead of a check at runtime for each component
		ComponentArray<PhysicsComponent>* m_pPhysicComponents;
		ApplicationCommandPool* p_commandPoolToFill;
	};

	class PhysicsSystem
	{
	public:
		void Init(EntityManager* pEntityManager);

		void ApplyForce(EntityHandle entity, const glm::vec2& force);
		void ApplyImpulse(EntityHandle entity, const glm::vec2& impulse);
		void ApplyTorque(EntityHandle entity, float torque);
		void ApplyForceAtPoint(EntityHandle entity, const glm::vec2& force, const glm::vec2& worldPoint);

		void UpdatePhysicsWorld(PhysicsUpdatePayload& updatePayload);

		PhysicsComponent CreatePhysicsLineComponent(PhysicsLineData lineData, PhysicsCollisionProperties props);
		PhysicsComponent CreatePhysicsCircleComponent(PhysicsCircleData circleData, PhysicsCollisionProperties props);

		// Repopulates the m_currentQueriedEvents list so that no allocations occur. 
		// Not a thread safe opration, never store a pointer to this array.
		GrowingArray<PhysicsCollisionEvent>& GetCollisionEventsForEntity(EntityHandle handle);
		const uint32 GetCollisionEventCountForEntity(EntityHandle handle) const;

	private:
		void ClearFrameData();

		void ResolveCircleVCircleComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity);
		void ResolveCircleVLineComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity);
		void ResolveLineVLineComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity);

		void CollisionSolverLoopIteration(ComponentArray<PhysicsComponent>& physicsComponents);
		void ResolveDebugSymbols(PhysicsUpdatePayload& updatePayload);

		void ResolveCollisionInternal(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity, float distance, const glm::vec2& normalizedDirectionToCollision, float penetrationDistance, const glm::vec2& collisionPoint);

		void RegisterCollision(EntityHandle entityA, EntityHandle entityB, PhysicsCollisionInfo collisionInfo);
		void ResolveCollisionEvents();

		GrowingArray<PhysicsCollisionEvent> m_currentCollisions;
		GrowingArray<PhysicsCollisionEvent> m_previousCollisions;

		GrowingArray<GrowingArray<uint32>> m_entityIdToEventMap;
		GrowingArray<PhysicsCollisionEvent> m_currentFrameCollisionEvents;

		GrowingArray<PhysicsCollisionEvent> m_currentQueriedEvents;

		GrowingArray<PhysicsCircleData> m_circleData;
		GrowingArray<PhysicsLineData> m_lineData;
		PhysicsConstants m_constants;
	};

}
