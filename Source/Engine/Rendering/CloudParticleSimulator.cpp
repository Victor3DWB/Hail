#include "Engine_PCH.h"
#include "CloudParticleSimulator.h"

#include "HailEngine.h"
#include "Timer.h"
#include "Interface\DebugLineHelpers.h"
#include "RenderCommands.h"

#include "EngineUtility\Sorting.h"
#include "Input\InputHandler.h"


#include "Containers\StaticArray\StaticArray.h"

#include "imgui.h"

namespace Hail
{
	glm::vec2 spaceModifier = glm::vec2(100.f);

	void CollideWithBounds(glm::vec2& velocity, glm::vec2 posToCheck, glm::vec2* pPosToUpdate, float particleRadius)
	{
		float halfRadius = particleRadius * 0.5f;
		const float collisionDampening = 0.5f;
		glm::vec2 upperBounds = spaceModifier - halfRadius;
		glm::vec2 lowerBounds = glm::vec2(halfRadius);

		if (Math::Abs(posToCheck.x) > upperBounds.x)
		{
			if (pPosToUpdate)
				pPosToUpdate->x = upperBounds.x;
			velocity.x *= -1.f * collisionDampening;
		}
		if (Math::Abs(posToCheck.y) > upperBounds.y)
		{
			if (pPosToUpdate)
				pPosToUpdate->y = upperBounds.y;
			velocity.y *= -1.f * collisionDampening;
		}

		if (Math::Abs(posToCheck.x) < lowerBounds.x)
		{
			if (pPosToUpdate)
				pPosToUpdate->x = lowerBounds.x;
			velocity.x *= -1.f * collisionDampening;
		}
		if (Math::Abs(posToCheck.y) < lowerBounds.y)
		{
			if (pPosToUpdate)
				pPosToUpdate->y = lowerBounds.y;
			velocity.y *= -1.f * collisionDampening;
		}
	}


	float CubicSplineKernel(float r, float h)
	{
		if (r >= h) return 0.f;
		float q = r / h;
		float sigma = 40.0f / (7.0f * Math::PIf * h * h); // 2D normalization

		if (q >= 0.f && q < 0.5f)
		{
			return sigma * (1.0f + 6.f * ((q * q * q) - (q * q)));
		}
		else if (q >= 0.5f && q < 1.f)
		{
			float term = 1.0f - q;
			return sigma * 2.f * (term * term * term);
		}
	}

	float CubicSplineGradient(float r, float h)
	{
		if (r >= h) return 0.f;
		float q = r / h;
		float sigma = (7.0f * Math::PIf * h * h); // 2D normalization

		if (q >= 0.f && q < 0.5f)
		{
			sigma = 40.f / sigma;
			return sigma * (18.f * (q * q) - 12.f * q);
		}
		else if (q >= 0.5f && q < 1.f)
		{
			sigma = -(240.f / sigma);
			float term = 1.0f - q;
			return sigma * (term * term);
		}
	}

	float NearSmoothingKernel(float radius, float distance)
	{
		if (distance >= radius) return 0.f;
		// Volume for (r-d)^3
		float volume = (Math::PIf * powf(radius, 5.f)) * 0.1f;
		float value = radius - distance;
		return value * value * value / volume;
	}

	glm::vec2 VogelDirection(uint32 sampleIndex, uint32 samplesCount, float Offset)
	{
		float r = sqrt(float(sampleIndex) + 0.5f) / sqrt(float(samplesCount));
		float theta = float(sampleIndex) * Math::GoldenAngle + Offset;
		return r * glm::vec2(cos(theta), sin(theta));
	}

	const int32 HASH_SIZE = 0xffff;
	const float L = 0.2f;

