#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <tuple>
#include <functional>

#include "vkUniqueObjects.h"
#include "binaryLoader.h"

#pragma comment(lib, "vulkan-1")

std::function<void()> g_RenderFunc;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY: PostQuitMessage(0); break;
	case WM_PAINT:
		BeginPaint(hWnd, nullptr); EndPaint(hWnd, nullptr);
		g_RenderFunc();
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

auto initApp(HINSTANCE hInstance)
{
	WNDCLASSEX wce{};

	wce.cbSize = sizeof wce;
	wce.hInstance = hInstance;
	wce.lpszClassName = L"com.cterm2.vkTest.AppFrame";
	wce.lpfnWndProc = &WndProc;
	wce.style = CS_OWNDC;
	wce.hCursor = LoadCursor(nullptr, IDC_ARROW);
	if (!RegisterClassEx(&wce)) return (HWND)nullptr;

	RECT rc;
	SetRect(&rc, 0, 0, 640, 480);
	AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, false, 0);
	return CreateWindowEx(0, wce.lpszClassName, L"vkTest", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
}

struct VertexData
{
	float pos[2];
	float color[4];
};

// Debug Layer Extensions
PFN_vkCreateDebugReportCallbackEXT	_vkCreateDebugReportCallbackEXT;
PFN_vkDebugReportMessageEXT			_vkDebugReportMessageEXT;
PFN_vkDestroyDebugReportCallbackEXT	_vkDestroyDebugReportCallbackEXT;

namespace Vulkan
{
	void checkError(VkResult res) { if (res != VK_SUCCESS) throw std::runtime_error(std::to_string(res).c_str()); }

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT, uint64_t object,
		size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
	{
		// OutputDebugString(L"Message Code: "); OutputDebugString(std::to_wstring(messageCode).c_str()); OutputDebugString(L"\n");
		OutputDebugString(L"Vulkan DebugCall: "); OutputDebugStringA(pMessage); OutputDebugString(L"\n");
		return VK_FALSE;
	}

	auto createInstance()
	{
		VkInstanceCreateInfo instanceInfo{};
		VkApplicationInfo appInfo{};
		const char* extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_report" };
		const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };

		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
		appInfo.pApplicationName = "com.cterm2.vkTest";
		appInfo.apiVersion = VK_API_VERSION;
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo = &appInfo;
		instanceInfo.enabledExtensionCount = std::size(extensions);
		instanceInfo.ppEnabledExtensionNames = extensions;
		instanceInfo.enabledLayerCount = std::size(layers);
		instanceInfo.ppEnabledLayerNames = layers;
		
		VkInstance instance;
		auto res = vkCreateInstance(&instanceInfo, nullptr, &instance);
		checkError(res);

