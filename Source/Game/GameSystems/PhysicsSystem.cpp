#include "Game_PCH.h"

#include "PhysicsSystem.h"
#include "CommonComponents.h"
#include "Interface/DebugLineHelpers.h"

namespace Hail
{
	glm::vec2 LocalAngularCross(float angularVelocity, const glm::vec2& vector)
	{
		return glm::vec2(-angularVelocity * vector.y, angularVelocity * vector.x);
	}

	bool LocalResolveSphereVSphereCollision(const glm::vec2& posCircleA, float radiiCircleA, const glm::vec2& posCircleB, float radiiCircleB, glm::vec2& outDirection, float& outDistance, glm::vec2& outCollisionPoint)
	{
		const glm::vec2 toVector = posCircleB - posCircleA;
		const float distanceSquared = glm::dot(toVector, toVector);
		const float radiiSum = radiiCircleA + radiiCircleB;
		const float radiiSumSquared = radiiSum * radiiSum;

		if (distanceSquared >= radiiSumSquared)
		{
			return false;
		}

		outDistance = sqrt(distanceSquared);

		if (outDistance < FLT_EPSILON)
		{
			outDirection = glm::vec2(0.0f, 1.0f);
			outDistance = 0.0f;
		}
		else
		{
			outDirection = glm::vec2(toVector.x / outDistance, toVector.y / outDistance);
		}

		outCollisionPoint = posCircleA + outDirection * (radiiCircleA - outDistance * 0.5f);

		return true;
	}

	float LocalCross(const glm::vec2& a, const glm::vec2& b) { return a.x * b.y - a.y * b.x; }

	bool LocalResolveLineVLineCollision(const glm::vec2& lineAMidPoint, float lineARotation, float lineAHalfLength, const glm::vec2& lineBMidPoint, float lineBRotation, float lineBHalfLength, glm::vec2& outCollisionPoint)
	{
		// Are the lines perpendicular?
		if (Math::Abs(lineARotation - lineBRotation) < FLT_EPSILON)
		{
			// Compare points distances
			float distanceBetweenCenters;
			glm::vec2 directionToCenter;
			glm::vec2 contactPoint;
			if (LocalResolveSphereVSphereCollision(lineAMidPoint, lineAHalfLength, lineBMidPoint, lineBHalfLength, directionToCenter, distanceBetweenCenters, contactPoint))
			{
				// Then return midpoint between points
				outCollisionPoint = lineAMidPoint + directionToCenter * distanceBetweenCenters * 0.5f;
				return true;
			}
			return false;
		}

		const glm::vec2 lineANormal = glm::vec2(sin(lineARotation), cos(lineARotation));
		const glm::vec2 lineBNormal = glm::vec2(sin(lineBRotation), cos(lineBRotation));

		const glm::vec2 lineA1 = lineAMidPoint + lineANormal * lineAHalfLength;
		const glm::vec2 lineA2 = lineAMidPoint - lineANormal * lineAHalfLength;
		const glm::vec2 lineB1 = lineBMidPoint + lineBNormal * lineBHalfLength;
		const glm::vec2 lineB2 = lineBMidPoint - lineBNormal * lineBHalfLength;

		glm::vec2 r = lineA2 - lineA1;
		glm::vec2 s = lineB2 - lineB1;
		glm::vec2 q_p = lineB1 - lineA1;

		float r_cross_s = LocalCross(r, s);
		float qp_cross_r = LocalCross(q_p, r);

		if (Math::Abs(r_cross_s) < FLT_EPSILON)
		{
			return false;
		}

		const float t = LocalCross(q_p, s) / r_cross_s;
		const float u = LocalCross(q_p, r) / r_cross_s;

		if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) 
		{
			outCollisionPoint = glm::vec2{ lineA1.x + t * r.x, lineA1.y + t * r.y };
			return true;
		}

