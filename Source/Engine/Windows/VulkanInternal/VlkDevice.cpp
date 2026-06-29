#include "Engine_PCH.h"
#include "VlkDevice.h"
#include "Utilities.h"


#include <array>
#include <set>
#include <stdio.h>

#include "MathUtils.h"

#include "DebugMacros.h"

#include "../Windows_ApplicationWindow.h"
#include "HailEngine.h"
#include "Resources\TextureManager.h"

#include "Resources\Vertices.h"
#include "Timer.h"
#include "Resources\ResourceManager.h"

#include "VlkVertex_Descriptor.h"
#include "VlkVmaUsage.h"

#include "vk_mem_alloc.h"

/* Put this in a single .cpp file that's vulkan related: */
PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT_ = nullptr;

using namespace Hail;

constexpr uint32_t DEVICEEXTENSIONCOUNT = 7;
// VK_KHR_SPIRV_1_4_EXTENSION_NAME, "VK_KHR_shader_float_controls" are both required by the mesh shader extension
const char* deviceExtensions[DEVICEEXTENSIONCOUNT] = { 
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
	, VK_EXT_MESH_SHADER_EXTENSION_NAME
	, VK_KHR_SPIRV_1_4_EXTENSION_NAME
	, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
	, VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
	, VK_KHR_MAINTENANCE_4_EXTENSION_NAME
	, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME
};


#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
constexpr uint32_t REQUIREDEXTENSIONCOUNT = 2;
const char* requiredExtensions[REQUIREDEXTENSIONCOUNT] = {
	"VK_KHR_surface",
	"VK_KHR_win32_surface"
};

#else
#include <iostream> 

constexpr bool enableValidationLayers = true;
constexpr uint32_t REQUIREDEXTENSIONCOUNT = 3;
const char* requiredExtensions[REQUIREDEXTENSIONCOUNT] = { 
	"VK_KHR_surface", 
	"VK_KHR_win32_surface",
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};

constexpr uint32_t VALIDATIONLAYERCOUNT = 1;
const char* validationLayers[VALIDATIONLAYERCOUNT] = { "VK_LAYER_KHRONOS_validation" };



static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	//std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) 
	{
		// Message is important enough to show
		H_ERROR(pCallbackData->pMessage);
	}

	return VK_FALSE;
}


void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) 
	{
		func(instance, debugMessenger, pAllocator);
	}
}

#endif


void VlkDevice::DestroyDevice()
{
	vmaDestroyAllocator(m_vmaAllocator);

	vkDestroyDevice(m_device, nullptr);
#ifdef DEBUG
	if (enableValidationLayers) {
		DestroyDebugUtilsMessengerEXT(m_vkInstance, m_debugMessenger, nullptr);
	}
#endif

	vkDestroySurfaceKHR(m_vkInstance, m_surface, nullptr);
	vkDestroyInstance(m_vkInstance, nullptr);
}


void VlkDevice::CreateInstance(ErrorManager* pErrorManager)
{
	if (enableValidationLayers && !CheckValidationLayerSupport()) {
		pErrorManager->AddErrors(EStartupErrors::InitGpuDevice, EErrorType::Startup);
		pErrorManager->AddString("validation layers requested, but not available!");
		return;
	}

	if (!CheckRequiredExtensions(pErrorManager))
	{
		pErrorManager->AddErrors(EStartupErrors::InitGpuDevice, EErrorType::Startup);
		pErrorManager->AddString("Rendering Device, missing required extensions: \n");
		return;
	}
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hail";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.pEngineName = "Hail Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = REQUIREDEXTENSIONCOUNT;
	createInfo.ppEnabledExtensionNames = requiredExtensions;
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
#ifdef DEBUG
		createInfo.enabledLayerCount = VALIDATIONLAYERCOUNT;
		createInfo.ppEnabledLayerNames = validationLayers;
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#endif
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_vkInstance);

    if (result != VK_SUCCESS) 
    { 
		pErrorManager->AddErrors(EStartupErrors::InitGpuDevice, EErrorType::Startup);
		pErrorManager->AddString("Rendering Device, Vulkan vkCreateInstanceFailed.");
		return;
    }	
	SetupDebugMessenger();
	CreateWindowsSurface();
	PickPhysicalDevice(pErrorManager);
	CreateLogicalDevice();

	vkCmdDrawMeshTasksEXT_ = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(m_device, "vkCmdDrawMeshTasksEXT");

	Hail::QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);
	vkGetDeviceQueue(m_device, indices.graphicsAndComputeFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.graphicsAndComputeFamily, 0, &m_computeQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily, 0, &m_presentQueue);
	CreateCommandPool(pErrorManager);
}

