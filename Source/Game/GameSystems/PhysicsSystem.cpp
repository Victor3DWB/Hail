#include "Game_PCH.h"

#include "PhysicsSystem.h"
#include "CommonComponents.h"
#include "Interface/DebugLineHelpers.h"

namespace Hail
{
	bool ResolveSphereVSphereCollision(const glm::vec2& posCircleA, float radiiCircleA, const glm::vec2& posCircleB, float radiiCircleB, glm::vec2& outDirection, float& outDistance)
	{
		const glm::vec2 toVector = posCircleB - posCircleA;
		const float distanceSquared = glm::dot(toVector, toVector);
		const float radiiSumSquared = radiiCircleA * radiiCircleA + radiiCircleB * radiiCircleB;

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

		return true;
	}

	float Cross(const glm::vec2& a, const glm::vec2& b) { return a.x * b.y - a.y * b.x; }

	bool ResolveLineVLineCollision(const glm::vec2& lineAMidPoint, float lineARotation, float lineAHalfLength, const glm::vec2& lineBMidPoint, float lineBRotation, float lineBHalfLength, glm::vec2& outCollisionPoint)
	{
		// Are the lines perpendicular?
		if (Math::Abs(lineARotation - lineBRotation) < FLT_EPSILON)
		{
			// Compare points distances
			float distanceBetweenCenters;
			glm::vec2 directionToCenter;
			if (ResolveSphereVSphereCollision(lineAMidPoint, lineAHalfLength, lineBMidPoint, lineBHalfLength, directionToCenter, distanceBetweenCenters))
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

		float r_cross_s = Cross(r, s);
		float qp_cross_r = Cross(q_p, r);

		const float t = Cross(q_p, s) / r_cross_s;
		const float u = Cross(q_p, r) / r_cross_s;

		if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) 
		{
			outCollisionPoint = glm::vec2{ lineA1.x + t * r.x, lineA1.y + t * r.y };
			return true;
		}

		return false;
	}

	bool ResolveSphereVLineCollision(const glm::vec2& lineA, const glm::vec2& lineB, const glm::vec2& posCircle, float radiiCircle, glm::vec2& outDirection, float& outDistance, glm::vec2& lineCollisionPoint)
	{
		// Direction vector of the line (d)
		glm::vec2 d = lineB - lineA;
		// Vector from circle center to line origin (f)
		glm::vec2 f = lineA - posCircle;

		// Compute quadratic coefficients: a*t^2 + b*t + c = 0
		float a = glm::dot(d, d);
		float b = 2.0 * glm::dot(f, d);
		float c = glm::dot(f, f) - (radiiCircle * radiiCircle);

		// Calculate discriminant
		float discriminant = (b * b) - (4.0 * a * c);

		if (discriminant < -FLT_EPSILON)
		{
			return false;
		}
		else if (Math::Abs(discriminant) <= FLT_EPSILON)
		{
			// Condition 2: Tangent line (1 point)
			float t = -b / (2.0 * a);
			const glm::vec2 collisionPoint = lineA + (d * t);
			outDirection = glm::normalize(collisionPoint); // Interpolate point
			outDistance = glm::distance(posCircle, collisionPoint);
			lineCollisionPoint = collisionPoint;
			return true;
		}
		else 
		{
			float t_mid = -b / (2.0 * a);

			const glm::vec2 midpoint = lineA + (d * t_mid);
			outDistance = glm::distance(midpoint, posCircle);

			const glm::vec2 centerToMid = midpoint - posCircle;
			outDirection = centerToMid / outDistance;
			lineCollisionPoint = midpoint;
			return true;
		}
	}

	//void UpdatePhysicsComponents(PhysicsComponent* pPhysicsCompsToUpdate, PhysicsInfluenceComponent* pPhysicsInfluenceCompoonents)
	//{
		// for each pPhysicsInfluenceCompoonents, get PhysicsComponent, and update
	//}

