#include "Engine_PCH.h"
#include "CloudRenderer.h"

#include "HailEngine.h"
#include "FrameBufferTexture.h"
#include "Settings.h"
#include "Renderer.h"
#include "Resources\ResourceManager.h"
#include "Resources\TextureManager.h"
#include "Resources\RenderingResourceManager.h"
#include "Resources\ResourceRegistry.h"
#include "Resources\MaterialManager.h"
#include "RenderContext.h"
#include "MathUtils.h"
#include "RenderCommands.h"
#include "Interface\DebugLineHelpers.h"
#include "imgui.h"

namespace Hail
{
	bool localCreateComputeShaderPipeline(MaterialManager* pMatManager, MaterialPipeline** pPipelineToCreate, const char* relativePath)
	{
		MaterialCreationProperties computeradixBuildHistogramProperties{};
		RelativeFilePath computeProjectPath(relativePath);
		ResourceRegistry& reg = GetResourceRegistry();
		if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
			pMatManager->ImportShaderResource(computeProjectPath.GetFilePath(), eShaderStage::Compute, false)))
		{
			computeradixBuildHistogramProperties.m_shaders[0].m_id = metaData->GetGUID();
			computeradixBuildHistogramProperties.m_shaders[0].m_type = eShaderStage::Compute;
		}
		computeradixBuildHistogramProperties.m_baseMaterialType = eMaterialType::CUSTOM;
		computeradixBuildHistogramProperties.m_typeRenderPass = eMaterialType::CUSTOM;
		
		*pPipelineToCreate = pMatManager->CreateMaterialPipeline(computeradixBuildHistogramProperties);
		return (*pPipelineToCreate) != nullptr;
	}

	void localCleanupBuffer(RenderingDevice* pDevice, BufferObject** pBuffer)
	{
		H_ASSERT((*pBuffer));
		(*pBuffer)->CleanupResource(pDevice);
		SAFEDELETE((*pBuffer));
	}

	void localCleanupPipeline(RenderingDevice* pDevice, MaterialPipeline** pPipeline)
	{
		H_ASSERT((*pPipeline));
		(*pPipeline)->CleanupResource(*pDevice);
		SAFEDELETE((*pPipeline));
	}

	void localCleanupTextureView(RenderingDevice* pDevice, TextureView** pTextureView)
	{
		H_ASSERT((*pTextureView));
		(*pTextureView)->CleanupResource(pDevice);
		SAFEDELETE((*pTextureView));
	}

	void localCleanupTextureResource(RenderingDevice* pDevice, TextureResource** pTextureResource)
	{
		H_ASSERT((*pTextureResource));
		(*pTextureResource)->CleanupResource(pDevice);
		SAFEDELETE((*pTextureResource));
	}

	glm::vec2 Vogel(uint32 sampleIndex, uint32 samplesCount, float Offset)
	{
		float r = sqrt(float(sampleIndex) + 0.5f) / sqrt(float(samplesCount));
		float theta = float(sampleIndex) * Math::GoldenAngle + Offset;
		return r * glm::vec2(cos(theta), sin(theta));
	}

	const uint32 HASH_SIZE = 0xffff;
	const float L = 0.2f;

	inline uint32 hasha(glm::ivec2& p) {
		int ix = (unsigned int)((p[0] + 2.f) / L);
		int iy = (unsigned int)((p[1] + 2.f) / L);
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663)) % HASH_SIZE;
	}

	GrowingArray<uint32> g_randomizedKeyes;

	void localCreateTempBufferToSort()
	{
		g_randomizedKeyes.Prepare(MaxNumberOfFluidParticles);
		StaticArray<uint32, 0xf + 1u> numbers;
		numbers.Fill(0u);
		uint32 above16Count = 0u;
		for (uint32 i = 0; i < MaxNumberOfFluidParticles; i++)
		{
			glm::vec2 randomPos = Vogel(i, 0xfff, 0.f) * 1024.f;
			uint32 hashValue = hasha(glm::ivec2(randomPos));

			//if (hashValue > 0xf)
			{
				uint32 digit = (hashValue >> 4) & 0xF;
				numbers[digit]++;
				above16Count++;
			}

			g_randomizedKeyes.Add(hashValue);
		}

		for (uint32 i = 0; i < 0xf + 1u; i++)
		{
			Debug_PrintConsoleString64(String64::Format("Number %u : %u", i + 0xf + 1u, numbers[i]));
		}
	}


	// TODO, link to website for credit
	// Dead reckoning alghorithm to generate a SDF to a texture from point data, found on the web. 
	void DeadReckoning(int width, int height, const GrowingArray<uint8>& binaryPixels, GrowingArray<float>& setSignedDistance) 
	{
		const auto contains = [&](int x, int y) -> bool 
		{
			return x >= 0 && x < width&& y >= 0 && y < height;
		};

		// I - a 2D binary image
		const bool outside = false;
		const bool inside = true;
		const auto I = [&](int x, int y) -> bool 
		{
			uint32 pixelCoord = x + y * width;
			return binaryPixels[pixelCoord] ? inside : outside;
		};

		// d - a 2D grey image representing the distance image
		GrowingArray<float> ds(width * height);
		ds.Fill();
		const float inf = static_cast<float>(256 * 256 * 256);
		const auto getd = [&](int x, int y) -> float { return ds[y * width + x]; };
		const auto setd = [&](int x, int y, float v) { ds[y * width + x] = v; };
		const auto distance = [](int x, int y) -> float 
		{
			return hypotf(static_cast<float>(x), static_cast<float>(y));
		};

		// p - for each pixel, the corresponding border point
		struct Point { int x, y; };
		GrowingArray<Point> ps(width * height);
		ps.Fill();
		const Point outOfBounds = { -1, -1 };
		const auto getp = [&](int x, int y) -> Point { return ps[y * width + x]; };
		const auto setp = [&](int x, int y, const Point& p) { ps[y * width + x] = p; };

		// initialize d
		// initialize immediate interior & exterior elements
		for (int y = 0; y < height; ++y) 
		{
			for (int x = 0; x < width; ++x) 
			{
				const bool c = I(x, y);
				const bool w = contains(x - 1, y) ? I(x - 1, y) : outside;
				const bool e = contains(x + 1, y) ? I(x + 1, y) : outside;
				const bool n = contains(x, y - 1) ? I(x, y - 1) : outside;
				const bool s = contains(x, y + 1) ? I(x, y + 1) : outside;
				if ((w != c) || (e != c) || (n != c) || (s != c)) 
				{
					setd(x, y, 0);
					setp(x, y, { x, y });
				}
				else 
				{
					setd(x, y, inf);
					setp(x, y, outOfBounds);
				}
			}
		}

		// Perform minimum distance choice for single pixel, single direction.
		enum class Dir 
		{
			NW, N, NE,      // NW=(x-1, y-1), N=(x, y-1), NE=(x+1, y-1)
			W, E,       // W =(x-1, y),               E =(x+1, y)
			SW, S, SE       // SW=(x-1, y+1), S=(x, y+1), SE=(x+1, y+1)
		};
		const auto f = [&](int x, int y, Dir dir) 
		{
			// d1 - distance between two adjacent pixels in either the x or y direction
			const float d1 = 1.0f;

			// d2 - distance between two diagonally adjacent pixels (sqrt(2))
			const float d2 = d1 * 1.4142135623730950488f;

			int dx, dy;
			float od;
			switch (dir) 
			{
			default:
			case Dir::NW: dx = -1; dy = -1; od = d2; break; // first pass
			case Dir::N:  dx = 0; dy = -1; od = d1; break; // first pass
			case Dir::NE: dx = 1; dy = -1; od = d2; break; // first pass
			case Dir::W:  dx = -1; dy = 0; od = d1; break; // first pass
			case Dir::E:  dx = 1; dy = 0; od = d1; break; // final pass
			case Dir::SW: dx = -1; dy = 1; od = d2; break; // final pass
			case Dir::S:  dx = 0; dy = 1; od = d1; break; // final pass
			case Dir::SE: dx = 1; dy = 1; od = d2; break; // final pass
			}
			const bool b = contains(x + dx, y + dy);
			const float cd = b ? getd(x + dx, y + dy) : inf;
			if (cd + od < getd(x, y)) 
			{
				const Point p = b ? getp(x + dx, y + dy) : outOfBounds;
				setp(x, y, p);
				const int xx = x - p.x;
				const int yy = y - p.y;
				const float nd = distance(xx, yy);
				setd(x, y, nd);
			}
		};

		// perform the first pass
		for (int y = 0; y < height; ++y) 
		{           // top to bottom
			for (int x = 0; x < width; ++x) 
			{        // left to right
				static const Dir dirs[] = { Dir::NW, Dir::N, Dir::NE, Dir::W };
				for (const Dir dir : dirs) 
				{
					f(x, y, dir);
				}
			}
		}

		// perform the final pass
		for (int y = height - 1; y >= 0; --y) 
		{        // bottom to top
			for (int x = width - 1; x >= 0; --x) 
			{     // right to left
				static const Dir dirs[] = { Dir::E, Dir::SW, Dir::S, Dir::SE };
				for (const Dir dir : dirs) 
				{
					f(x, y, dir);
				}
			}
		}

		// indicate inside & outside
		for (int y = 0; y < height; ++y) 
		{
			for (int x = 0; x < width; ++x) 
			{
				float d = getd(x, y);
				if (I(x, y) == outside) 
				{
					d = -d;                         // Negative distance means outside.
				}

				uint32 pixelCoord = x + y * width;
				setSignedDistance[pixelCoord] = d * -1.f;
			}
		}
	}


	constexpr uint32 locMaxNumberOfGlyphlets = 1028u;

	CloudRenderer::~CloudRenderer()
	{
		H_ASSERT(m_pCloudPipeline == nullptr);
		//H_ASSERT(m_pCloudBuffer == nullptr);
	}

	CloudRenderer::CloudRenderer(Renderer* pRenderer, ResourceManager* pResourceManager)
		: m_pRenderer(pRenderer)
		, m_pResourceManager(pResourceManager) 
		, m_pCloudPipeline(nullptr)
		//, m_pCloudBuffer(nullptr)
		, m_pSdfTexture(nullptr)
		, m_pSdfView(nullptr)
		, m_numberOfPointsUploaded(0u)
	{
	}

	void CloudRenderer::Initialize(ErrorManager* pErrorManager)
	{
		// Hard coded random array to sort
		localCreateTempBufferToSort();
		// Hard coded shape for debugging
		GrowingArray<glm::uvec2> pointBaseList = {
			{1, 7},
			{2, 7},
			{2, 6},
			{3, 7},
			{3, 6},
			{3, 4},
			{4, 7},
			{4, 6},
			{4, 5},
			{4, 4},
			{4, 3},
			{4, 2},
			{5, 7},
			{5, 6},
			{5, 5},
			{5, 4},
			{5, 3},
			{5, 2},
			{6, 7},
			{6, 6},
			{6, 5},
			{6, 4},
			{6, 3},
			{7, 7},
			{7, 6},
			{7, 5},
			{7, 4},
			{7, 3},
			{7, 2},
			{8, 7},
			{8, 6},
			{8, 5},
			{9, 7}
		};

		CloudParticle particle;
		particle.velocity = glm::vec2(0.f);
		m_cloudParticles.Prepare(pointBaseList.Size() + 2u);
		for (uint32 i = 0; i < pointBaseList.Size(); i++)
		{
			const uint32 rnd = ((pointBaseList[i].x * 5u + pointBaseList[i].y * 7u) >> (pointBaseList[i].x & 3u));
			for (uint32 iPRand = 0; iPRand < 4; ++iPRand)
			{
				glm::vec2 vogelNoise = Vogel(rnd + iPRand, 128u, 0.1 * iPRand) * 0.5f;
				glm::vec2 finalPos = (glm::vec2( pointBaseList[i].x + 0.5, pointBaseList[i].y + 0.5) / 11.f + vogelNoise / 11.f);

				particle.pos = finalPos;
				m_cloudParticles.Add(particle);
			}
		}
		const float cloudPointRadius = 0.1f;
		const float cloudPointRadiusSq = cloudPointRadius * cloudPointRadius;
		uint32 width = 128u;
		uint32 height = 128u;
		GrowingArray<uint8> imageRepresentationOfCloud(width * height);
		imageRepresentationOfCloud.Fill();
		for (uint32 x = 0; x < width; x++)
		{
			for (uint32 y = 0; y < height; y++)
			{
				uint32 imageCoord = x + y * height;
				glm::vec2 cloudCoord = glm::vec2((float)(x + 0.5) / width, (float)(y + 0.5) / height);
				uint32 numberOfPointsInRadius = 0;
				for (uint32 iPoint = 0; iPoint < m_cloudParticles.Size(); iPoint++)
				{
					glm::vec2 cloudPointToSamplePoint = cloudCoord - m_cloudParticles[iPoint].pos;
					float distanceSquared = glm::dot(cloudPointToSamplePoint, cloudPointToSamplePoint);
					if (distanceSquared <= cloudPointRadiusSq)
					{
						numberOfPointsInRadius++;
					}
				}
				imageRepresentationOfCloud[imageCoord] = numberOfPointsInRadius >= 4u ? numberOfPointsInRadius : 0;
			}
		}
		m_cloudSdfTexture.PrepareAndFill(width * height);
		DeadReckoning(width, height, imageRepresentationOfCloud, m_cloudSdfTexture);

		particle.pos = glm::vec2(0.9, 0.1);
		m_cloudParticles.Add(particle);
		particle.pos = glm::vec2(0.8, 0.23);
		m_cloudParticles.Add(particle);

		RenderContext* pContext = m_pRenderer->GetCurrentContext();

		CompiledTexture sdfTextureValues;
		sdfTextureValues.properties.height = height;
		sdfTextureValues.properties.width = width;
		sdfTextureValues.properties.textureType = (uint32)eTextureSerializeableType::R32F;
		sdfTextureValues.properties.format = eTextureFormat::R32_SFLOAT;
		sdfTextureValues.compiledColorValues = m_cloudSdfTexture.Data();
		sdfTextureValues.loadState = TEXTURE_LOADSTATE::LOADED_TO_RAM;
		m_pSdfTexture = m_pResourceManager->GetTextureManager()->CreateTexture(pContext, "CloudSdfTexture", sdfTextureValues);

		TextureViewProperties viewProps;
		viewProps.pTextureToView = m_pSdfTexture;
		viewProps.viewUsage = eTextureUsage::Texture;
		viewProps.accessQualifier = eShaderAccessQualifier::ReadOnly;
		m_pSdfView = m_pResourceManager->GetTextureManager()->CreateTextureView(viewProps);

		BufferProperties cloudListBufferProps;
		cloudListBufferProps.elementByteSize = sizeof(CloudData);
		cloudListBufferProps.numberOfElements = MaxNumberOfClouds;
		cloudListBufferProps.type = eBufferType::structured;
		cloudListBufferProps.domain = eShaderBufferDomain::CpuToGpu;
		cloudListBufferProps.updateFrequency = eShaderBufferUpdateFrequency::Sporadic;
		cloudListBufferProps.accessQualifier = eShaderAccessQualifier::ReadOnly;
		m_pCloudListBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(cloudListBufferProps, "Cloud List Buffer");

		BufferProperties particleBufferProps;
		particleBufferProps.elementByteSize = sizeof(CloudParticle);
		particleBufferProps.numberOfElements = MaxNumberOfFluidParticles;
		particleBufferProps.type = eBufferType::structured;
		particleBufferProps.domain = eShaderBufferDomain::GpuOnly;
		particleBufferProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
		particleBufferProps.updateFrequency = eShaderBufferUpdateFrequency::Never;
		m_pParticleBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleBufferProps, "Fluid Particle Buffer");

		BufferProperties particleLookupBufferProps;
		particleLookupBufferProps.elementByteSize = sizeof(glm::uvec2);
		particleLookupBufferProps.numberOfElements = MaxNumberOfFluidParticles;
		particleLookupBufferProps.type = eBufferType::structured;
		particleLookupBufferProps.domain = eShaderBufferDomain::GpuOnly;
		particleLookupBufferProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
		particleLookupBufferProps.updateFrequency = eShaderBufferUpdateFrequency::Never;
		// Create 2 as we need to have two for sorting
		m_pParticleLookupBufferRead = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleLookupBufferProps, "Fluid Particle Lookup Buffer Read");
		m_pParticleLookupBufferWrite = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleLookupBufferProps, "Fluid Particle Lookup Buffer Write");
		particleLookupBufferProps.elementByteSize = sizeof(uint32);
		m_pParticleLookupStartIndicesBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleLookupBufferProps, "Fluid Particle Lookup StartIndices Buffer");
		m_pParticleLookupSortIndicesBufferRead = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleLookupBufferProps, "Fluid Particle Lookup SortIndices Buffer Read");
		m_pParticleLookupSortIndicesBufferWrite = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleLookupBufferProps, "Fluid Particle Lookup SortIndices Buffer Write");

		ResourceRegistry& reg = GetResourceRegistry();
		MaterialManager* pMatManager = m_pResourceManager->GetMaterialManager();
		RelativeFilePath fullscreenVertexShaderPath("resources/shaders/VS_fullscreenPass.shr");
		{
			MaterialCreationProperties matProperties{};
			if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
				pMatManager->ImportShaderResource(fullscreenVertexShaderPath.GetFilePath(), eShaderStage::Vertex)))
			{
				matProperties.m_shaders[0].m_id = metaData->GetGUID();
				matProperties.m_shaders[0].m_type = eShaderStage::Vertex;
			}

			RelativeFilePath fragmentProjectPath("resources/shaders/FS_cloudDrawing.shr");
			if (const MetaResource* metaData = reg.GetResourceMetaInformation(ResourceType::Shader,
				pMatManager->ImportShaderResource(fragmentProjectPath.GetFilePath(), eShaderStage::Fragment)))
			{
				matProperties.m_shaders[1].m_id = metaData->GetGUID();
				matProperties.m_shaders[1].m_type = eShaderStage::Fragment;
			}

			matProperties.m_baseMaterialType = eMaterialType::CUSTOM;
			matProperties.m_typeRenderPass = eMaterialType::FULLSCREEN_PRESENT_LETTERBOX;
			m_pCloudPipeline = pMatManager->CreateMaterialPipeline(matProperties);
		}


		m_numberOfPointsUploaded = m_cloudParticles.Size();
		bool bValidComputePasses = true;

		if (!localCreateComputeShaderPipeline(pMatManager, &m_pCloudCoveragePipeline, "resources/shaders/CS_calculateCloudCoverage.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pRadixBuildHistogramPipeline, "resources/shaders/CS_fluidParticleRadixSort_buildHistogram.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pRadixBuildOffsetTablePipeline, "resources/shaders/CS_fluidParticleRadixSort_buildOffsetTable.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pRadixShuffleDataPipeline, "resources/shaders/CS_fluidParticleRadixSort_shuffleData.shr"))
		{
			bValidComputePasses = false;
		}
 		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationCalculateDensity, "resources/shaders/CS_fluidParticleCalculateDensity.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationCalculateIntermediateVelocity, "resources/shaders/CS_fluidParticleCalculateIntermediateVelocity.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationCalculatePressureForce, "resources/shaders/CS_fluidParticleCalculatePressureForce.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationApplyPosition, "resources/shaders/CS_fluidParticleApplyPosition.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationBuildPartialLookup, "resources/shaders/CS_fluidParticleBuildPartialLook.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationClearPartialLookup, "resources/shaders/CS_fluidParticleClearLookup.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationUpdateUniforms, "resources/shaders/CS_fluidParticleUpdateUniforms.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pSimulationUpdateParticleListFromCPU, "resources/shaders/CS_fluidParticleUpdateParticleListFromCPU.shr"))
		{
			bValidComputePasses = false;
		}

		{
			BufferProperties particleSortGlobalHistogramBufferProps;
			particleSortGlobalHistogramBufferProps.elementByteSize = sizeof(uint32);
			particleSortGlobalHistogramBufferProps.numberOfElements = 16u * (MaxNumberOfSortBuckets);
			particleSortGlobalHistogramBufferProps.type = eBufferType::structured;
			particleSortGlobalHistogramBufferProps.domain = eShaderBufferDomain::GpuOnly;
			particleSortGlobalHistogramBufferProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
			particleSortGlobalHistogramBufferProps.updateFrequency = eShaderBufferUpdateFrequency::Never;
			m_pParticleSortGlobalHistogramBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleSortGlobalHistogramBufferProps, "Particle Sorting Global Histogram Buffer");
			m_pParticleSortLocalHistogramBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleSortGlobalHistogramBufferProps, "Particle Sorting Local Histogram Buffer");
		}
		{
			BufferProperties particleSortGlobalBucketOffsetBufferProps;
			particleSortGlobalBucketOffsetBufferProps.elementByteSize = sizeof(uint32);
			particleSortGlobalBucketOffsetBufferProps.numberOfElements = 16u;
			particleSortGlobalBucketOffsetBufferProps.type = eBufferType::structured;
			particleSortGlobalBucketOffsetBufferProps.domain = eShaderBufferDomain::GpuOnly;
			particleSortGlobalBucketOffsetBufferProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
			particleSortGlobalBucketOffsetBufferProps.updateFrequency = eShaderBufferUpdateFrequency::Never;
			m_pParticleSortBucketOffsetBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleSortGlobalBucketOffsetBufferProps, "Particle Sorting Global Bucket Offset Buffer");
		}

		BufferProperties particleUniformBufferProps;
		particleUniformBufferProps.elementByteSize = sizeof(ParticleUniformBuffer);
		particleUniformBufferProps.numberOfElements = 1;
		particleUniformBufferProps.type = eBufferType::uniform;
		particleUniformBufferProps.domain = eShaderBufferDomain::CpuToGpu;
		particleUniformBufferProps.accessQualifier = eShaderAccessQualifier::ReadOnly;
		particleUniformBufferProps.updateFrequency = eShaderBufferUpdateFrequency::PerFrame;
		m_pParticleUniformBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleUniformBufferProps, "Particle Uniform Buffer");

		BufferProperties particleDynamicBufferProps;
		particleDynamicBufferProps.elementByteSize = sizeof(ParticleDynamicVariableBuffer);
		particleDynamicBufferProps.numberOfElements = 1;
		particleDynamicBufferProps.type = eBufferType::structured;
		particleDynamicBufferProps.domain = eShaderBufferDomain::GpuOnly;
		particleDynamicBufferProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
		particleDynamicBufferProps.updateFrequency = eShaderBufferUpdateFrequency::Never;
		m_pDynamicParticleVariableBuffer = m_pResourceManager->GetRenderingResourceManager()->CreateBuffer(particleDynamicBufferProps, "Particle Dynamic Variable Buffer");

		TextureProperties particleCoverageTextureProps;
		particleCoverageTextureProps.height = m_pResourceManager->GetSwapChain()->GetTargetResolution().y;
		particleCoverageTextureProps.width = m_pResourceManager->GetSwapChain()->GetTargetResolution().x;
		particleCoverageTextureProps.format = eTextureFormat::R32G32B32A32_SFLOAT;
		particleCoverageTextureProps.accessQualifier = eShaderAccessQualifier::ReadWrite;
		particleCoverageTextureProps.textureUsage = eTextureUsage::Texture;
		
		m_pParticleCoverageTexture[0] = m_pResourceManager->GetTextureManager()->CreateTexture(pContext, "Particle Coverage Texture 0", particleCoverageTextureProps);
		m_pParticleCoverageTexture[1] = m_pResourceManager->GetTextureManager()->CreateTexture(pContext, "Particle Coverage Texture 1", particleCoverageTextureProps);

		TextureViewProperties particleCoverageProps;
		particleCoverageProps.pTextureToView = m_pParticleCoverageTexture[0];
		particleCoverageProps.viewUsage = eTextureUsage::Texture;
		particleCoverageProps.accessQualifier = eShaderAccessQualifier::WriteOnly;
		m_pParticleCoverageViewWrite[0] = m_pResourceManager->GetTextureManager()->CreateTextureView(particleCoverageProps);
		particleCoverageProps.pTextureToView = m_pParticleCoverageTexture[1];
		m_pParticleCoverageViewWrite[1] = m_pResourceManager->GetTextureManager()->CreateTextureView(particleCoverageProps);

		TextureViewProperties particleCoveragePropsRead;
		particleCoveragePropsRead.pTextureToView = m_pParticleCoverageTexture[0];
		particleCoveragePropsRead.viewUsage = eTextureUsage::Texture;
		particleCoveragePropsRead.accessQualifier = eShaderAccessQualifier::ReadOnly;
		m_pParticleCoverageViewRead[0] = m_pResourceManager->GetTextureManager()->CreateTextureView(particleCoveragePropsRead);
		particleCoveragePropsRead.pTextureToView = m_pParticleCoverageTexture[1];
		m_pParticleCoverageViewRead[1] = m_pResourceManager->GetTextureManager()->CreateTextureView(particleCoveragePropsRead);

		if (!localCreateComputeShaderPipeline(pMatManager, &m_pJFAPipelines[0], "resources/shaders/CS_jumpFloodAlgo.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pJFAPipelines[1], "resources/shaders/CS_jumpFloodAlgo.shr"))
		{
			bValidComputePasses = false;
		}
		if (!localCreateComputeShaderPipeline(pMatManager, &m_pCollectJFAPipeline, "resources/shaders/CS_gatherDistancesFromJFA.shr"))
		{
			bValidComputePasses = false;
		}

		if (!bValidComputePasses)
		{
			pErrorManager->AddErrors(EStartupErrors::InitCloudRenderer, EErrorType::Startup);
			pErrorManager->AddString("Failed to load compute shader.");
		}

		if (!m_pCloudPipeline)
		{
			pErrorManager->AddErrors(EStartupErrors::InitCloudRenderer, EErrorType::Startup);
			pErrorManager->AddString("Failed to load fullscreen cloud pipeline effect.");
		}

		m_pTileableCloudTextureView = m_pResourceManager->GetTextureManager()->CreateTextureView(RelativeFilePath("resources/textures/tileAbleCloud.txr"), pContext);
	}

	void CloudRenderer::Cleanup()
	{
		RenderingDevice* pDevice = m_pRenderer->GetRenderingDevice();
		localCleanupBuffer(pDevice, &m_pParticleSortGlobalHistogramBuffer);
		localCleanupBuffer(pDevice, &m_pParticleSortLocalHistogramBuffer);
		localCleanupBuffer(pDevice, &m_pParticleSortBucketOffsetBuffer);
		localCleanupBuffer(pDevice, &m_pParticleBuffer);
		localCleanupBuffer(pDevice, &m_pParticleUniformBuffer);
		localCleanupBuffer(pDevice, &m_pDynamicParticleVariableBuffer);
		localCleanupBuffer(pDevice, &m_pParticleLookupBufferRead);
		localCleanupBuffer(pDevice, &m_pParticleLookupBufferWrite);
		localCleanupBuffer(pDevice, &m_pParticleLookupStartIndicesBuffer);
		localCleanupBuffer(pDevice, &m_pParticleLookupSortIndicesBufferRead);
		localCleanupBuffer(pDevice, &m_pParticleLookupSortIndicesBufferWrite);
		localCleanupBuffer(pDevice, &m_pCloudListBuffer);

		// Pipelines
		localCleanupPipeline(pDevice, &m_pCloudPipeline);
		localCleanupPipeline(pDevice, &m_pCloudCoveragePipeline);
		localCleanupPipeline(pDevice, &m_pRadixBuildHistogramPipeline);
		localCleanupPipeline(pDevice, &m_pRadixBuildOffsetTablePipeline);
		localCleanupPipeline(pDevice, &m_pRadixShuffleDataPipeline);

		localCleanupPipeline(pDevice, &m_pSimulationUpdateUniforms);
		localCleanupPipeline(pDevice, &m_pSimulationUpdateParticleListFromCPU);
		localCleanupPipeline(pDevice, &m_pSimulationCalculateDensity);
		localCleanupPipeline(pDevice, &m_pSimulationCalculateIntermediateVelocity);
		localCleanupPipeline(pDevice, &m_pSimulationCalculatePressureForce);
		localCleanupPipeline(pDevice, &m_pSimulationApplyPosition);
		localCleanupPipeline(pDevice, &m_pSimulationBuildPartialLookup);
		localCleanupPipeline(pDevice, &m_pSimulationClearPartialLookup);
		localCleanupPipeline(pDevice, &m_pCollectJFAPipeline);

		localCleanupPipeline(pDevice, &m_pJFAPipelines[0]);
		localCleanupPipeline(pDevice, &m_pJFAPipelines[1]);


		m_pTileableCloudTextureView = nullptr;

		localCleanupTextureView(pDevice, &m_pSdfView);
		localCleanupTextureResource(pDevice, &m_pSdfTexture);

		localCleanupTextureView(pDevice, &m_pParticleCoverageViewWrite[0]);
		localCleanupTextureView(pDevice, &m_pParticleCoverageViewRead[0]);
		localCleanupTextureResource(pDevice, &m_pParticleCoverageTexture[0]);
		localCleanupTextureView(pDevice, &m_pParticleCoverageViewWrite[1]);
		localCleanupTextureView(pDevice, &m_pParticleCoverageViewRead[1]);
		localCleanupTextureResource(pDevice, &m_pParticleCoverageTexture[1]);
	}

	float hashwithoutsine11(float p)
	{
		p = fmodf(p * 0.1031, 1.0f);
		p *= p + 33.33;
		p *= p + p;
		return fmodf(p, 1.0f);
	}

	void CloudRenderer::Prepare(PrepareParams prepareParams)
	{
		RenderCommandPool& poolOfCommands = *prepareParams.m_pPoolOfCommands;

		m_frameRenderSettings = prepareParams.m_frameRenderSettings;

		const float aspectRatio = m_pResourceManager->GetSwapChain()->GetTargetHorizontalAspectRatio();
		bool bRespawnParticles = false;

		if (m_frameRenderSettings.m_bEnableImgui && m_frameRenderSettings.m_bEnableGpuParticles)
		{
			ImGui::Begin("Particle test window");
		}

		if (m_frameRenderSettings.m_bEnableGpuParticles)
		{
			m_ParticleUniforms.bSimulateCloud = true;

			if (m_frameRenderSettings.m_bEnableImgui)
			{
				ImGui::SliderFloat("Mouse force", &m_ParticleUniforms.mouseForceStrength, 0.f, 100.f);
				ImGui::SliderFloat("Mouse radius", &m_ParticleUniforms.mouseForceRadius, 0.f, 1.f);
				ImGui::SliderFloat("Cloud size multiplier", &m_ParticleUniforms.cloudSizeMultiplier, 1.f, 8.f);
				if (ImGui::SliderInt("Cloud Num Points per Pixel", &m_numberOfPointsPerPixelInClouds, 1, 32))
				{
					bRespawnParticles = true;
				}
				ImGui::SliderFloat("Particle Radius", &m_ParticleUniforms.particleKernelRadius, 0.1f, 5.f);
				ImGui::SliderFloat("Mass", &m_ParticleUniforms.mass, 0.01f, 5.f);
				ImGui::SliderFloat("Rest density", &m_ParticleUniforms.restDensity, -10.f, 10.f);
				ImGui::SliderFloat("Stiffness", &m_ParticleUniforms.stiffness, 0.f, 10.f);
				ImGui::SliderFloat("Near Pressure", &m_ParticleUniforms.nearPressureMultiplier, 0.f, 50.f);
				ImGui::SliderFloat("Viscosity", &m_ParticleUniforms.viscosityModifier, 0.01f, 200.f);
				ImGui::SliderFloat("Gravitic Force", &m_ParticleUniforms.graviticForce, -1.f, 1.f);
				ImGui::SliderFloat("Cloud dampening multiplier", &m_ParticleUniforms.cloudDampeningMultiplier, 0, 1.f);
				ImGui::SliderFloat("Deltatime modifier", &m_ParticleUniforms.deltaTimeModifier, 0.1f, 5.f);

				ImGui::Separator();

				if (ImGui::SliderInt("Number of Clouds", &m_numberOfClouds, 1, MaxNumberOfClouds))
				{
					bRespawnParticles = true;
				}
			}


			if (!m_bUpdatedParticleGrid)
			{
				m_numberOfClouds++;
				m_bUpdatedParticleGrid = true;
			}

			int32 numberOfCloudsToAdd = m_numberOfClouds - m_cloudList.Size();
			m_cloudList.Resize(m_numberOfClouds);
			for (int i = 0; i < numberOfCloudsToAdd; i++)
			{
				int iPlusOne = m_cloudList.Size() + 1;
				const uint32 rnd = ((iPlusOne * 7u) >> (iPlusOne * 16u & 3u));
				float noise1 = hashwithoutsine11(iPlusOne);
				float noise2 = hashwithoutsine11(iPlusOne * rnd);
				CloudData& cloud = m_cloudList.Add();
				cloud.position = glm::vec2((noise1 * 2.0 - 1.0) * 720.f, (noise2 * 2.0 - 1.0) * 720.f);
				noise1 = noise1 < 0.25f ? noise1 * 8 : noise1;
				noise2 = noise2 < 0.25f ? noise2 * 8 : noise2;
				cloud.dimensions.x *= Math::Max(noise1, noise2);
				cloud.dimensions.y *= Math::Min(noise1, noise2);
			}
			if (numberOfCloudsToAdd)
				bRespawnParticles = true;

			if (m_frameRenderSettings.m_bEnableImgui)
			{
				if (ImGui::TreeNode("Clouds"))
				{
					for (int32 i = 0; i < m_numberOfClouds; i++)
					{
						// Use SetNextItemOpen() so set the default state of a node to be open. We could
						// also use TreeNodeEx() with the ImGuiTreeNodeFlags_DefaultOpen flag to achieve the same thing!
						if (i == 0)
							ImGui::SetNextItemOpen(true, ImGuiCond_Once);

						// Here we use PushID() to generate a unique base ID, and then the "" used as TreeNode id won't conflict.
						// An alternative to using 'PushID() + TreeNode("", ...)' to generate a unique ID is to use 'TreeNode((void*)(intptr_t)i, ...)',
						// aka generate a dummy pointer-sized value to be hashed. The demo below uses that technique. Both are fine.
						ImGui::PushID(i);
						if (ImGui::TreeNode("", "Cloud %d", i))
						{
							//if (ImGui::SliderFloat2("Cloud Size", &m_cloudList[i].dimensions.x, 0.f, 512.f))
							//{
							//	bRespawnParticles = true;
							//}
							ImGui::SliderFloat2("Cloud Position", &m_cloudList[i].position.x, -512.f, 512.f);
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}

				ImGui::Separator();

				ImGui::SliderFloat("Effect Sun Direction Rad", &m_ParticleUniforms.effectSunDirectionRadian, 0.f, Math::PI2f);
				ImGui::SliderFloat("Effect Step Length", &m_ParticleUniforms.effectStepLength, 0.f, 64.f);
				ImGui::SliderFloat("Effect Turbulence", &m_ParticleUniforms.effectTurbulence, 0.f, 32.f);
				ImGui::SliderFloat("Effect Noise", &m_ParticleUniforms.effectNoise, 0.f, 4.f);
				ImGui::SliderFloat("Effect Greenstein Value", &m_ParticleUniforms.HenyeyGreensteinPhaseValue, 0.f, 1.f);
				ImGui::SliderFloat("Effect Beers Law Multiplier", &m_ParticleUniforms.BeersLawStepLengthMultiplier, 0.f, 64.f);
				ImGui::SliderFloat("Effect tileable threshhold", &m_ParticleUniforms.tileableCloudThreshold, 0.f, 1.f);
				ImGui::SliderFloat("Effect tileable tile factor", &m_ParticleUniforms.tileableCloudTilingFactor, 0.f, 64.f);
				ImGui::SliderFloat("Effect dither threshold", &m_ParticleUniforms.ditherThreshold, 0.f, 1.f);
				ImGui::SliderInt("Effect number of Steps", &m_ParticleUniforms.numberOfSteps, 1, 8);
			}
		}
		else
		{
			m_simulator.UpdateParticles(m_cloudParticles, poolOfCommands, m_pResourceManager->GetSwapChain()->GetTargetResolution(), m_cloudSdfTexture);
		}

		RenderContext* pContext = m_pRenderer->GetCurrentContext();
		pContext->StartTransferPass();

		if (m_frameRenderSettings.m_bEnableGpuParticles)
		{
			if (bRespawnParticles)
			{
				m_ParticleUniforms.numberOfParticles = 0u;

				for (int32 i = 0; i < m_numberOfClouds; i++)
				{
					m_ParticleUniforms.numberOfParticles += (m_cloudList[i].dimensions.x * m_numberOfPointsPerPixelInClouds) * (m_cloudList[i].dimensions.y * m_numberOfPointsPerPixelInClouds);
				}
			}
		}
		else
		{
			m_ParticleUniforms.numberOfParticles = m_cloudParticles.Size();
		}

		m_bRespawnParticles = bRespawnParticles;

		pContext->UploadDataToBuffer(m_pCloudListBuffer, m_cloudList.Data(), sizeof(CloudData)* m_cloudList.Size());
		pContext->UploadDataToBuffer(m_pParticleUniformBuffer, &m_ParticleUniforms, sizeof(ParticleUniformBuffer));

		pContext->EndTransferPass();

		if (m_frameRenderSettings.m_bEnableImgui)
			ImGui::End();

	}

	void CloudRenderer::Render()
	{
		RenderContext* pContext = m_pRenderer->GetCurrentContext();
		uint32 currentDoubleBufferWrite = pContext->GetCurrentRenderFrame() % 2u;
		glm::uvec2 resolution = m_pResourceManager->GetSwapChain()->GetTargetResolution();

		pContext->SetBufferAtSlot(m_pParticleUniformBuffer, 0);

		bool bSimulationPaused = m_frameRenderSettings.m_bPausedSimulation;

		if (m_frameRenderSettings.m_bEnableGpuParticles)
		{
			SortConstants sortingConstants{};
			// Will be number of Active Particles
			sortingConstants.numberOfGroups = Math::Max(m_ParticleUniforms.numberOfParticles / 255u, 1u);
			sortingConstants.totalNumberOfElements = m_ParticleUniforms.numberOfParticles;

			// global cloud data
			pContext->SetBufferAtSlot(m_pParticleBuffer, 1);
			pContext->SetBufferAtSlot(m_pParticleLookupStartIndicesBuffer, 2);
			pContext->SetBufferAtSlot(m_pParticleLookupBufferWrite, 3);
			pContext->SetBufferAtSlot(m_pDynamicParticleVariableBuffer, 4);

			if (!bSimulationPaused)
			{
				if (m_bRespawnParticles)
				{
					pContext->SetBufferAtSlot(m_pCloudListBuffer, 5);

					pContext->BindMaterial(m_pSimulationUpdateParticleListFromCPU->m_pPipeline);
					glm::uvec4 pushConstants = glm::uvec4(0);
					pushConstants.x = m_numberOfPointsPerPixelInClouds;
					pContext->SetPushConstantValue(&pushConstants);
					pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));
					m_bRespawnParticles = false;
				}

				pContext->BindMaterial(m_pSimulationUpdateUniforms->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));

				// Clearing lookup data and setting data straight
				pContext->SetBufferAtSlot(m_pParticleLookupSortIndicesBufferWrite, 5);
				pContext->BindMaterial(m_pSimulationClearPartialLookup->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));

				pContext->CopyDataToBuffer(m_pParticleLookupBufferRead, m_pParticleLookupBufferWrite);
				pContext->CopyDataToBuffer(m_pParticleLookupSortIndicesBufferRead, m_pParticleLookupSortIndicesBufferWrite);
				// Sort the data
				for (uint32 i = 0; i < NumberOfDispatches; i++)
				{
					// Build Histogram
					BufferObject* pReadBuffer = m_pParticleLookupSortIndicesBufferRead;
					BufferObject* pWriteBuffer = m_pParticleLookupSortIndicesBufferWrite;

					pContext->SetBufferAtSlot(pReadBuffer, 1);
					pContext->SetBufferAtSlot(m_pParticleSortLocalHistogramBuffer, 2);

					pContext->BindMaterial(m_pRadixBuildHistogramPipeline->m_pPipeline);

					sortingConstants.currentOffset = i * 4u; // 4 bit shift per dispatch round
					pContext->SetPushConstantValue(&sortingConstants);

					pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));

					// Build OffsetTable
					pContext->SetBufferAtSlot(m_pParticleSortLocalHistogramBuffer, 1);
					pContext->SetBufferAtSlot(m_pParticleSortGlobalHistogramBuffer, 2);
					pContext->SetBufferAtSlot(m_pParticleSortBucketOffsetBuffer, 3);

					pContext->BindMaterial(m_pRadixBuildOffsetTablePipeline->m_pPipeline);

					pContext->Dispatch(glm::uvec3(1u, 1u, 1u));

					//// Reshuffle the data
					pContext->SetBufferAtSlot(pReadBuffer, 1);
					pContext->SetBufferAtSlot(pWriteBuffer, 2);
					pContext->SetBufferAtSlot(m_pParticleSortGlobalHistogramBuffer, 3);
					pContext->SetBufferAtSlot(m_pParticleSortBucketOffsetBuffer, 4);

					pContext->SetBufferAtSlot(m_pParticleLookupBufferRead, 5);
					pContext->SetBufferAtSlot(m_pParticleLookupBufferWrite, 6);

					pContext->BindMaterial(m_pRadixShuffleDataPipeline->m_pPipeline);

					pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));

					// Copy data from Write to read for next pass
					pContext->CopyDataToBuffer(pReadBuffer, pWriteBuffer);
					pContext->CopyDataToBuffer(m_pParticleLookupBufferRead, m_pParticleLookupBufferWrite);
				}

				pContext->SetBufferAtSlot(m_pParticleUniformBuffer, 0);
				pContext->SetBufferAtSlot(m_pParticleBuffer, 1);
				pContext->SetBufferAtSlot(m_pParticleLookupStartIndicesBuffer, 2);
				pContext->SetBufferAtSlot(m_pParticleLookupBufferWrite, 3);
				pContext->SetBufferAtSlot(m_pDynamicParticleVariableBuffer, 4);

				// Build lookup table and update max velocity
				pContext->BindMaterial(m_pSimulationBuildPartialLookup->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));
				// Density
				pContext->BindMaterial(m_pSimulationCalculateDensity->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));
				// Intermediate Velocity
				pContext->SetTextureAtSlot(m_pSdfView, 5);
				pContext->SetBufferAtSlot(m_pCloudListBuffer, 6);
				pContext->BindMaterial(m_pSimulationCalculateIntermediateVelocity->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));
				// Pressure Force
				pContext->BindMaterial(m_pSimulationCalculatePressureForce->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));
				// Apply everything
				pContext->BindMaterial(m_pSimulationApplyPosition->m_pPipeline);
				pContext->Dispatch(glm::uvec3(sortingConstants.numberOfGroups, 1u, 1u));
			}

			pContext->SetTextureAtSlot(m_pParticleCoverageViewWrite[currentDoubleBufferWrite], 5);

			pContext->BindMaterial(m_pCloudCoveragePipeline->m_pPipeline);
			pContext->Dispatch(glm::uvec3(resolution.x / 64u, resolution.y / 8u, 1u));
		}

		int numberOfJfaIterations = int(log2(resolution.x)) - 1;
		uint32 currentDoubleBufferRead = (currentDoubleBufferWrite + 1) % 2u;
		while (numberOfJfaIterations != 0)
		{
			// From read to write
			pContext->SetTextureAtSlot(m_pParticleCoverageViewRead[currentDoubleBufferWrite], 0);
			pContext->SetTextureAtSlot(m_pParticleCoverageViewWrite[currentDoubleBufferRead], 1);
			pContext->BindMaterial(m_pJFAPipelines[currentDoubleBufferRead]->m_pPipeline);

			glm::uvec4 stepWidth = glm::uvec4(exp2(numberOfJfaIterations), 0, 0, 0);

			pContext->SetPushConstantValue(&stepWidth);
			pContext->Dispatch(glm::uvec3(resolution.x / 64u, resolution.y / 8u, 1u));

			currentDoubleBufferRead = (currentDoubleBufferRead + 1u) % 2u;
			currentDoubleBufferWrite = (currentDoubleBufferWrite + 1u) % 2u;

			numberOfJfaIterations--;
		}

		// From write to read, collecting JFA results
		pContext->SetTextureAtSlot(m_pParticleCoverageViewRead[currentDoubleBufferRead], 0);
		pContext->SetTextureAtSlot(m_pParticleCoverageViewWrite[currentDoubleBufferWrite], 1);
		pContext->BindMaterial(m_pCollectJFAPipeline->m_pPipeline);

		pContext->Dispatch(glm::uvec3(resolution.x / 64u, resolution.y / 8u, 1u));


		pContext->SetTextureAtSlot(m_pParticleCoverageViewRead[currentDoubleBufferWrite], 1);
		pContext->SetTextureAtSlot(m_pTileableCloudTextureView, 2);

		pContext->BindMaterial(m_pCloudPipeline->m_pPipeline);

		pContext->RenderFullscreenPass();
	}
}