		// load extensions
		_vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
		_vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));
		_vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));

		return Instance(instance, &vkDestroyInstance);
	}
	auto createDebugReportCallback(const Instance& instance)
	{
		VkDebugReportCallbackCreateInfoEXT callbackInfo{};
		
		callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		callbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
		callbackInfo.pfnCallback = &debugCallback;
		
		VkDebugReportCallbackEXT callback;
		auto res = _vkCreateDebugReportCallbackEXT(instance.get(), &callbackInfo, nullptr, &callback);
		return UniqueObjectWithInstance<VkDebugReportCallbackEXT>(instance.get(), callback, _vkDestroyDebugReportCallbackEXT);
	}
	auto createSurfaceForHwnd(const Instance& instance, HWND hWnd)
	{
		VkWin32SurfaceCreateInfoKHR surfaceInfo{};

		surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.hinstance = GetModuleHandle(nullptr);
		surfaceInfo.hwnd = hWnd;

		VkSurfaceKHR surface;
		auto res = vkCreateWin32SurfaceKHR(instance.get(), &surfaceInfo, nullptr, &surface);
		checkError(res);
		return Surface(instance.get(), surface, &vkDestroySurfaceKHR);
	}
	auto enumerateAndGetDefaultPhysicalDevice(const Instance& instance)
	{
		uint32_t adapterCount;
		auto res = vkEnumeratePhysicalDevices(instance.get(), &adapterCount, nullptr);
		checkError(res);
		auto adapters = std::make_unique<VkPhysicalDevice[]>(adapterCount);
		res = vkEnumeratePhysicalDevices(instance.get(), &adapterCount, adapters.get());
		checkError(res);
		OutputDebugString(L"=== Physical Device Enumeration ===\n");
		for (uint32_t i = 0; i < adapterCount; i++)
		{
			static VkPhysicalDeviceProperties props;
			static VkPhysicalDeviceMemoryProperties memProps;
			vkGetPhysicalDeviceProperties(adapters[i], &props);
			vkGetPhysicalDeviceMemoryProperties(adapters[i], &memProps);

			OutputDebugString(L"#"); OutputDebugString(std::to_wstring(i).c_str()); OutputDebugString(L": \n");
			OutputDebugString(L"  Name: "); OutputDebugStringA(props.deviceName); OutputDebugString(L"\n");
			OutputDebugString(L"  API Version: "); OutputDebugString(std::to_wstring(props.apiVersion).c_str()); OutputDebugString(L"\n");
		}
		return adapters[0];
	}

	// Unique Arrays and Paired Structures
	using ImageArray = UniqueArray<VkImage>;
	using ImageViewArray = UniqueArray<ImageView>;
	using FramebufferArray = UniqueArray<Framebuffer>;
	using BufferData = std::pair<Buffer, DeviceMemory>;

	// Logical Device for Graphics
	class Device final
	{
		VkPhysicalDevice pDevRef;
		UniqueObject<VkDevice> pInternal;
		VkQueue devQueue;
		uint32_t queueFamilyIndex;
		VkPhysicalDeviceMemoryProperties memProps;

		Device(VkPhysicalDevice pd, VkDevice p, uint32_t qfi)
			: pDevRef(pd), pInternal(p, &vkDestroyDevice), queueFamilyIndex(qfi)
		{
			vkGetDeviceQueue(p, queueFamilyIndex, 0, &devQueue);
			vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
		}
	public:
		static auto create(VkPhysicalDevice pDev)
		{
			VkDeviceCreateInfo devInfo{};
			VkDeviceQueueCreateInfo queueInfo{};

			// Search queue family index for Graphics Queue
			uint32_t propertyCount, queueFamilyIndex = 0xffffffff;
			vkGetPhysicalDeviceQueueFamilyProperties(pDev, &propertyCount, nullptr);
			auto properties = std::make_unique<VkQueueFamilyProperties[]>(propertyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(pDev, &propertyCount, properties.get());
			for (uint32_t i = 0; i < propertyCount; i++)
			{
				if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
				{
					queueFamilyIndex = i;
					break;
				}
			}
			if (queueFamilyIndex == 0xffffffff) throw std::runtime_error("No Graphics queues available on current device.");

			const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
			const char* extensions[] = { "VK_KHR_swapchain" };
			static float qPriorities[] = { 0.0f };
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueCount = 1;
			queueInfo.queueFamilyIndex = queueFamilyIndex;
			queueInfo.pQueuePriorities = qPriorities;
			devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			devInfo.queueCreateInfoCount = 1;
			devInfo.pQueueCreateInfos = &queueInfo;
			devInfo.enabledLayerCount = std::size(layers);
			devInfo.ppEnabledLayerNames = layers;
			devInfo.enabledExtensionCount = std::size(extensions);
			devInfo.ppEnabledExtensionNames = extensions;

			VkDevice device;
			auto res = vkCreateDevice(pDev, &devInfo, nullptr, &device);
			checkError(res);
			return Device(pDev, device, queueFamilyIndex);
		}

		// Derived from this
		auto createCommandPool()
		{
			VkCommandPoolCreateInfo info{};

			info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			info.queueFamilyIndex = this->queueFamilyIndex;
			info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

			VkCommandPool object;
			auto res = vkCreateCommandPool(this->pInternal.get(), &info, nullptr, &object);
			checkError(res);
			return CommandPool(this->pInternal.get(), object, &vkDestroyCommandPool);
		}
		auto createSwapchain(const Surface& surface)
		{
			VkSwapchainCreateInfoKHR scinfo{};

			VkBool32 surfaceSupported;
			vkGetPhysicalDeviceSurfaceSupportKHR(this->pDevRef, this->queueFamilyIndex, surface.get(), &surfaceSupported);
			VkSurfaceCapabilitiesKHR surfaceCaps;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->pDevRef, surface.get(), &surfaceCaps);
			uint32_t surfaceFormatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->pDevRef, surface.get(), &surfaceFormatCount, nullptr);
			auto surfaceFormats = std::make_unique<VkSurfaceFormatKHR[]>(surfaceFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->pDevRef, surface.get(), &surfaceFormatCount, surfaceFormats.get());
			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(this->pDevRef, surface.get(), &presentModeCount, nullptr);
			auto presentModes = std::make_unique<VkPresentModeKHR[]>(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(this->pDevRef, surface.get(), &presentModeCount, presentModes.get());

			for (uint32_t i = 0; i < surfaceFormatCount; i++)
			{
				auto c = surfaceFormats[i];
				OutputDebugString(L"Supported Format Check...");
			}

			scinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			scinfo.surface = surface.get();
			scinfo.minImageCount = 2;
			scinfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
			scinfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
			scinfo.imageExtent.width = 640;
			scinfo.imageExtent.height = 480;
			scinfo.imageArrayLayers = 1;
			scinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			scinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			scinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			scinfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
			scinfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
			scinfo.clipped = VK_TRUE;

			VkSwapchainKHR object;
			auto res = vkCreateSwapchainKHR(this->pInternal.get(), &scinfo, nullptr, &object);
			checkError(res);
			return Swapchain(this->pInternal.get(), object, &vkDestroySwapchainKHR);
		}
		auto retrieveImagesFromSwapchain(const Swapchain& chain)
		{
			uint32_t imageCount;
			auto res = vkGetSwapchainImagesKHR(this->pInternal.get(), chain.get(), &imageCount, nullptr);
			checkError(res);
			auto images = std::make_unique<VkImage[]>(imageCount);
			res = vkGetSwapchainImagesKHR(this->pInternal.get(), chain.get(), &imageCount, images.get());
			checkError(res);
			return ImageArray(std::move(images), imageCount);
		}
		auto createImageViews(const ImageArray& images)
		{
			auto views = std::make_unique<ImageView[]>(size(images));

			for (uint32_t i = 0; i < size(images); i++)
			{
				VkImageViewCreateInfo vinfo{};
				vinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				vinfo.image = images.first[i];
				vinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				vinfo.format = VK_FORMAT_B8G8R8A8_UNORM;
				vinfo.components = {
					VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A
				};
				vinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				VkImageView view;
				auto res = vkCreateImageView(this->pInternal.get(), &vinfo, nullptr, &view);
				checkError(res);
				views[i] = ImageView(this->pInternal.get(), view, &vkDestroyImageView);
			}
			return ImageViewArray(std::move(views), size(images));
		}
		auto createCommonRenderPass()
		{
			VkAttachmentDescription attachmentDesc{};
			VkAttachmentReference attachmentRef{};

			attachmentDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
			attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			attachmentRef.attachment = 0;
			attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			VkRenderPassCreateInfo renderPassInfo{};

			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &attachmentRef;
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &attachmentDesc;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;

			VkRenderPass object;
			auto res = vkCreateRenderPass(this->pInternal.get(), &renderPassInfo, nullptr, &object);
			checkError(res);
			return RenderPass(this->pInternal.get(), object, &vkDestroyRenderPass);
		}
		auto createFramebuffers(const RenderPass& renderPass, const ImageViewArray& imageViews)
		{
			auto buffers = std::make_unique<Framebuffer[]>(size(imageViews));

			// Common Properties
			VkFramebufferCreateInfo fbinfo{};
			VkImageView attachmentViews[1];

			fbinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbinfo.attachmentCount = 1;
			fbinfo.renderPass = renderPass.get();
			fbinfo.pAttachments = attachmentViews;
			fbinfo.width = 640;
			fbinfo.height = 480;
			fbinfo.layers = 1;

			for (uint32_t i = 0; i < size(imageViews); i++)
			{
				attachmentViews[0] = imageViews.first[i].get();

				VkFramebuffer fb;
				auto res = vkCreateFramebuffer(this->pInternal.get(), &fbinfo, nullptr, &fb);
				checkError(res);
				buffers[i] = Framebuffer(this->pInternal.get(), fb, &vkDestroyFramebuffer);
			}
			return FramebufferArray(std::move(buffers), size(imageViews));
		}

		// Buffer Resources
		template<size_t nElements>
		auto createVertexBuffer(const VertexData(&data)[nElements])
		{
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			bufferInfo.size = sizeof(VertexData) * nElements;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkBuffer buffer;
			auto res = vkCreateBuffer(this->pInternal.get(), &bufferInfo, nullptr, &buffer);
			checkError(res);

			// Memory Allocation
			VkMemoryRequirements memreq;
			VkMemoryAllocateInfo allocInfo{};
			vkGetBufferMemoryRequirements(this->pInternal.get(), buffer, &memreq);
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memreq.size;
			allocInfo.memoryTypeIndex = UINT32_MAX;
			// Search memory index can be visible from host
			for (size_t i = 0; i < this->memProps.memoryTypeCount; i++)
			{
				if ((this->memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
				{
					allocInfo.memoryTypeIndex = i;
					break;
				}
			}
			if (allocInfo.memoryTypeIndex == UINT32_MAX) throw std::runtime_error("No found available heap.");

			VkDeviceMemory mem;
			res = vkAllocateMemory(this->pInternal.get(), &allocInfo, nullptr, &mem);
			checkError(res);

			// Set data
			uint8_t* pData;
			res = vkMapMemory(this->pInternal.get(), mem, 0, sizeof(VertexData) * nElements, 0, reinterpret_cast<void**>(&pData));
			checkError(res);
			memcpy(pData, data, sizeof(VertexData) * nElements);
			vkUnmapMemory(this->pInternal.get(), mem);

			// Associate memory to buffer
			res = vkBindBufferMemory(this->pInternal.get(), buffer, mem, 0);
			checkError(res);

			return BufferData(Buffer(this->pInternal.get(), buffer, &vkDestroyBuffer), DeviceMemory(this->pInternal.get(), mem, &vkFreeMemory));
		}
		auto createShaderModule(const std::wstring& path)
		{
			const auto bin = BinaryLoader::load(path);
			VkShaderModuleCreateInfo shaderInfo{};

			shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderInfo.codeSize = bin.second;
			shaderInfo.pCode = reinterpret_cast<uint32_t*>(bin.first.get());

			VkShaderModule mod;
			auto res = vkCreateShaderModule(this->pInternal.get(), &shaderInfo, nullptr, &mod);
			checkError(res);
			return ShaderModule(this->pInternal.get(), mod, &vkDestroyShaderModule);
		}
		auto createPipelineLayout()
		{
			VkPipelineLayoutCreateInfo pLayoutInfo{};

			pLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

			VkPipelineLayout pLayout;
			auto res = vkCreatePipelineLayout(this->pInternal.get(), &pLayoutInfo, nullptr, &pLayout);
			checkError(res);
			return PipelineLayout(this->pInternal.get(), pLayout, &vkDestroyPipelineLayout);
		}
		auto createPipelineCache()
		{
			VkPipelineCacheCreateInfo cacheInfo{};

			cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

			VkPipelineCache cache;
			auto res = vkCreatePipelineCache(this->pInternal.get(), &cacheInfo, nullptr, &cache);
			checkError(res);
			return PipelineCache(this->pInternal.get(), cache, &vkDestroyPipelineCache);
		}
		template<size_t nAttrElements>
		auto createGraphicsPipelineVF(
			const ShaderModule& vshader, const ShaderModule& fshader,
			const VkVertexInputBindingDescription& bindDesc, const VkVertexInputAttributeDescription(&attrDescs)[nAttrElements],
			const PipelineLayout& pLayout, const RenderPass& renderPass, const PipelineCache& pCache)
		{
			VkPipelineShaderStageCreateInfo stageInfo[2]{};
			VkPipelineVertexInputStateCreateInfo vinStateInfo{};
			VkPipelineInputAssemblyStateCreateInfo iaInfo{};
			VkPipelineViewportStateCreateInfo vpInfo{};
			VkPipelineRasterizationStateCreateInfo rasterizerStateInfo{};
			VkPipelineMultisampleStateCreateInfo msInfo{};
			VkPipelineColorBlendAttachmentState blendState{};
			VkPipelineColorBlendStateCreateInfo blendInfo{};
			VkPipelineDynamicStateCreateInfo dynamicInfo{};
			VkGraphicsPipelineCreateInfo gpInfo{};

			static VkViewport vports[] = { { 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f } };
			static VkRect2D scissors[] = { { { 0, 0 }, { 640, 480 } } };
			static VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

			stageInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			stageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			stageInfo[0].module = vshader.get();
			stageInfo[1].module = fshader.get();
			stageInfo[0].pName = "main";
			stageInfo[1].pName = "main";
			vinStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vinStateInfo.vertexBindingDescriptionCount = 1;
			vinStateInfo.pVertexBindingDescriptions = &bindDesc;
			vinStateInfo.vertexAttributeDescriptionCount = nAttrElements;
			vinStateInfo.pVertexAttributeDescriptions = attrDescs;
			iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			vpInfo.viewportCount = 1;
			vpInfo.pViewports = vports;
			vpInfo.scissorCount = 1;
			vpInfo.pScissors = scissors;
			rasterizerStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizerStateInfo.depthClampEnable = VK_FALSE;
			rasterizerStateInfo.rasterizerDiscardEnable = VK_FALSE;
			rasterizerStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizerStateInfo.cullMode = VK_CULL_MODE_NONE;
			rasterizerStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizerStateInfo.depthBiasEnable = VK_FALSE;
			rasterizerStateInfo.lineWidth = 1.0f;
			msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			msInfo.sampleShadingEnable = VK_FALSE;
			msInfo.alphaToCoverageEnable = VK_FALSE;
			msInfo.alphaToOneEnable = VK_FALSE;
			blendState.blendEnable = VK_FALSE;
			blendState.colorWriteMask = VK_COLOR_COMPONENT_A_BIT
				| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT;
			blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blendInfo.logicOpEnable = VK_FALSE;
			blendInfo.attachmentCount = 1;
			blendInfo.pAttachments = &blendState;
			dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicInfo.dynamicStateCount = std::size(dynamicStates);
			dynamicInfo.pDynamicStates = dynamicStates;
			gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			gpInfo.stageCount = std::size(stageInfo);
			gpInfo.pStages = stageInfo;
			gpInfo.pVertexInputState = &vinStateInfo;
			gpInfo.pInputAssemblyState = &iaInfo;
			gpInfo.pViewportState = &vpInfo;
			gpInfo.pRasterizationState = &rasterizerStateInfo;
			gpInfo.pMultisampleState = &msInfo;
			gpInfo.pColorBlendState = &blendInfo;
			gpInfo.pDynamicState = &dynamicInfo;
			gpInfo.layout = pLayout.get();
			gpInfo.renderPass = renderPass.get();
			gpInfo.subpass = 0;

			VkPipeline pl;
			auto res = vkCreateGraphicsPipelines(this->pInternal.get(), pCache.get(), 1, &gpInfo, nullptr, &pl);
			checkError(res);
			return Pipeline(this->pInternal.get(), pl, &vkDestroyPipeline);
		}
		auto createCommandBuffers(const CommandPool& pool, uint32_t nBuffers)
		{
			VkCommandBufferAllocateInfo cbAllocInfo{};

			cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cbAllocInfo.commandPool = pool.get();
			cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cbAllocInfo.commandBufferCount = nBuffers;

			auto buffers = CommandBuffers(this->pInternal.get(), pool.get(), nBuffers);
			auto res = vkAllocateCommandBuffers(this->pInternal.get(), &cbAllocInfo, buffers.data());
			checkError(res);
			return buffers;
		}
		auto createFence()
		{
			VkFenceCreateInfo finfo{};

			finfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

			VkFence fence;
			auto res = vkCreateFence(this->pInternal.get(), &finfo, nullptr, &fence);
			checkError(res);
			return Fence(this->pInternal.get(), fence, &vkDestroyFence);
		}

		// Command Shortcuts //
		void submitCommandAndWait(VkCommandBuffer buffer)
		{
			static VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			VkSubmitInfo sinfo{};

			sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			sinfo.pWaitDstStageMask = &stageFlags;
			sinfo.commandBufferCount = 1;
			sinfo.pCommandBuffers = &buffer;

			auto res = vkQueueSubmit(this->devQueue, 1, &sinfo, VK_NULL_HANDLE);
			checkError(res);
			res = vkQueueWaitIdle(this->devQueue);
			checkError(res);
		}
		void acquireNextImageAndWait(const Swapchain& swapchain, const Fence& fence, uint32_t& nextFrameIndex)
		{
			auto res = vkAcquireNextImageKHR(this->pInternal.get(), swapchain.get(),
				UINT64_MAX, VK_NULL_HANDLE, fence.get(), &nextFrameIndex);
			Vulkan::checkError(res);
			res = vkWaitForFences(this->pInternal.get(), 1, &fence.get(), VK_FALSE, UINT64_MAX);
			Vulkan::checkError(res);
			res = vkResetFences(this->pInternal.get(), 1, &fence.get());
			Vulkan::checkError(res);
		}
		void submitCommands(VkCommandBuffer buffer, const Fence& fence)
		{
			VkSubmitInfo sinfo{};
			static const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

			sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			sinfo.commandBufferCount = 1;
			sinfo.pCommandBuffers = &buffer;
			sinfo.pWaitDstStageMask = &waitStageMask;
			auto res = vkQueueSubmit(devQueue, 1, &sinfo, fence.get());
			checkError(res);
		}
		auto waitForFence(const Fence& fence)
		{
			return vkWaitForFences(this->pInternal.get(), 1, &fence.get(), VK_TRUE, UINT64_MAX);
		}
		void present(const Swapchain& swapchain, uint32_t frameIndex)
		{
			VkPresentInfoKHR pinfo{};

			pinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			pinfo.swapchainCount = 1;
			pinfo.pSwapchains = &swapchain.get();
			pinfo.pImageIndices = &frameIndex;

			auto res = vkQueuePresentKHR(this->devQueue, &pinfo);
			checkError(res);
		}
		void resetFence(const Fence& fence)
		{
			auto res = vkResetFences(this->pInternal.get(), 1, &fence.get());
			checkError(res);
		}
	};

	void initialImageLayouting(VkCommandBuffer buffer, const ImageArray& images)
	{
		auto barriers = std::make_unique<VkImageMemoryBarrier[]>(size(images));

		for (uint32_t i = 0; i < size(images); i++)
		{
			VkImageMemoryBarrier barrier{};

			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.image = images.first[i];
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;
			barriers[i] = barrier;
		}

		vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, size(images), barriers.get());
	}
	void beginCommandWithFramebuffer(VkCommandBuffer buffer, const Framebuffer& fb)
	{
		VkCommandBufferInheritanceInfo inhInfo{};
		VkCommandBufferBeginInfo beginInfo{};

		inhInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inhInfo.framebuffer = fb.get();
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pInheritanceInfo = &inhInfo;

		vkBeginCommandBuffer(buffer, &beginInfo);
	}
	void barrierResource(VkCommandBuffer buffer, VkImage img,
		VkPipelineStageFlags srcStageFlags, VkPipelineStageFlags dstStageFlags,
		VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
		VkImageLayout srcImageLayout, VkImageLayout dstImageLayout)
	{
		VkImageMemoryBarrier barrier{};

		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = img;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.oldLayout = srcImageLayout;
		barrier.newLayout = dstImageLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdPipelineBarrier(buffer, srcStageFlags, dstStageFlags, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}
	void beginRenderPass(VkCommandBuffer buffer, const Framebuffer& frame, const RenderPass& renderPass)
	{
		static VkClearValue clearValue
		{
			{ 0.0f, 0.0f, 0.0f, 1.0f }
		};
		VkRenderPassBeginInfo rpinfo{};

		rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpinfo.framebuffer = frame.get();
		rpinfo.renderPass = renderPass.get();
		rpinfo.renderArea.extent.width = 640;
		rpinfo.renderArea.extent.height = 480;
		rpinfo.clearValueCount = 1;
		rpinfo.pClearValues = &clearValue;
		
		vkCmdBeginRenderPass(buffer, &rpinfo, VK_SUBPASS_CONTENTS_INLINE);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	auto hWnd = initApp(hInstance);
	if (hWnd == nullptr) return 1;

	auto instance = Vulkan::createInstance();
	auto reporter = Vulkan::createDebugReportCallback(instance);
	auto pDevice = Vulkan::enumerateAndGetDefaultPhysicalDevice(instance);
	auto device = Vulkan::Device::create(pDevice);
	auto cmdPool = device.createCommandPool();
	auto fence = device.createFence();

	auto surface = Vulkan::createSurfaceForHwnd(instance, hWnd);
	auto swapchain = device.createSwapchain(surface);
	auto images = device.retrieveImagesFromSwapchain( swapchain);
	auto imageViews = device.createImageViews(images);
	auto renderPass = device.createCommonRenderPass();
	auto frameBuffers = device.createFramebuffers(renderPass, imageViews);
	auto cmdBuffers = device.createCommandBuffers(cmdPool, Vulkan::size(frameBuffers));

	static VertexData verticesData[] = {
		{ { 0.0f, -0.75f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
		{ { -0.5f, 0.75f }, { 1.0f, 0.5f, 0.0f, 1.0f } },
		{ { 0.5f, 0.75f }, { 0.0f, 0.5f, 1.0f, 1.0f } }
	};
	static VkVertexInputBindingDescription bindDesc
	{
		0, sizeof(VertexData), VK_VERTEX_INPUT_RATE_VERTEX
	};
	static VkVertexInputAttributeDescription attrDescs[] =
	{
		{ 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
		{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 2 }
	};
	auto vertices = device.createVertexBuffer(verticesData);
	auto vs = device.createShaderModule(L"VertexShader.vert.spv");
	auto fs = device.createShaderModule(L"FragmentShader.frag.spv");
	auto pLayout = device.createPipelineLayout();
	auto pCache = device.createPipelineCache();
	auto pipeline = device.createGraphicsPipelineVF(vs, fs, bindDesc, attrDescs, pLayout, renderPass, pCache);

	Vulkan::beginCommandWithFramebuffer(cmdBuffers[0], Vulkan::Framebuffer());
	Vulkan::initialImageLayouting(cmdBuffers[0], images);
	auto res = vkEndCommandBuffer(cmdBuffers[0]);
	Vulkan::checkError(res);
	device.submitCommandAndWait(cmdBuffers[0]);

	uint32_t currentFrameIndex = 0;
	// Acquire First
	device.acquireNextImageAndWait(swapchain, fence, currentFrameIndex);

	g_RenderFunc = [&]()
	{
		static VkViewport vp = { 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f };
		static VkRect2D sc = { { 0, 0 }, { 640, 480 } };
		static VkDeviceSize offsets[] = { 0 };

		Vulkan::beginCommandWithFramebuffer(cmdBuffers[0], frameBuffers.first[currentFrameIndex]);
		Vulkan::barrierResource(cmdBuffers[0], images.first[currentFrameIndex],
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		Vulkan::beginRenderPass(cmdBuffers[0], frameBuffers.first[currentFrameIndex], renderPass);
		vkCmdBindPipeline(cmdBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		vkCmdSetViewport(cmdBuffers[0], 0, 1, &vp);
		vkCmdSetScissor(cmdBuffers[0], 0, 1, &sc);
		vkCmdBindVertexBuffers(cmdBuffers[0], 0, 1, &vertices.first.get(), offsets);
		vkCmdDraw(cmdBuffers[0], 3, 1, 0, 0);
		vkCmdEndRenderPass(cmdBuffers[0]);
		/*Vulkan::barrierResource(cmdBuffers[0], images.first[currentFrameIndex],
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);*/
		vkEndCommandBuffer(cmdBuffers[0]);
		
		// Submit and Wait with Fences
		device.submitCommands(cmdBuffers[0], fence);
		switch (device.waitForFence(fence))
		{
		case VK_SUCCESS: device.present(swapchain, currentFrameIndex); break;
		case VK_TIMEOUT: throw std::runtime_error("Command execution timed out."); break;
		default: OutputDebugString(L"waitForFence returns unknown value.\n");
		}
		device.resetFence(fence);

		// Acquire next
		device.acquireNextImageAndWait(swapchain, fence, currentFrameIndex);
	};

	ShowWindow(hWnd, nCmdShow);
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0) > 0)
	{
		DispatchMessage(&msg);
	}

	return msg.wParam;
}