	void PhysicsSystem::UpdatePhysicsWorld(PhysicsUpdatePayload& updatePayload)
	{
		H_ASSERT(updatePayload.m_pPhysicComponents);

		const glm::vec2 gravity = glm::vec2(0.0, m_constants.m_gravity);

		ComponentArray<PhysicsComponent>& physicsComponents = *updatePayload.m_pPhysicComponents;
		uint32 numberOfPhysicsComponents = physicsComponents.GetSize();

		const glm::vec2 force = Hail::Vec2Zero;

		// Update positions
		for (size_t iCurrentComponent = 0; iCurrentComponent < numberOfPhysicsComponents; iCurrentComponent++)
		{
			PhysicsComponent& physicsComponent = *physicsComponents.GetComponentFromIndex(iCurrentComponent);
			Entity& physicsComponentEntity = physicsComponents.GetEntityFromComponentIndex(iCurrentComponent);

			//vel = (force / mass - drag * vel)* dt
			if (physicsComponent.m_props.m_mass > 0.f)
			{
				glm::vec2 acceleration = gravity;
				// TODO: is to add forces, external forces applied from gameplay
				acceleration += force / physicsComponent.m_props.m_mass;
				acceleration -= physicsComponent.m_props.m_drag * physicsComponent.m_velocity;
				physicsComponent.m_velocity += acceleration * updatePayload.m_deltaTime;
			}

			physicsComponentEntity.m_transform.AddToPosition(physicsComponent.m_velocity * updatePayload.m_deltaTime);
		}

		for (int iCollisionResolveLoop = 0; iCollisionResolveLoop < 4; ++iCollisionResolveLoop)
		{
			CollisionSolverLoopIteration(physicsComponents);
		}

		if (updatePayload.m_bShowDebugSymbols)
		{
			ApplicationCommandPool& poolToFill = *updatePayload.p_commandPoolToFill;
			// Update positions
			for (size_t iCurrentComponent = 0; iCurrentComponent < numberOfPhysicsComponents; iCurrentComponent++)
			{
				PhysicsComponent& physicsComponent = *physicsComponents.GetComponentFromIndex(iCurrentComponent);
				Entity& physicsComponentEntity = physicsComponents.GetEntityFromComponentIndex(iCurrentComponent);

				switch (physicsComponent.m_shape)
				{
					case EPhysicShapeType::Circle:
					{
						const glm::vec2 position = physicsComponentEntity.m_transform.GetPosition();
						const PhysicsCircleData& circleData = m_circleData[physicsComponent.m_shapeDataIndex];
						DrawCircle2DPixelSpace(poolToFill, position, circleData.m_radius, true, Vec4One);
						const glm::vec2 directionNormal = glm::normalize(physicsComponent.m_velocity);
						const glm::vec2 circleArrowEndPoint = position + directionNormal * circleData.m_radius;

						// Center to edge
						DrawLine2DPixelSpace(poolToFill, physicsComponentEntity.m_transform.GetPosition(), circleArrowEndPoint, true);

						const glm::vec2 ccwTangentVector = glm::vec2(-directionNormal.y, directionNormal.x);

						DrawLine2DPixelSpace(poolToFill, position + ccwTangentVector * circleData.m_radius * 0.5f, circleArrowEndPoint, true, glm::vec4(1.0, 0.0, 0.0, 1.0));
						DrawLine2DPixelSpace(poolToFill, position - ccwTangentVector * circleData.m_radius * 0.5f, circleArrowEndPoint, true, glm::vec4(1.0, 0.0, 0.0, 1.0));
					}
					break;
					case EPhysicShapeType::Line:
					{
						const PhysicsLineData& lineData = m_lineData[physicsComponent.m_shapeDataIndex];
						const glm::vec2 lineNormal = glm::vec2(cos(lineData.m_rotation), sin(lineData.m_rotation));
						const glm::vec2 linePointA = physicsComponentEntity.m_transform.GetPosition() + lineNormal * lineData.m_length * 0.5f;
						const glm::vec2 linePointB = physicsComponentEntity.m_transform.GetPosition() - lineNormal * lineData.m_length * 0.5f;
						DrawLine2DPixelSpace(poolToFill, linePointA, linePointB, true, Vec4One);
					}
					break;
				}
			}
		}
	}

	PhysicsComponent PhysicsSystem::CreatePhysicsLineComponent(PhysicsLineData lineData, PhysicsCollisionProperties props)
	{
		PhysicsComponent lineComponent{};
		lineComponent.m_shapeDataIndex = m_lineData.Size();
		lineComponent.m_shape = EPhysicShapeType::Line;
		m_lineData.Add(lineData);
		lineComponent.m_props = props;
		return lineComponent;
	}

	PhysicsComponent PhysicsSystem::CreatePhysicsCircleComponent(PhysicsCircleData circleData, PhysicsCollisionProperties props)
	{
		PhysicsComponent circleComponent{};
		circleComponent.m_shapeDataIndex = m_circleData.Size();
		circleComponent.m_shape = EPhysicShapeType::Circle;
		m_circleData.Add(circleData);
		circleComponent.m_props = props;
		return circleComponent;
	}