bool VlkDevice::CheckValidationLayerSupport()
{
#ifdef DEBUG
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	VkLayerProperties* availableLayers = new VkLayerProperties[layerCount];
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	uint32_t layersFound = 0;
	//Debug_PrintConsoleConstChar("Available validation layers:");
	for (uint32_t foundLayer = 0; foundLayer < layerCount; foundLayer++)
	{
		for (uint32_t validationLayer = 0; validationLayer < VALIDATIONLAYERCOUNT; validationLayer++) {
			if (strcmp(availableLayers[foundLayer].layerName, availableLayers[validationLayer].layerName) == 0) {
				layersFound++;
				break;
			}
		}
		//Debug_PrintConsoleStringL(StringL::Format("\t%s", availableLayers[foundLayer].layerName));
	}

	if (layersFound != VALIDATIONLAYERCOUNT) {
		delete[] availableLayers;
		return false;
	}

	delete[] availableLayers;
#endif
	return true;

}

bool VlkDevice::CheckRequiredExtensions(ErrorManager* pErrorManager)
{
	uint32_t extensionCount = 0; //query extension counts on the system
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	GrowingArray<VkExtensionProperties> extensions = GrowingArray<VkExtensionProperties>(extensionCount);
	extensions.Fill();
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.Data());

	uint32_t foundCounter = 0;

	//Debug_PrintConsoleConstChar("Available vKInstance extensions:");
	for (uint32_t reqExtension = 0; reqExtension < REQUIREDEXTENSIONCOUNT; reqExtension++)
	{
		bool bFoundExtension = false;
		for (uint32_t extension = 0; extension < extensionCount; extension++)
		{
			if (strcmp(extensions[extension].extensionName, requiredExtensions[reqExtension]) == 0)
			{
				bFoundExtension = true;
				foundCounter++;
				break;
			}
		}

		if (!bFoundExtension)
		{
			pErrorManager->AddString(StringL::Format("\tGPU Device does not have the extension: %s", requiredExtensions[reqExtension]));
		}
	}
	if (foundCounter != REQUIREDEXTENSIONCOUNT)
	{
		return false;
	}
	return true;
}

void VlkDevice::SetupDebugMessenger()
{
	if (!enableValidationLayers)
	{
		return;
	}
#ifdef DEBUG
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(m_vkInstance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}

#endif
}

void VlkDevice::PickPhysicalDevice(ErrorManager* pErrorManager)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
	if (deviceCount == 0) 
	{
		pErrorManager->AddString("\tNo GPU device that supports Vulkan");
	}
	GrowingArray<VkPhysicalDevice> devices =GrowingArray<VkPhysicalDevice>(deviceCount);
	devices.Fill();
	vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices.Data());

	for (uint32_t device = 0; device < deviceCount; device++)
	{
		if (IsDeviceSuitable(devices[device])) 
		{
			m_physicalDevice = devices[device];

			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(devices[device], &deviceProperties);
			pErrorManager->AddString(StringL::Format("\tPicked GPU, name: %s\n", deviceProperties.deviceName));
			m_deviceLimits.m_maxComputeWorkGroupInvocations = deviceProperties.limits.maxComputeWorkGroupInvocations;
			m_deviceLimits.m_maxComputeSharedMemorySize = deviceProperties.limits.maxComputeSharedMemorySize;
			break;
		}
		else
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(devices[device], &deviceProperties);
			pErrorManager->AddString(StringL::Format("\tGPU was not suitable, name: %s\n", deviceProperties.deviceName));
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE) 
	{
		pErrorManager->AddString("\tFailed to find suitable GPU");
	}
}

bool VlkDevice::IsDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = FindQueueFamilies(device);
	bool extensionsSupported = CheckDeviceExtensionSupport(device);
	bool swapChainAdequate = false;

	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.Empty() && !swapChainSupport.presentModes.Empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.IsComplete() && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VlkDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	GrowingArray<VkExtensionProperties> availableExtensions = GrowingArray < VkExtensionProperties>(extensionCount);
	availableExtensions.Fill();
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.Data());

	uint32_t foundCounter = 0;
	//Debug_PrintConsoleConstChar("Available device extensions:");
	for (uint32_t extension = 0; extension < extensionCount; extension++)
	{
		for (uint32_t reqExtension = 0; reqExtension < DEVICEEXTENSIONCOUNT; reqExtension++)
		{
			if (strcmp(deviceExtensions[reqExtension], availableExtensions[extension].extensionName) == 0)
			{
				foundCounter++;
				break;
			}
		}
		//Debug_PrintConsoleStringL(StringL::Format("\t%s", availableExtensions[extension].extensionName));
	}
	if (foundCounter != DEVICEEXTENSIONCOUNT)
	{
		return false;
	}
	return true;
}