	inline uint32 hasha(glm::ivec2& p) {
		int ix = (unsigned int)((p[0] + 2.f) / L);
		int iy = (unsigned int)((p[1] + 2.f) / L);
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663)) % HASH_SIZE;
	}

	glm::ivec2 PositionToCellCoord(glm::vec2 positionToCheck, float hRadius)
	{
		glm::vec2 worldCenter = glm::vec2(0.5f) * spaceModifier;
		glm::vec2 adjustedCenter = positionToCheck - worldCenter;

		return adjustedCenter / hRadius;
	}

	uint32 GenerateKeyFromCellPos(glm::ivec2 cellCoord, uint32 listMaxLength)
	{
		return hasha(cellCoord) % listMaxLength;
	}

	uint32 ComputeParticleKeyLookup(const CloudParticle& cloudParticle, uint32 listMaxLength, float hRadius)
	{
		glm::ivec2 cellCoord = PositionToCellCoord(cloudParticle.pos, hRadius);
		return GenerateKeyFromCellPos(cellCoord, listMaxLength);
	}

	glm::ivec2 ParticlePosToSdfCoords(glm::vec2 particlePosition)
	{
		glm::vec2 normalizedParticlePos = particlePosition / spaceModifier;
		return glm::ivec2(normalizedParticlePos.x * 128, normalizedParticlePos.y * 128);
	}

	glm::vec2 MouseForce(glm::uvec2 renderResolution, float radius, float mouseForceStrength, glm::vec2 posToCheck, glm::vec2 velocityOfParticle)
	{
		glm::vec2 mouseForce = glm::vec2(0.f);

		glm::vec2 mouseParticleSpacePos = (glm::vec2(GetInputHandler().GetInputMap().mouse.mousePos) / (float)renderResolution.y) * spaceModifier;

		glm::vec2 offset = posToCheck - mouseParticleSpacePos;
		float sqrDistance = glm::dot(offset, offset);

		const bool lmbDown = GetInputHandler().GetInputMap().mouse.keys[(uint32)eMouseMapping::LMB] == (uint8)eInputState::Down;
		const bool rmbDown = GetInputHandler().GetInputMap().mouse.keys[(uint32)eMouseMapping::RMB] == (uint8)eInputState::Down;

		float strength = 0.f;
		if (lmbDown)
		{
			strength = 1.f * mouseForceStrength;
		}
		else if (rmbDown)
		{
			strength = -1.f * mouseForceStrength;
		}

		if (strength != 0.f && sqrDistance < radius * radius)
		{
			float distance = glm::length(offset);

			// Normalized or Zero
			glm::vec2 dirToMouse = distance <= FLT_EPSILON ? glm::vec2(0.f) : offset / distance;

			float centreT = Math::Min(1.f - distance / radius, 0.8f);
			mouseForce += (dirToMouse * strength - velocityOfParticle) * centreT;
		}
		return mouseForce;
	}

	void CloudParticleSimulator::UpdateParticles(GrowingArray<CloudParticle>& cloudParticles, RenderCommandPool& poolOfCommands, glm::uvec2 resolution, const GrowingArray<float>& distanceField)
	{
		const float dt = GetRenderLoopTimer().GetDeltaTime();
		if (dt > 1.0)
			return;

		bool respawnGrid = false;
		if (!m_bUpdatedParticleGrid)
		{
			respawnGrid = true;
			m_cloudParticles.PrepareAndFill(cloudParticles.Size());
			for (uint32 i = 0; i < cloudParticles.Size(); i++)
			{
				m_cloudParticles[i] = cloudParticles[i];
				m_cloudParticles[i].pos *= spaceModifier;
			}
			m_bUpdatedParticleGrid = true;
		}



		ImGui::SliderFloat("Mouse force", &m_mouseForceStrength, 0.f, 100.f);
		ImGui::SliderFloat("Mouse radius", &m_mouseForceRadius, 0.f, 1.f);
		if (ImGui::SliderInt2("NumberOfPoints", &m_numberOfPointsToSpawn.x, 1, 100))
		{
			respawnGrid = true;
		}
		if (ImGui::SliderFloat("Grid Spacing", &m_particleSpacing, -1.f, 1.f))
		{
			respawnGrid = true;
		}
		// Settings
		if (ImGui::Checkbox("Simulate Grid", &m_bSimulateGrid))
		{
			respawnGrid = true;
		}
		ImGui::Checkbox("Draw Circles", &m_bShowCircles);
		ImGui::Checkbox("Draw Gradient Vectors", &m_bShowGradientValues);

		ImGui::SameLine();
		if (ImGui::Button("<") && m_particleToDebug != 0u)
		{
			--m_particleToDebug;
		}
		ImGui::SameLine();
		if (ImGui::Button(">"))
		{
			++m_particleToDebug;
		}

		if (respawnGrid)
		{
			m_particleGrid.RemoveAll();
			m_numberOfParticles = m_numberOfPointsToSpawn.x * m_numberOfPointsToSpawn.y;
			m_particleGridSize = Math::Max(m_particleGridSize, m_numberOfParticles);
			m_particleGrid.PrepareAndFill(m_particleGridSize);

			float spacing = m_particleKernelRadius * 2 + m_particleSpacing * spaceModifier.x;
			float rowLength = (1.f / (float)m_numberOfPointsToSpawn.x) * 0.5f * spaceModifier.x;
			float columnLength = (1.f / (float)m_numberOfPointsToSpawn.y) * 0.5f * spaceModifier.y;

			//glm::vec2 minSpace = glm::vec2(spaceModifier.x / 2 - rowLength / 2, spaceModifier.y / 2 - columnLength / 2);
			glm::vec2 minSpace = glm::vec2(spaceModifier.x / 2, spaceModifier.y / 2) - glm::vec2((float)m_numberOfPointsToSpawn.x * spacing / 2 , (float)m_numberOfPointsToSpawn.y * spacing / 2);

			uint32 numberOfPointsToSpawn = m_numberOfPointsToSpawn.x * m_numberOfPointsToSpawn.y;
			for (uint32 i = 0; i < numberOfPointsToSpawn; i++)
			{
				CloudParticle particle;

				uint32 xCoord = i % m_numberOfPointsToSpawn.x;
				uint32 yCoord = i / m_numberOfPointsToSpawn.x;
				particle.velocity = glm::vec2(0.f);
				float x = minSpace.x + xCoord * spacing;
				float y = minSpace.y + yCoord * spacing;
				particle.pos = glm::vec2(x, y);
				m_particleGrid[i] = particle;
			}
		}

		float horizontalAspectRatio = float(resolution.x) / float(resolution.y);
		float inverseHorizontalAspectRatio = 1.f / horizontalAspectRatio;

		if (!m_bSimulateGrid)
		{
			m_numberOfParticles = cloudParticles.Size();
			m_particleGridSize = cloudParticles.Capacity();
		}

		GrowingArray<CloudParticle>& particlesToSimulate = m_bSimulateGrid ? m_particleGrid : m_cloudParticles;

		{
			glm::vec2 mouseNormalizedPos = glm::vec2(GetInputHandler().GetInputMap().mouse.mousePos) / (float)resolution.y;
			glm::vec2 adjustedDebugPos = glm::vec2(mouseNormalizedPos.x * inverseHorizontalAspectRatio, mouseNormalizedPos.y);
			DrawCircle2D(poolOfCommands.m_debugLineCommands, inverseHorizontalAspectRatio, adjustedDebugPos, m_mouseForceRadius);
		}

		float h = m_particleKernelRadius;
		ImGui::SliderFloat("Particle Radius", &m_particleKernelRadius, 0.1f, 5.f);
		h = m_particleKernelRadius;
		float mass = m_mass;
		UpdatePartialLookupList(particlesToSimulate, h);
		UpdateParticlesBasedOnPaper(particlesToSimulate, h, resolution, dt, distanceField);

		ImGui::Text("Particle density: %f", particlesToSimulate[m_particleToDebug].densityNearDensity.x);
		ImGui::Text("Particle pressure: %f", particlesToSimulate[m_particleToDebug].pressure);
		ImGui::SameLine();
		ImGui::Text("Near pressure: %f", particlesToSimulate[m_particleToDebug].nearPressure);
		ImGui::Text("Particle pressureForce x: %f y: %f", particlesToSimulate[m_particleToDebug].pressureForce.x, particlesToSimulate[m_particleToDebug].pressureForce.y);

		DebugCircle debugCircle;
		debugCircle.scale = (h * (float)resolution.y) / spaceModifier.x;
		debugCircle.color = { 3.0 / 255.f, 169.f / 255.f, 252.f / 255.f };
		for (uint32 i = 0; i < m_numberOfParticles; i++)
		{
			glm::vec2 adjustedVelocity = glm::vec2(particlesToSimulate[i].velocity.x * inverseHorizontalAspectRatio, particlesToSimulate[i].velocity.y) / spaceModifier;
			float velocityLength = glm::length(particlesToSimulate[i].velocity);
			adjustedVelocity *= 10.f;
			const float normalizedPressure = (particlesToSimulate[i].pressure + 10.f) / 20.f;
			const float r = Math::Lerp(0.f, 255.f, Math::Clamp(0.f, 1.0f, velocityLength / (h / spaceModifier.x))) / 255.f;
			const float g = i == m_particleToDebug ? 225.f / 255.f : 188.f / 255.f;
			const float b = Math::Lerp(252.f, 3.f, Math::Clamp(0.f, 1.0f, normalizedPressure)) / 255.f;
			if (i == m_particleToDebug)
				debugCircle.color = { r, g, b };
			else
				debugCircle.color = { r, g, b };

			debugCircle.pos = particlesToSimulate[i].pos / spaceModifier;
			debugCircle.pos.x *= inverseHorizontalAspectRatio;
			if (m_bShowCircles)
				poolOfCommands.m_debugCircles.Add(debugCircle);

			if (m_bShowGradientValues)
			{


				glm::vec2 endPos = adjustedVelocity + debugCircle.pos;
				DrawLine2D(poolOfCommands.m_debugLineCommands, debugCircle.pos, endPos, Color::Red, Color::LimeGreen);
			}
		}

		if (!m_bSimulateGrid)
		{
			for (uint32 i = 0; i < cloudParticles.Size(); i++)
			{
				cloudParticles[i] = m_cloudParticles[i];
				cloudParticles[i].pos /= spaceModifier;
			}
		}
	}

	StaticArray<glm::ivec2, 9> cellOffsets = {
		{-1, -1},
		{0, -1},
		{1, -1},
		{-1, 0},
		{0, 0},
		{1, 0},
		{-1, 1},
		{0, 1},
		{1, 1}
	};

	void CloudParticleSimulator::UpdatePartialLookupList(GrowingArray<CloudParticle>& cloudParticles, float hRadius)
	{
		const uint32 numberOfParticles = m_numberOfParticles;
		m_particleSpatialLookup.RemoveAll();
		m_particleSpatialLookup.PrepareAndFill(m_particleGridSize);
		m_particleSpatialLookupStartIndices.PrepareAndFill(m_particleGridSize);
		for (uint32 i = 0; i < numberOfParticles; i++)
		{
			uint32 key = ComputeParticleKeyLookup(cloudParticles[i], m_particleGridSize, hRadius);
			m_particleSpatialLookup[i] = (SpatialIndexLookup{ key, i });
		}
		for (uint32 i = 0; i < m_particleGridSize; i++)
		{
			m_particleSpatialLookupStartIndices[i] = MAX_UINT;
		}

		SpatialIndexLookup* lookupStart = m_particleSpatialLookup.Data();
		Sorting::LinearBubbleParticleLookup(&lookupStart, numberOfParticles);
		
		if (numberOfParticles)
		{
			for (uint32 i = 0u; i < numberOfParticles; i++)
			{
				uint32 key = m_particleSpatialLookup[i].cellKey;
				uint32 keyPrevious = i == 0u ? MAX_UINT : m_particleSpatialLookup[i - 1].cellKey;
				if (key != keyPrevious)
				{
					m_particleSpatialLookupStartIndices[key] = (i);
				}
			}
		}
	}

	glm::vec2 CloudParticleSimulator::CalculateForceToSdf(const CloudParticle& cloudParticle,  const GrowingArray<float>& distanceField)
	{
		glm::ivec2 particleSdfPos = ParticlePosToSdfCoords(cloudParticle.pos);

		float sdfAtPos = distanceField[particleSdfPos.x + particleSdfPos.y * 128];

		if (sdfAtPos < -4.f)
			return glm::vec2(0.f);


		// calculate the normal to the closest cloud point
		int xShift = 1;
		int yShift = 1;

		if (particleSdfPos.x == 127)
			xShift = -1;
		if (particleSdfPos.y == 127)
			yShift = -1;

		float xSdf = distanceField[(particleSdfPos.x + xShift) + (particleSdfPos.y) * 128];
		float ySdf = distanceField[(particleSdfPos.x) + (particleSdfPos.y + yShift) * 128];

		float xGradient = xSdf - sdfAtPos;
		float yGradient = ySdf - sdfAtPos;

		xGradient = xGradient == 0.f ? (float)xShift : xGradient;
		yGradient = yGradient == 0.f ? (float)yShift : yGradient;

		glm::vec2 returnVec = glm::normalize(glm::vec2(xGradient, yGradient));
		if (std::isnan(returnVec.x) || std::isnan(returnVec.y))
			return glm::vec2(0.f);
		return returnVec;
	}

	void CloudParticleSimulator::UpdateParticlesBasedOnPaper(GrowingArray<CloudParticle>& cloudParticleList, float h, glm::uvec2 resolution, float actualDT, const GrowingArray<float>& distanceField)
	{
		ImGui::SliderFloat("Mass", &m_mass, 0.01f, 5.f);
		float mass = m_mass;
		ImGui::SliderFloat("Rest density", &m_restDensity, -10.f, 10.f);
		const float restDensity = m_restDensity;
		ImGui::SliderFloat("Stiffness", &m_stiffness, 0.f, 10.f);
		ImGui::SliderFloat("Near Pressure", &m_nearPressureMultiplier, 0.f, 50.f);
		const float stiffness = m_stiffness;
		ImGui::SliderFloat("Viscosity", &m_viscosity, 0.01f, 200.f);
		const float viscosity = m_viscosity;
		ImGui::SliderFloat("gravity", &m_gravity, -50.f, 50.f);
		glm::vec2 gravity = glm::vec2(0.f, m_gravity);
		ImGui::SliderFloat("Deltatime modifier", &m_deltaTimeModifier, 0.1f, 5.f);

		const float particleSize = h * 0.5f;
		const float deltaTime = Math::Min(0.005f, actualDT);
		//const float deltaTime = actualDT;
		const float adjustedDeltaTime = (particleSize / m_maxVelocity);

		//const float deltaTimeByMass = (Math::Min(deltaTime, adjustedDeltaTime) / mass);
		const float deltaTimeByMass = Math::Min(deltaTime, adjustedDeltaTime) * m_deltaTimeModifier;

		for (uint32 i = 0; i < m_numberOfParticles; i++)
		{
			CalculateDensityForEachPointWithinRadius(cloudParticleList, i, h, mass);
			cloudParticleList[i].pressure = stiffness * (cloudParticleList[i].densityNearDensity.x - restDensity);
			cloudParticleList[i].nearPressure = m_nearPressureMultiplier * (cloudParticleList[i].densityNearDensity.y);
		}

		for (uint32 i = 0; i < m_numberOfParticles; i++)
		{
			glm::vec2 F_viscosity = CalculateViscosityForceForEachPointWithinRadius(cloudParticleList, i, h, mass) * viscosity * mass;
			glm::vec2 mouseForce = MouseForce(resolution, m_mouseForceRadius * spaceModifier.x, m_mouseForceStrength, cloudParticleList[i].pos, cloudParticleList[i].velocity);
			glm::vec2 graviticForce = m_bSimulateGrid ? gravity : CalculateForceToSdf(cloudParticleList[i], distanceField) * m_gravity;

			glm::vec2 F_ext = mass * (graviticForce + mouseForce);

			glm::vec2 acceleration = (F_viscosity + F_ext);
			cloudParticleList[i].intermediateVelocity = cloudParticleList[i].velocity + deltaTimeByMass * acceleration;
		}

		for (uint32 i = 0; i < m_numberOfParticles; i++)
		{
			cloudParticleList[i].pressureForce = CalculatePressureForceForEachPointWithinRadius(cloudParticleList, i, h, mass, deltaTimeByMass);
		}

		m_maxVelocity = 0.f;
		for (uint32 i = 0; i < m_numberOfParticles; i++)
		{
			glm::vec2 acceleration_pressure = cloudParticleList[i].pressureForce / mass;

			cloudParticleList[i].velocity = (cloudParticleList[i].intermediateVelocity + deltaTimeByMass * acceleration_pressure);

			m_maxVelocity = Math::Max(m_maxVelocity, glm::dot(cloudParticleList[i].velocity, cloudParticleList[i].velocity));
			
			cloudParticleList[i].pos = cloudParticleList[i].pos + cloudParticleList[i].velocity;
			CollideWithBounds(cloudParticleList[i].velocity, cloudParticleList[i].pos, &cloudParticleList[i].pos, h);
		}
		m_maxVelocity = sqrt(m_maxVelocity);
	}

	// Compute density using cubic spline kernel
	void CloudParticleSimulator::CalculateDensityForEachPointWithinRadius(GrowingArray<CloudParticle>& cloudParticleList, uint32 indexToCheck, float h, float mass)
	{
		glm::vec2 calculatedDensity = glm::vec2(0.f);

		CloudParticle& particle = cloudParticleList[indexToCheck];
		particle.densityNearDensity.x = 0.f;
		particle.densityNearDensity.y = 0.f;
		glm::ivec2 cellCoord = PositionToCellCoord(particle.pos, h);

		for (uint32 iCellID = 0; iCellID < 9; iCellID++)
		{
			const glm::ivec2 cellCoordToTest = cellCoord + cellOffsets[iCellID];
			const uint32 key = GenerateKeyFromCellPos(cellCoordToTest, m_particleGridSize);
			const uint32 cellStartIndex = m_particleSpatialLookupStartIndices[key];
			for (uint32 i = cellStartIndex; i < m_numberOfParticles; i++)
			{
				if (m_particleSpatialLookup[i].cellKey != key)
					break;

				const uint32 particleIndex = m_particleSpatialLookup[i].particleIndex;
				glm::vec2 rij = particle.pos - cloudParticleList[particleIndex].pos;
				float r = glm::length(rij);
				particle.densityNearDensity.x += mass * CubicSplineKernel(r, h);
				particle.densityNearDensity.y += mass * NearSmoothingKernel(h, r);
			}
		}
	}

	glm::vec2 CloudParticleSimulator::CalculateViscosityForceForEachPointWithinRadius(GrowingArray<CloudParticle>& cloudParticleList, uint32 indexToCheck, float h, float mass)
	{
		glm::vec2 viscosityForce = glm::vec2(0.f);

		CloudParticle& pi = cloudParticleList[indexToCheck];
		glm::ivec2 cellCoord = PositionToCellCoord(pi.pos, h);

		for (uint32 iCellID = 0; iCellID < 9; iCellID++)
		{
			const glm::ivec2 cellCoordToTest = cellCoord + cellOffsets[iCellID];
			const uint32 key = GenerateKeyFromCellPos(cellCoordToTest, m_particleGridSize);
			const uint32 cellStartIndex = m_particleSpatialLookupStartIndices[key];
			for (uint32 i = cellStartIndex; i < m_numberOfParticles; i++)
			{
				if (m_particleSpatialLookup[i].cellKey != key)
					break;

				const uint32 particleIndex = m_particleSpatialLookup[i].particleIndex;

				if (particleIndex == indexToCheck)
					continue;

				const CloudParticle& pj = cloudParticleList[particleIndex];

				glm::vec2 rij = pi.pos - pj.pos;
				float r = glm::length(rij);
				if (r == 0.0f) continue; // avoid division by zero

				glm::vec2 velocityDiff = pj.velocity - pi.velocity;

				float smoothingKernelGradient = CubicSplineGradient(r, h);
				
				float gradW = smoothingKernelGradient;
				glm::vec2 gradientOfKernel = (rij / r) * gradW;
				float kernelGradientNormalized = (glm::length(gradientOfKernel) * 2.f) / r;

				viscosityForce += ((mass / pj.densityNearDensity.x) * velocityDiff * kernelGradientNormalized);
			}
		}

		return viscosityForce;
	}

	glm::vec2 CloudParticleSimulator::CalculatePressureForceForEachPointWithinRadius(GrowingArray<CloudParticle>& cloudParticleList, uint32 indexToCheck, float h, float mass, float dT)
	{
		glm::vec2 pressureForce = glm::vec2(0.f);

		CloudParticle& pi = cloudParticleList[indexToCheck];
		float piNearPressureTerm = pi.nearPressure / (pi.densityNearDensity.y * pi.densityNearDensity.y);
		float piPressureTerm = pi.pressure / (pi.densityNearDensity.x * pi.densityNearDensity.x);

		glm::vec2 piA = (pi.pos + pi.intermediateVelocity);

		glm::ivec2 cellCoord = PositionToCellCoord(piA, h);
		glm::vec2 randomFallbackDirection = VogelDirection(indexToCheck, 512u, (indexToCheck + 1u) * 0.01f);
		const float hSquared = h * h;

		for (uint32 iCellID = 0; iCellID < 9; iCellID++)
		{
			const glm::ivec2 cellCoordToTest = cellCoord + cellOffsets[iCellID];
			const uint32 key = GenerateKeyFromCellPos(cellCoordToTest, m_particleGridSize);
			const uint32 cellStartIndex = m_particleSpatialLookupStartIndices[key];
			for (uint32 i = cellStartIndex; i < m_numberOfParticles; i++)
			{
				if (m_particleSpatialLookup[i].cellKey != key)
					break;

				const uint32 particleIndex = m_particleSpatialLookup[i].particleIndex;

				if (particleIndex == indexToCheck)
					continue;

				const CloudParticle& pj = cloudParticleList[particleIndex];

				glm::vec2 rij = piA - (pj.pos + pj.intermediateVelocity);
				const float rijLengthSquard = rij.x * rij.x + rij.y * rij.y;
				if (rijLengthSquard >= hSquared)
					continue;

				float nearPressureTerm = piNearPressureTerm + (pj.nearPressure / (pj.densityNearDensity.y * pj.densityNearDensity.y));
				float pressureTerm = piPressureTerm + (pj.pressure / (pj.densityNearDensity.x * pj.densityNearDensity.x));

				float r = sqrt(rijLengthSquard);
				const float pressureModifier = 1.f - r / h;
				pressureTerm = pressureTerm * pressureModifier + nearPressureTerm * (pressureModifier * pressureModifier);

				bool useRandomDirection = false;
				float gradient = CubicSplineGradient(r, h);
				if (r < FLT_EPSILON)
					gradient *= 2.f;
				glm::vec2 normalizedDirection = r < FLT_EPSILON ? randomFallbackDirection : rij / r;
				glm::vec2 pressureGradient = normalizedDirection * gradient;
				
				pressureForce += mass * -pressureTerm * pressureGradient;
			}
		}
		return pressureForce;
	}
}