	void PhysicsSystem::ResolveCircleVCircleComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity)
	{
		glm::vec2 normalizedDirection;
		float distance;
		const PhysicsCircleData& circleData = m_circleData[physicsComponent.m_shapeDataIndex];
		const PhysicsCircleData& otherCircleData = m_circleData[otherPhysicsComponent.m_shapeDataIndex];
		
		if (!ResolveSphereVSphereCollision(physicsComponentEntity.m_transform.GetPosition(), circleData.m_radius, otherPhysicsComponentEntity.m_transform.GetPosition(), otherCircleData.m_radius, normalizedDirection, distance))
		{
			return;
		}
		float radiiSum = circleData.m_radius + otherCircleData.m_radius;
		float penetrationDistance = radiiSum - distance;

		ResolveCollisionInternal(physicsComponent, physicsComponentEntity, otherPhysicsComponent, otherPhysicsComponentEntity, distance, normalizedDirection, penetrationDistance);
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

		if (!ResolveSphereVLineCollision(linePointA, linePointB, circlePhysicsComponentEntity.m_transform.GetPosition(), circleData.m_radius, normalizedDirection, distance, collisionPointOnLine))
		{
			return;
		}
		float penetrationDistance = circleData.m_radius - distance;
		ResolveCollisionInternal(circlePhysicsComponent, circlePhysicsComponentEntity, linePhysicsComponent, linePhysicsComponentEntity, distance, normalizedDirection, penetrationDistance);
	}

	void PhysicsSystem::ResolveLineVLineComponentCollision(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity,  PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity)
	{
		glm::vec2 collisionPoint;
		const PhysicsLineData& lineData = m_lineData[physicsComponent.m_shapeDataIndex];
		const PhysicsLineData& otherLineData = m_lineData[otherPhysicsComponent.m_shapeDataIndex];

		if (!ResolveLineVLineCollision(physicsComponentEntity.m_transform.GetPosition(), lineData.m_rotation, lineData.m_length, otherPhysicsComponentEntity.m_transform.GetPosition(), otherLineData.m_rotation, otherLineData.m_length, collisionPoint))
		{
			return;
		}

		glm::vec2 normalizedDirection = glm::normalize(physicsComponentEntity.m_transform.GetPosition() - otherPhysicsComponentEntity.m_transform.GetPosition());
		ResolveCollisionInternal(physicsComponent, physicsComponentEntity, otherPhysicsComponent, otherPhysicsComponentEntity, 0.f, normalizedDirection, 0.f);
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

				// Check if both objects are static
				if (physicsComponentA.m_props.m_mass < FLT_EPSILON && physicsComponentB.m_props.m_mass < FLT_EPSILON)
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

	void PhysicsSystem::ResolveCollisionInternal(PhysicsComponent& physicsComponent, Entity& physicsComponentEntity, PhysicsComponent& otherPhysicsComponent, Entity& otherPhysicsComponentEntity, float distance, const glm::vec2& normalizedDirectionToCollision, float penetrationDistance)
	{
		const float invMass = physicsComponent.m_props.m_mass > 0.0f ? 1.0f / physicsComponent.m_props.m_mass : 0.0f;
		const float otherInvMass = otherPhysicsComponent.m_props.m_mass > 0.0f ? 1.0f / otherPhysicsComponent.m_props.m_mass : 0.0f;

		const float	percent = 0.80;// Penetration resolution percentage(70 %)
		const float slop = 0.005;// Penetration allowance

		const float correctionMagnitude = Math::Max(penetrationDistance - slop, 0.f) / (invMass + otherInvMass) * percent;
		const glm::vec2 correctionVector = glm::vec2(normalizedDirectionToCollision.x * correctionMagnitude, normalizedDirectionToCollision.y * correctionMagnitude);

		physicsComponentEntity.m_transform.AddToPosition(-glm::vec2(invMass * correctionVector.x, invMass * correctionVector.y));
		otherPhysicsComponentEntity.m_transform.AddToPosition(glm::vec2(otherInvMass * correctionVector.x, otherInvMass * correctionVector.y));

		const glm::vec2 relativeVelocity = otherPhysicsComponent.m_velocity - physicsComponent.m_velocity;
		const float velocityAlongNormal = glm::dot(relativeVelocity, normalizedDirectionToCollision);
		if (velocityAlongNormal > 0.0f)
		{
			return;
		}

		const float combinedRestitution = Math::Min(physicsComponent.m_props.m_bounciness, otherPhysicsComponent.m_props.m_bounciness);
		constexpr float restitutionThreshold = 1.0f;

		const float restitution = Math::Abs(velocityAlongNormal) < restitutionThreshold ? 0.0f : combinedRestitution;

		float impulseScalar = -(1.0 + restitution) * velocityAlongNormal;
		impulseScalar /= (invMass + otherInvMass);

		physicsComponent.m_velocity -= invMass * impulseScalar * normalizedDirectionToCollision;
		otherPhysicsComponent.m_velocity += otherInvMass * impulseScalar * normalizedDirectionToCollision;
	}
}
