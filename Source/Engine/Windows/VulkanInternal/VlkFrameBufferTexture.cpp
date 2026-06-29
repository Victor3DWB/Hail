#include "Engine_PCH.h"

#include "VlkFrameBufferTexture.h"
#include "VlkDevice.h"
#include "Resources\Vulkan\VlkTextureResource.h"
#include "Containers\VectorOnStack\VectorOnStack.h"
using namespace Hail;

VlkFrameBufferTexture::VlkFrameBufferTexture(glm::uvec2 resolution, eTextureFormat format, TEXTURE_DEPTH_FORMAT depthFormat) :
	FrameBufferTexture(resolution, format, depthFormat)
{
	//memset(m_frameBuffers, 0, sizeof(VkFramebuffer) * MAX_FRAMESINFLIGHT);
	m_renderPass = VK_NULL_HANDLE;
	m_frameBuffers.Fill(VK_NULL_HANDLE);
}

void VlkFrameBufferTexture::CreateFrameBufferTextureObjects(RenderingDevice* pDevice)
{
	VlkDevice* vlkDevice = (VlkDevice*)pDevice;

	uint32_t attachmentCount = 0;
	attachmentCount += m_textureFormat != eTextureFormat::UNDEFINED ? 1 : 0;
	attachmentCount += m_depthFormat != TEXTURE_DEPTH_FORMAT::UNDEFINED ? 1 : 0;

	//TODO: assert if both are undefined
	if (m_textureFormat != eTextureFormat::UNDEFINED && m_depthFormat != TEXTURE_DEPTH_FORMAT::UNDEFINED )
	{
		CreateTextureResources(false, vlkDevice);
		CreateTextureResources(true, vlkDevice);
	}
	else if(m_textureFormat != eTextureFormat::UNDEFINED)
	{
		CreateTextureResources(true, vlkDevice);
	}
	else if(m_depthFormat != TEXTURE_DEPTH_FORMAT::UNDEFINED)
	{
		CreateTextureResources(false, vlkDevice);
	}

	H_ASSERT(CreateRenderpass(pDevice));
	for (uint32 i = 0; i < MAX_FRAMESINFLIGHT; ++i)
		H_ASSERT(CreateFramebuffers(pDevice, i));
}

void VlkFrameBufferTexture::ClearResources(RenderingDevice* pDevice, bool isSwapchain)
{
	VlkDevice& vlkDevice = *(VlkDevice*)pDevice;
	for (size_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
	{
		if (m_frameBuffers[i] != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(vlkDevice.GetDevice(), m_frameBuffers[i], nullptr);
			m_frameBuffers[i] = VK_NULL_HANDLE;
		}
	}
	if (m_renderPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(vlkDevice.GetDevice(), m_renderPass, nullptr);
	m_renderPass = VK_NULL_HANDLE;
	FrameBufferTexture::ClearResources(pDevice, isSwapchain);
}

void VlkFrameBufferTexture::CreateTextureResources(bool bIsColorTexture, RenderingDevice* pDevice)
{
	TextureProperties props{};
	props.width = m_resolution.x;
	props.height = m_resolution.y;
	props.format = m_textureFormat;
	VlkDevice* pVlkDevice = (VlkDevice*)pDevice;

	for (size_t i = 0; i < MAX_FRAMESINFLIGHT; i++)
	{
		VlkTextureResource* vlkTextureResource = new VlkTextureResource();
		vlkTextureResource->textureName = m_bufferName.Data();
		if (bIsColorTexture)
		{
			props.depthFormat = TEXTURE_DEPTH_FORMAT::UNDEFINED;
			props.textureUsage = eTextureUsage::FramebufferColor;
		}
		else
		{
			props.depthFormat = m_depthFormat;
			props.textureUsage = eTextureUsage::FramebufferDepthOnly;
		}
		vlkTextureResource->m_properties = props;
		vlkTextureResource->m_index = MAX_UINT - 2;
		H_ASSERT(vlkTextureResource->Init(pDevice), "Failed creating frame buffer texture");

		VlkTextureView* pVlkTextureView = new VlkTextureView();
		TextureViewProperties viewProps{};
		viewProps.viewUsage = props.textureUsage;
		viewProps.pTextureToView = vlkTextureResource;
		viewProps.accessQualifier = eShaderAccessQualifier::ReadOnly;
		H_ASSERT(pVlkTextureView->InitView(pDevice, viewProps));

		if (bIsColorTexture)
		{
			m_pTextureResource[i] = vlkTextureResource;
			m_pTextureViews[i] = pVlkTextureView;
		}
		else
		{
			m_pDepthTextureResource[i] = vlkTextureResource;
			m_pDepthTextureViews[i] = pVlkTextureView;
		}
	}
}

bool Hail::VlkFrameBufferTexture::CreateFramebuffers(RenderingDevice* pDevice, uint32 frameInFlight)
{
	VlkDevice& device = *(VlkDevice*)(pDevice);
	VkImageView colorTexture = ((VlkTextureView*)GetColorTextureView(frameInFlight))->GetVkImageView();
	VectorOnStack<VkImageView, 2> attachments;
	attachments.Add(colorTexture);
	if (HasDepthAttachment())
	{
		VkImageView depthTexture = ((VlkTextureView*)GetDepthTextureView(frameInFlight))->GetVkImageView();
		attachments.Add(depthTexture);
	}
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = m_renderPass;
	framebufferInfo.attachmentCount = attachments.Size();
	framebufferInfo.pAttachments = attachments.Data();
	framebufferInfo.width = m_resolution.x;
	framebufferInfo.height = m_resolution.y;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(device.GetDevice(), &framebufferInfo, nullptr, &m_frameBuffers[frameInFlight]) != VK_SUCCESS)
		return false;
	return true;
}

bool VlkFrameBufferTexture::CreateRenderpass(RenderingDevice* pDevice)
{
	GrowingArray<VkAttachmentDescription> attachmentDescriptors(2);
	VkAttachmentReference colorAttachmentRef{};
	VkAttachmentReference depthAttachmentRef{};

	VkAttachmentDescription colorAttachment{};
	VkAttachmentDescription depthAttachment{};

	colorAttachment.format = ToVkFormat(GetTextureFormat());
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentDescriptors.Add(colorAttachment);

	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	if (HasDepthAttachment())
	{
		depthAttachment.format = ToVkFormat(GetDepthFormat());
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachmentDescriptors.Add(depthAttachment);
	}

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;


	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachmentDescriptors.Size();
	renderPassInfo.pAttachments = attachmentDescriptors.Data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VlkDevice& device = *(VlkDevice*)pDevice;
	if (vkCreateRenderPass(device.GetDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
		return false;

	return true;
}