		return false;
	}

	bool LocalResolveSphereVLineCollision(const glm::vec2& lineA, const glm::vec2& lineB, const glm::vec2& posCircle, float radiiCircle, glm::vec2& outDirection, float& outDistance, glm::vec2& lineCollisionPoint)
	{
		// Direction vector of the line segment
		const glm::vec2 d = lineB - lineA;

		const float lengthSquared = glm::dot(d, d);

		if (lengthSquared < FLT_EPSILON)
		{
			return false;
		}

		// Find closest point on the line segment
		float t = glm::dot(posCircle - lineA, d) / lengthSquared;

		// Clamp to line segment
		t = glm::clamp(t, 0.0f, 1.0f);

		// Calculate closest point
		lineCollisionPoint = lineA + t * d;

		// Calculate distance between circle center and closest point
		const glm::vec2 difference = posCircle - lineCollisionPoint;

		outDistance = glm::length(difference);

		// Check collision
		if (outDistance >= radiiCircle)
		{
			return false;
		}

		// Calculate collision normal
		if (outDistance < FLT_EPSILON)
		{
			outDirection = glm::vec2(0.0f, 1.0f);
			outDistance = 0.0f;
		}
		else
		{
			outDirection = -difference / outDistance;
		}

		return true;
	}

	void LocalApplyForceAtPoint(PhysicsComponent& component, const glm::vec2& centerOfMass, const glm::vec2& force, const glm::vec2& applicationPoint)
	{
		const glm::vec2 r = applicationPoint - centerOfMass;

		component.m_props.m_torque += LocalCross(r, force);
	}

	PhysicsCollisionPair LocalMakeOrderedPair(EntityHandle entityA, EntityHandle entityB)
	{
		H_ASSERT(entityA != entityB, "Logic error, the same entity should not be able to collide with itself");
		if (entityA < entityB)
		{
			return { entityA, entityB };
		}

		return { entityB, entityA };
	}

	void PhysicsSystem::Init(EntityManager* pEntityManager)
	{
		H_ASSERT(pEntityManager);
		m_entityIdToEventMap.PrepareAndFill(pEntityManager->GetEntityCapacity());
	}

	void PhysicsSystem::UpdatePhysicsWorld(PhysicsUpdatePayload& updatePayload)
	{
		H_ASSERT(updatePayload.m_pPhysicComponents);
		ClearFrameData();

		const glm::vec2 gravity = glm::vec2(0.0, m_constants.m_gravity);

		ComponentArray<PhysicsComponent>& physicsComponents = *updatePayload.m_pPhysicComponents;
		uint32 numberOfPhysicsComponents = physicsComponents.GetSize();

		// TODO: add forces, external forces applied from gameplay
		const glm::vec2 force = Hail::Vec2Zero;

		// Update positions
		for (size_t iCurrentComponent = 0; iCurrentComponent < numberOfPhysicsComponents; iCurrentComponent++)
		{
			PhysicsComponent& physicsComponent = *physicsComponents.GetComponentFromIndex(iCurrentComponent);
			Entity& physicsComponentEntity = physicsComponents.GetEntityFromComponentIndex(iCurrentComponent);

			const bool bSimulateEntity = (physicsComponent.m_props.m_flags & (PhysicsCollisionProperties::EFlags::Static | PhysicsCollisionProperties::EFlags::GameplayDriven)) == 0;
			if (bSimulateEntity)
			{
				glm::vec2 acceleration = gravity;
				//vel = (force / mass - drag * vel) * dt
				acceleration += force / physicsComponent.m_props.m_mass;
				acceleration -= physicsComponent.m_props.m_drag * physicsComponent.m_velocity;
				physicsComponent.m_velocity += acceleration * updatePayload.m_deltaTime;

				// Angular integration
				physicsComponent.m_props.m_angularAcceleration = physicsComponent.m_props.m_torque * physicsComponent.m_props.m_inverseMomentOfInertia;
				physicsComponent.m_props.m_angularVelocity += physicsComponent.m_props.m_angularAcceleration * updatePayload.m_deltaTime;
				physicsComponentEntity.m_transform.AddToRotationRad(physicsComponent.m_props.m_angularVelocity * updatePayload.m_deltaTime);

				physicsComponent.m_props.m_torque = 0.0f;
				physicsComponentEntity.m_transform.AddToPosition(physicsComponent.m_velocity * updatePayload.m_deltaTime);
			}
		}

		for (int iCollisionResolveLoop = 0; iCollisionResolveLoop < 2; ++iCollisionResolveLoop)
		{
			CollisionSolverLoopIteration(physicsComponents);
		}

		ResolveCollisionEvents();
		ResolveDebugSymbols(updatePayload);
	}

	PhysicsComponent PhysicsSystem::CreatePhysicsLineComponent(PhysicsLineData lineData, PhysicsCollisionProperties props)
	{
		PhysicsComponent lineComponent{};
		lineComponent.m_shapeDataIndex = m_lineData.Size();
		lineComponent.m_shape = EPhysicShapeType::Line;
		m_lineData.Add(lineData);
		lineComponent.m_props = props;
		const bool bSimulateEntity = (props.m_flags & (PhysicsCollisionProperties::EFlags::Static | PhysicsCollisionProperties::EFlags::GameplayDriven)) == 0;
		if (bSimulateEntity)
		{
			H_ASSERT(props.m_mass > 0.0f, "Mass needs to be non negative larger than 0");
			lineComponent.m_props.m_momentOfInertia = (1.0f / 12.0f) * props.m_mass * lineData.m_length * lineData.m_length;
			const bool bInertiaNotZero = lineComponent.m_props.m_momentOfInertia > FLT_EPSILON;
			lineComponent.m_props.m_inverseMomentOfInertia = bInertiaNotZero ? 1.0f / lineComponent.m_props.m_momentOfInertia : 0.0f;
		}
		return lineComponent;
	}

	PhysicsComponent PhysicsSystem::CreatePhysicsCircleComponent(PhysicsCircleData circleData, PhysicsCollisionProperties props)
	{
		PhysicsComponent circleComponent{};
		circleComponent.m_shapeDataIndex = m_circleData.Size();
		circleComponent.m_shape = EPhysicShapeType::Circle;
		m_circleData.Add(circleData);
		circleComponent.m_props = props;
		const bool bSimulateEntity = (props.m_flags & (PhysicsCollisionProperties::EFlags::Static | PhysicsCollisionProperties::EFlags::GameplayDriven)) == 0;
		if (bSimulateEntity)
		{
			H_ASSERT(props.m_mass > 0.0f, "Mass needs to be non negative larger than 0");
			circleComponent.m_props.m_momentOfInertia = 0.5f * props.m_mass * circleData.m_radius * circleData.m_radius;

			const bool bInertiaNotZero = circleComponent.m_props.m_momentOfInertia > FLT_EPSILON;
			circleComponent.m_props.m_inverseMomentOfInertia = bInertiaNotZero ? 1.0f / circleComponent.m_props.m_momentOfInertia : 0.0f;
		}
		return circleComponent;
	}

	GrowingArray<PhysicsCollisionEvent>& PhysicsSystem::GetCollisionEventsForEntity(EntityHandle handle)
	{
		m_currentQueriedEvents.RemoveAll();
		const GrowingArray<uint32>& entityCollisionEvents = m_entityIdToEventMap[handle];
		for (size_t i = 0; i < entityCollisionEvents.Size(); i++)
		{
			const uint32& eventId = entityCollisionEvents[i];
			m_currentQueriedEvents.Add(m_currentFrameCollisionEvents[eventId]);
		}
		return m_currentQueriedEvents;
	}

	const uint32 PhysicsSystem::GetCollisionEventCountForEntity(EntityHandle handle) const
	{
		return m_entityIdToEventMap[handle].Size();
	}

	void PhysicsSystem::ClearFrameData() 
	{
		m_currentCollisions.RemoveAll();
		m_currentFrameCollisionEvents.RemoveAll();
		for (uint32 i = 0; i < m_entityIdToEventMap.Size(); i++)
		{
			m_entityIdToEventMap[i].RemoveAll();
		}
		m_currentQueriedEvents.RemoveAll();
	}

	void PhysicsSystem::ResolveCircleVCircleComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity)
	{
		glm::vec2 normalizedDirection;
		float distance;
		const PhysicsCircleData& circleData = m_circleData[physicsComponent.m_shapeDataIndex];
		const PhysicsCircleData& otherCircleData = m_circleData[otherPhysicsComponent.m_shapeDataIndex];
		glm::vec2 collisionPoint;
		if (!LocalResolveSphereVSphereCollision(physicsComponentEntity.m_transform.GetPosition(), circleData.m_radius, otherPhysicsComponentEntity.m_transform.GetPosition(), otherCircleData.m_radius, normalizedDirection, distance, collisionPoint))
		{
			return;
		}
		float radiiSum = circleData.m_radius + otherCircleData.m_radius;
		float penetrationDistance = radiiSum - distance;
		ResolveCollisionInternal(physicsComponent, physicsComponentEntity, otherPhysicsComponent, otherPhysicsComponentEntity, distance, normalizedDirection, penetrationDistance, collisionPoint);
	}

	void PhysicsSystem::ResolveCircleVLineComponentCollision(PhysicsComponent& circlePhysicsComponent, Entity& circlePhysicsComponentEntity, PhysicsComponent& linePhysicsComponent, Entity& linePhysicsComponentEntity)
	{
		glm::vec2 normalizedDirection;
		glm::vec2 collisionPointOnLine;
		float distance;
		const PhysicsCircleData& circleData = m_circleData[circlePhysicsComponent.m_shapeDataIndex];
		const PhysicsLineData& lineData = m_lineData[linePhysicsComponent.m_shapeDataIndex];

		const glm::vec2 lineNormal = glm::vec2(cos(lineData.m_rotation), sin(lineData.m_rotation));
		const glm::vec2 linePointA = linePhysicsComponentEntity.m_transform.GetPosition() + lineNormal * lineData.m_length * 0.5f;
		const glm::vec2 linePointB = linePhysicsComponentEntity.m_transform.GetPosition() - lineNormal * lineData.m_length * 0.5f;

		if (!LocalResolveSphereVLineCollision(linePointA, linePointB, circlePhysicsComponentEntity.m_transform.GetPosition(), circleData.m_radius, normalizedDirection, distance, collisionPointOnLine))
		{
			return;
		}
		float penetrationDistance = circleData.m_radius - distance;
		ResolveCollisionInternal(circlePhysicsComponent, circlePhysicsComponentEntity, linePhysicsComponent, linePhysicsComponentEntity, distance, normalizedDirection, penetrationDistance, collisionPointOnLine);
	}

	void PhysicsSystem::ResolveLineVLineComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity,  PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity)
	{
		glm::vec2 collisionPoint;
		const PhysicsLineData& lineData = m_lineData[physicsComponent.m_shapeDataIndex];
		const PhysicsLineData& otherLineData = m_lineData[otherPhysicsComponent.m_shapeDataIndex];

		if (!LocalResolveLineVLineCollision(physicsComponentEntity.m_transform.GetPosition(), lineData.m_rotation, lineData.m_length, otherPhysicsComponentEntity.m_transform.GetPosition(), otherLineData.m_rotation, otherLineData.m_length, collisionPoint))
		{
			return;
		}

		glm::vec2 normalizedDirection = glm::normalize(physicsComponentEntity.m_transform.GetPosition() - otherPhysicsComponentEntity.m_transform.GetPosition());
		ResolveCollisionInternal(physicsComponent, physicsComponentEntity, otherPhysicsComponent, otherPhysicsComponentEntity, 0.f, normalizedDirection, 0.f, collisionPoint);
	}

	void PhysicsSystem::CollisionSolverLoopIteration(ComponentArray<PhysicsComponent>& physicsComponents)
	{
		uint32 numberOfPhysicsComponents = physicsComponents.GetSize();

		for (size_t i = 0; i < numberOfPhysicsComponents; ++i)
		{
			PhysicsComponent& physicsComponentA = *physicsComponents.GetComponentFromIndex(i);
			Entity& entityA = physicsComponents.GetEntityFromComponentIndex(i);

			for (size_t j = i + 1; j < numberOfPhysicsComponents; ++j)
			{
				PhysicsComponent& physicsComponentB = *physicsComponents.GetComponentFromIndex(j);
				Entity& entityB = physicsComponents.GetEntityFromComponentIndex(j);

				// If both objects are static there is nothing to simulate
				if (physicsComponentA.m_props.m_flags & PhysicsCollisionProperties::EFlags::Static && physicsComponentB.m_props.m_flags & PhysicsCollisionProperties::EFlags::Static)
				{
					continue;
				}

				switch (physicsComponentA.m_shape)
				{
				case EPhysicShapeType::Circle:

					switch (physicsComponentB.m_shape)
					{
					case EPhysicShapeType::Circle:
						ResolveCircleVCircleComponentCollision(physicsComponentA, entityA, physicsComponentB, entityB);
						break;
					case EPhysicShapeType::Line:
						ResolveCircleVLineComponentCollision(physicsComponentA, entityA, physicsComponentB, entityB);
						break;
					}
					break;

				case EPhysicShapeType::Line:
					switch (physicsComponentB.m_shape)
					{
					case EPhysicShapeType::Circle:
						// First components are circle data
						ResolveCircleVLineComponentCollision(physicsComponentB, entityB, physicsComponentA, entityA);
						break;
					case EPhysicShapeType::Line:
						ResolveLineVLineComponentCollision(physicsComponentA, entityA, physicsComponentB, entityB);
						break;
					}
					break;
				}
			}
		}
	}

	void PhysicsSystem::ResolveDebugSymbols(PhysicsUpdatePayload& updatePayload)
	{
		if (!updatePayload.m_bShowDebugSymbols)
		{
			return;
		}

		ComponentArray<PhysicsComponent>& physicsComponents = *updatePayload.m_pPhysicComponents;
		uint32 numberOfPhysicsComponents = physicsComponents.GetSize();

		ApplicationCommandPool& poolToFill = *updatePayload.p_commandPoolToFill;
		// Update positions
		for (size_t iCurrentComponent = 0; iCurrentComponent < numberOfPhysicsComponents; iCurrentComponent++)
		{
			const PhysicsComponent& physicsComponent = *physicsComponents.GetComponentFromIndex(iCurrentComponent);
			const Entity& physicsComponentEntity = physicsComponents.GetEntityFromComponentIndex(iCurrentComponent);

			bool bHasCollided = false;
			EPhysicsCollisionEventType collisionEventType;

			for (uint32 iEvent = 0; iEvent < m_currentFrameCollisionEvents.Size(); iEvent++)
			{
				const PhysicsCollisionEvent& event = m_currentFrameCollisionEvents[iEvent];
				if (event.m_collisionPair.m_entityA == physicsComponentEntity.GetId() || event.m_collisionPair.m_entityB == physicsComponentEntity.GetId())
				{
					if (bHasCollided)
					{
						collisionEventType = Math::Max(collisionEventType, event.m_type);
					}
					else
					{
						collisionEventType = event.m_type;
					}
					bHasCollided = true;
				}
			}

			// Exit, Enter, Stay, None
			glm::vec4 collisionColors[3] = { glm::vec4(1.0, 0.0, 0.0, 1.0), glm::vec4(0.0, 1.0, 0.0, 1.0), glm::vec4(0.0, 0.0, 1.0, 1.0) };
			glm::vec4 collisionShapeColor = bHasCollided ? collisionColors[(uint32)collisionEventType] : glm::vec4(1.0, 1.0, 1.0, 1.0);

			switch (physicsComponent.m_shape)
			{
			case EPhysicShapeType::Circle:
			{
				const glm::vec2 position = physicsComponentEntity.m_transform.GetPosition();
				const PhysicsCircleData& circleData = m_circleData[physicsComponent.m_shapeDataIndex];
				DrawCircle2DPixelSpace(poolToFill, position, circleData.m_radius, true, collisionShapeColor);
				const glm::vec2 velocityNormal = glm::normalize(physicsComponent.m_velocity);
				const glm::vec2 directionNormal = physicsComponentEntity.m_transform.GetForward();
				// Center to edge
				DrawLine2DPixelSpace(poolToFill, physicsComponentEntity.m_transform.GetPosition(), position + velocityNormal * circleData.m_radius, true);

				const glm::vec2 ccwTangentVector = glm::vec2(-directionNormal.y, directionNormal.x);
				const glm::vec2 circleArrowEndPoint = position + directionNormal * circleData.m_radius;
				DrawLine2DPixelSpace(poolToFill, position + ccwTangentVector * circleData.m_radius * 0.5f, circleArrowEndPoint, true, glm::vec4(1.0, 0.0, 0.0, 1.0), glm::vec4(1.0, 0.0, 0.0, 1.0));
				DrawLine2DPixelSpace(poolToFill, position - ccwTangentVector * circleData.m_radius * 0.5f, circleArrowEndPoint, true, glm::vec4(1.0, 0.0, 0.0, 1.0), glm::vec4(1.0, 0.0, 0.0, 1.0));
			}
			break;
			case EPhysicShapeType::Line:
			{
				const PhysicsLineData& lineData = m_lineData[physicsComponent.m_shapeDataIndex];
				const glm::vec2 lineNormal = glm::vec2(cos(lineData.m_rotation), sin(lineData.m_rotation));
				const glm::vec2 linePointA = physicsComponentEntity.m_transform.GetPosition() + lineNormal * lineData.m_length * 0.5f;
				const glm::vec2 linePointB = physicsComponentEntity.m_transform.GetPosition() - lineNormal * lineData.m_length * 0.5f;
				DrawLine2DPixelSpace(poolToFill, linePointA, linePointB, true, collisionShapeColor, collisionShapeColor);
			}
			break;
			}
		}
	}

	void PhysicsSystem::ResolveCollisionInternal(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity, float distance, const glm::vec2& normalizedDirectionToCollision, float penetrationDistance, const glm::vec2& collisionPoint)
	{
		PhysicsCollisionInfo collisionData;
		collisionData.m_penetrationDepth = penetrationDistance;
		collisionData.m_contactPoint = collisionPoint;
		collisionData.m_contactNormal = normalizedDirectionToCollision;
		RegisterCollision(physicsComponentEntity.GetId(), otherPhysicsComponentEntity.GetId(), collisionData);
		
		const bool bSimulateEntity = (physicsComponent.m_props.m_flags & (PhysicsCollisionProperties::EFlags::Static | PhysicsCollisionProperties::EFlags::GameplayDriven)) == 0;
		const bool bSimulateOtherEntity = (otherPhysicsComponent.m_props.m_flags & (PhysicsCollisionProperties::EFlags::Static | PhysicsCollisionProperties::EFlags::GameplayDriven)) == 0;

		// A gameplay driven component can still collide with a static object, but nothing to resolve for the system
		if (bSimulateEntity == false && bSimulateOtherEntity == false)
		{
			return;
		}

		const float invMass = physicsComponent.m_props.m_mass > 0.0f ? 1.0f / physicsComponent.m_props.m_mass : 0.0f;
		const float otherInvMass = otherPhysicsComponent.m_props.m_mass > 0.0f ? 1.0f / otherPhysicsComponent.m_props.m_mass : 0.0f;

		const float	percent = 0.80;// Penetration resolution percentage(80 %)
		const float slop = 0.005;// Penetration allowance

		const float correctionMagnitude = Math::Max(penetrationDistance - slop, 0.f) / (invMass + otherInvMass) * percent;
		const glm::vec2 correctionVector = glm::vec2(normalizedDirectionToCollision.x * correctionMagnitude, normalizedDirectionToCollision.y * correctionMagnitude);

		if (bSimulateEntity)
		{
			physicsComponentEntity.m_transform.AddToPosition(-glm::vec2(invMass * correctionVector.x, invMass * correctionVector.y));
		}
		if (bSimulateOtherEntity)
		{
			otherPhysicsComponentEntity.m_transform.AddToPosition(glm::vec2(otherInvMass * correctionVector.x, otherInvMass * correctionVector.y));
		}

		const glm::vec2 rA = collisionPoint - physicsComponentEntity.m_transform.GetPosition();
		const glm::vec2 rB = collisionPoint - otherPhysicsComponentEntity.m_transform.GetPosition();

		const glm::vec2 velocityA = physicsComponent.m_velocity + LocalAngularCross(physicsComponent.m_props.m_angularVelocity, rA);
		const glm::vec2 velocityB = otherPhysicsComponent.m_velocity + LocalAngularCross(otherPhysicsComponent.m_props.m_angularVelocity, rB);

		glm::vec2 relativeVelocity = velocityB - velocityA;

		const float velocityAlongNormal = glm::dot(relativeVelocity, normalizedDirectionToCollision);
		if (velocityAlongNormal > 0.0f)
		{
			return;
		}

		const float combinedRestitution = Math::Min(physicsComponent.m_props.m_bounciness, otherPhysicsComponent.m_props.m_bounciness);
		constexpr float restitutionThreshold = 1.0f;

		const float restitution = Math::Abs(velocityAlongNormal) < restitutionThreshold ? 0.0f : combinedRestitution;

		const float rACrossN = LocalCross(rA, normalizedDirectionToCollision);
		const float rBCrossN = LocalCross(rB, normalizedDirectionToCollision);

		float denominator = invMass + otherInvMass + physicsComponent.m_props.m_inverseMomentOfInertia * rACrossN * rACrossN + otherPhysicsComponent.m_props.m_inverseMomentOfInertia * rBCrossN * rBCrossN;

		float impulseScalar = -(1.0f + restitution) * velocityAlongNormal;

		impulseScalar /= denominator;

		glm::vec2 impulse = impulseScalar * normalizedDirectionToCollision;

		if (bSimulateEntity)
		{
			physicsComponent.m_velocity -= invMass * impulse;
			physicsComponent.m_props.m_angularVelocity -= physicsComponent.m_props.m_inverseMomentOfInertia * LocalCross(rA, impulse);
		}
		if (bSimulateOtherEntity)
		{
			otherPhysicsComponent.m_velocity += otherInvMass * impulse;
			otherPhysicsComponent.m_props.m_angularVelocity += otherPhysicsComponent.m_props.m_inverseMomentOfInertia * LocalCross(rB, impulse);
		}

		// Recalculate relative velocity after normal impulse
		const glm::vec2 velocityAfterNormalA = physicsComponent.m_velocity + LocalAngularCross(physicsComponent.m_props.m_angularVelocity, rA);
		const glm::vec2 velocityAfterNormalB = otherPhysicsComponent.m_velocity + LocalAngularCross(otherPhysicsComponent.m_props.m_angularVelocity, rB);

		relativeVelocity = velocityAfterNormalB - velocityAfterNormalA;

		// Calculate tangent direction
		glm::vec2 tangent = relativeVelocity - glm::dot(relativeVelocity, normalizedDirectionToCollision) * normalizedDirectionToCollision;

		const float tangentLengthSquared = glm::dot(tangent, tangent);

		if (tangentLengthSquared > FLT_EPSILON)
		{
			tangent /= sqrt(tangentLengthSquared);

			const float rACrossT = LocalCross(rA, tangent);
			const float rBCrossT = LocalCross(rB, tangent);

			const float frictionDenominator = invMass + otherInvMass + physicsComponent.m_props.m_inverseMomentOfInertia * rACrossT * rACrossT + otherPhysicsComponent.m_props.m_inverseMomentOfInertia * rBCrossT * rBCrossT;

			if (frictionDenominator > FLT_EPSILON)
			{
				float frictionImpulseScalar = -glm::dot(relativeVelocity, tangent) / frictionDenominator;

				// Combine friction coefficients
				const float friction = sqrt(physicsComponent.m_props.m_friction * otherPhysicsComponent.m_props.m_friction);

				// Coulomb friction limit
				const float maxFrictionImpulse = impulseScalar * friction;

				frictionImpulseScalar = glm::clamp(frictionImpulseScalar, -maxFrictionImpulse, maxFrictionImpulse);

				const glm::vec2 frictionImpulse = frictionImpulseScalar * tangent;
				if (bSimulateEntity)
				{
					// Apply linear friction impulse
					physicsComponent.m_velocity -= invMass * frictionImpulse;
					// Apply angular friction impulse
					physicsComponent.m_props.m_angularVelocity -= physicsComponent.m_props.m_inverseMomentOfInertia * LocalCross(rA, frictionImpulse);
				}
				if (bSimulateOtherEntity)
				{
					otherPhysicsComponent.m_velocity += otherInvMass * frictionImpulse;
					otherPhysicsComponent.m_props.m_angularVelocity += otherPhysicsComponent.m_props.m_inverseMomentOfInertia * LocalCross(rB, frictionImpulse);
				}
			}
		}
	}

	void PhysicsSystem::RegisterCollision(EntityHandle entityA, EntityHandle entityB, PhysicsCollisionInfo collisionInfo)
	{
		PhysicsCollisionPair pair = LocalMakeOrderedPair(entityA, entityB);

		int eventIndex = m_currentCollisions.FindCustomCondition([pair](const PhysicsCollisionEvent& collisionEvent) { return pair == collisionEvent.m_collisionPair; });

		if (eventIndex != -1)
		{
			return;
		}

		PhysicsCollisionEvent& collisionEvent = m_currentCollisions.Add();
		collisionEvent.m_collisionPair = pair;
		collisionEvent.m_collisionInfo = collisionInfo;


	}

	void PhysicsSystem::ResolveCollisionEvents()
	{
		auto addEventToMaps = [this](const PhysicsCollisionEvent& event){
				const uint32 currentFrameEventIndex = m_currentFrameCollisionEvents.Size();
				m_entityIdToEventMap[event.m_collisionPair.m_entityA].Add(currentFrameEventIndex);
				m_entityIdToEventMap[event.m_collisionPair.m_entityB].Add(currentFrameEventIndex);
				m_currentFrameCollisionEvents.Add(event);
			};

		for (int iEvent = 0; iEvent < m_currentCollisions.Size(); ++iEvent)
		{
			PhysicsCollisionEvent& current = m_currentCollisions[iEvent];
			int previousEventIndex = m_previousCollisions.FindCustomCondition([current](const PhysicsCollisionEvent& collisionEvent) { return current.m_collisionPair == collisionEvent.m_collisionPair; });
			current.m_type = previousEventIndex != -1 ? EPhysicsCollisionEventType::Stay : EPhysicsCollisionEventType::Enter;

			addEventToMaps(current);
		}

		for (int iEvent = 0; iEvent < m_previousCollisions.Size(); ++iEvent)
		{
			const PhysicsCollisionEvent& previous = m_previousCollisions[iEvent];
			int currentEventIndex = m_previousCollisions.FindCustomCondition([previous](const PhysicsCollisionEvent& collisionEvent) { return previous.m_collisionPair == collisionEvent.m_collisionPair; });
			if (currentEventIndex == -1)
			{
				PhysicsCollisionEvent exitEvent = previous;
				exitEvent.m_type = EPhysicsCollisionEventType::Exit;

				addEventToMaps(exitEvent);

			}
		}

		m_previousCollisions.Swap(m_currentCollisions);
	}
}