QueueFamilyIndices VlkDevice::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	VkQueueFamilyProperties* queueFamilies = new VkQueueFamilyProperties[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

	for (uint32_t queues = 0; queues < queueFamilyCount; queues++)
	{
		if ((queueFamilies[queues].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamilies[queues].queueFlags & VK_QUEUE_COMPUTE_BIT))
		{
			indices.graphicsAndComputeFamily = queues;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, queues, m_surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = queues;
		}
		if (indices.IsComplete()) {
			break;
		}
	}
	delete[] queueFamilies;
	return indices;
}

void VlkDevice::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

	GrowingArray<VkDeviceQueueCreateInfo, uint32_t> queueCreateInfos = GrowingArray<VkDeviceQueueCreateInfo, uint32_t>(2);
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsAndComputeFamily, indices.presentFamily };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.Add(queueCreateInfo);
	}


	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	//TODO: Add an If debug
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.wideLines = VK_TRUE;
	
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.Data();
	createInfo.queueCreateInfoCount = queueCreateInfos.Size();
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = DEVICEEXTENSIONCOUNT;
	createInfo.ppEnabledExtensionNames = deviceExtensions;

	// Adding mesh shader extension to our device.
	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderExtension{};
	meshShaderExtension.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	meshShaderExtension.taskShader = VK_TRUE;
	meshShaderExtension.meshShader = VK_TRUE;
	meshShaderExtension.primitiveFragmentShadingRateMeshShader = VK_TRUE;
	createInfo.pNext = (void*)&meshShaderExtension;

	//VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProperties{};
	//meshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
	//meshShaderProperties.prefersCompactPrimitiveOutput = false;
	//meshShaderExtension.pNext = &meshShaderProperties;


	// Barycentric coords
	VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR requested_fragment_shader_barycentric_features{};
	requested_fragment_shader_barycentric_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;
	requested_fragment_shader_barycentric_features.fragmentShaderBarycentric = VK_TRUE;
	meshShaderExtension.pNext = &requested_fragment_shader_barycentric_features;

	VkPhysicalDeviceMaintenance4Features maintenance4Features{};
	maintenance4Features.maintenance4 = VK_TRUE;
	maintenance4Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR;
	requested_fragment_shader_barycentric_features.pNext = &maintenance4Features;

	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT floatFeatures{};
	floatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	floatFeatures.shaderBufferFloat32AtomicAdd = true;
	floatFeatures.shaderBufferFloat32Atomics = true;
	maintenance4Features.pNext = &floatFeatures;

	floatFeatures.pNext = nullptr;


	if (enableValidationLayers) 
	{
#ifdef DEBUG
		createInfo.enabledLayerCount = VALIDATIONLAYERCOUNT;
		createInfo.ppEnabledLayerNames = validationLayers;
#endif
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) 
	{
#ifdef DEBUG
		throw std::runtime_error("failed to create logical device!");
#endif
	}

	VmaAllocatorCreateInfo vlkVmaCreateInfo{};

	vlkVmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	vlkVmaCreateInfo.device = m_device;
	vlkVmaCreateInfo.physicalDevice = m_physicalDevice;
	vlkVmaCreateInfo.preferredLargeHeapBlockSize = 0u;
	vlkVmaCreateInfo.instance = m_vkInstance;

	if (vmaCreateAllocator(&vlkVmaCreateInfo, &m_vmaAllocator) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create memory allocator.");
	}
}

void VlkDevice::CreateWindowsSurface()
{
	VkWin32SurfaceCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	Windows_ApplicationWindow* appWindow = reinterpret_cast<Windows_ApplicationWindow*> (Hail::GetApplicationWIndow());

	if (appWindow)
	{
		createInfo.hwnd = appWindow->GetWindowHandle();
		createInfo.hinstance = appWindow->GetAppModuleHandle();
	}

	if (vkCreateWin32SurfaceKHR(m_vkInstance, &createInfo, nullptr, &m_surface) != VK_SUCCESS)
	{
#ifdef DEBUG
		throw std::runtime_error("failed to create window surface!");
#endif
	}
}

void Hail::VlkDevice::CreateCommandPool(ErrorManager* pErrorManager)
{
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily;

	for (uint32 i = 0; i < MAX_FRAMESINFLIGHT; i++)
	{
		if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPools[i]) != VK_SUCCESS)
		{
			pErrorManager->AddErrors(EStartupErrors::InitGpuDevice, EErrorType::Startup);
			pErrorManager->AddString("\tFailed to create command pools");
		}
	}
}

SwapChainSupportDetails VlkDevice::QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.Prepare(formatCount);
		details.formats.Fill();
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.Data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.Prepare(presentModeCount);
		details.presentModes.Fill();
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.Data());
	}

	return details;
}