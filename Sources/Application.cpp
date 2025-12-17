//
// Created by ianpo on 13/10/2025.
//

#include "MVT/Application.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stb_image.h>
#include <stdexcept>
#include <utility>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "MVT/GLM.hpp"
#include "MVT/Mesh.hpp"
#include "MVT/SlangCompiler.hpp"
#include "MVT/UniformBufferObject.hpp"
#include "MVT/VulkanMesh.hpp"

#define MVT_ALIGN_SIZE(size, alignement) (size % alignment == 0 ? size : ((size / alignment) + 1) * alignment)

namespace MVT {
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
		if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}

	Application::Application() {
		initWindow("Modern Vulkan");
		initVulkan("Modern Vulkan");
	}

	Application::~Application() = default;

	void Application::run() {
		mainLoop();
		cleanup();
	}

	void Application::initWindow(const char *appName, WindowParameters parameters) {
		// We initialize SDL and create a window with it.
		SDL_Init(SDL_INIT_VIDEO);

		SDL_WindowFlags window_flags{};

		if (parameters.Resizable) window_flags |= SDL_WINDOW_RESIZABLE;

		window_flags |= (SDL_WindowFlags) (SDL_WINDOW_VULKAN);

		// TODO: Use `parameters.Resizable`.
		m_Window = SDL_CreateWindow(appName, parameters.Width, parameters.Height, window_flags);
	}

	void Application::initVulkan(const char *appName) {
		createInstance(appName);
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createVMA();

		createSwapChain();
		createSwapChainViews();

		createDescriptorSetLayout();
		createGraphicsPipeline();

		createCommandPool();
		createCommandBuffer();
		createSyncObjects();

		createTextureImage();

		createVertexBuffer(rectangle_vertices);
		createIndexBuffer(rectangle_indices);
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();

		// createCommandBuffer();
		// createSyncObjects();
	}

	void Application::mainLoop() {
		const auto shaderFile = std::filesystem::path{"./EngineAssets/Shaders/mesh.slang"};
		auto date = std::filesystem::last_write_time(shaderFile);
		while (!m_ShouldClose) {
			// Handle SDL Events
			{
				SDL_Event e;
				while (SDL_PollEvent(&e) != 0) {
					// ImGui_ImplSDL3_ProcessEvent(&e);

					switch (e.type) {
						case SDL_EVENT_QUIT: {
							m_ShouldClose = true;
						}
						break;

						case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
						case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED: {
							framebufferResized = true;
							windowMinimized = e.window.data1 == 0 || e.window.data2 == 0;
						}
						break;
						case SDL_EVENT_WINDOW_MINIMIZED: {
							windowMinimized = true;
							framebufferResized = true;
						}
						break;
						case SDL_EVENT_WINDOW_RESTORED:
						case SDL_EVENT_WINDOW_MAXIMIZED: {
							windowMinimized = true;
							framebufferResized = true;
						}
						break;
						default: {
							break;
						};
					}
				}
			}

			auto newDate = std::filesystem::last_write_time(shaderFile);
			if (newDate > date) {
				device.waitIdle();
				createGraphicsPipeline();
				std::cout << "Hot Reload Shader" << std::endl;
				date = newDate;
			}

			if (!windowMinimized) {
				// Rest of the code
				drawFrame();
			}
			else {
				device.waitIdle();
				// recreateSwapChain();
			}
		}
	}

	void Application::cleanup() {
		if (*device) {
			device.waitIdle();
		}

		descriptorSets.clear();

		descriptorPool.clear();

		uniformBuffersMemory.clear();
		uniformBuffersMapped.clear();
		uniformBuffers.clear();

		indexBufferMemory.clear();
		indexBuffer.clear();

		vertexBufferMemory.clear();
		vertexBuffer.clear();

		presentCompleteSemaphores.clear();
		renderFinishedSemaphores.clear();
		inFlightFences.clear();
		transferFence.clear();

		textureImageMemory.clear();
		textureImage.clear();

		commandBuffers.clear();
		// transferCommands.clear();

		commandPool.clear();
		//transfersPool.clear();

		graphicsPipeline.clear();

		pipelineLayout.clear();

		descriptorSetLayout.clear();

		cleanupSwapChain();

		//transferQueue.clear();
		presentQueue.clear();
		graphicsQueue.clear();

		cleanupVMA();

		device.clear();

		physicalDevice.clear();

		surface.clear();

		if constexpr (enableValidationLayers) {
			debugMessenger.clear();
		}

		instance.clear();

		if (m_Window) {
			SDL_DestroyWindow(m_Window);
			m_Window = nullptr;
		}
	}

	void Application::drawFrame() {
		if (!*graphicsPipeline) {
			std::cerr << "No Graphics Pipeline available." << std::endl;
			return;
		}

		while (vk::Result::eTimeout == device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX)) {
			std::cerr << "Waiting for 'inFlightFences' timed out. Waiting again." << std::endl;
		}

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr);
		switch (result) {
			case vk::Result::eSuccess: {
				break;
			}
			case vk::Result::eSuboptimalKHR: {
				framebufferResized = true;
				break;
			}
			case vk::Result::eErrorOutOfDateKHR: {
				recreateSwapChain();
				break;
			}
			default:
				throw std::runtime_error("[Vulkan] Failed to acquire swap chain image!");
				break;
		}

		device.resetFences(*inFlightFences[currentFrame]);
		commandBuffers[currentFrame].reset();
		recordCommandBuffer(imageIndex);

		updateUniformBuffer(currentFrame);

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo submitInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*presentCompleteSemaphores[semaphoreIndex],
			.pWaitDstStageMask = &waitDestinationStageMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandBuffers[currentFrame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*renderFinishedSemaphores[semaphoreIndex],
		};
		graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

		// const vk::PresentInfoKHR presentInfoKHR( **renderFinishedSemaphore, **swapChain, imageIndex );
		const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*renderFinishedSemaphores[semaphoreIndex],
			.swapchainCount = 1,
			.pSwapchains = &*swapChain,
			.pImageIndices = &imageIndex,
			.pResults = nullptr,
		};
		result = presentQueue.presentKHR(presentInfoKHR);

		switch (result) {
			case vk::Result::eSuccess:
				break;
			case vk::Result::eSuboptimalKHR:
			case vk::Result::eErrorOutOfDateKHR: {
				framebufferResized = true;
				break;
			}
			default:
				break; // an unexpected result is returned!
		}

		if (framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		}

		semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphores.size();
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void Application::createInstance(const char *appName) {
		vk::ApplicationInfo appInfo{
			.pApplicationName = appName,
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "No Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = vk::ApiVersion14
		};
		// Get the required layers
		std::vector<char const *> requiredLayers;
		if constexpr (enableValidationLayers) {
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties = context.enumerateInstanceLayerProperties();
		if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const &requiredLayer) {
			return std::ranges::none_of(layerProperties,
										[requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		})) {
			throw std::runtime_error("[Vulkan] One or more required layers are not supported!");
		}

		const auto requiredExtensions = GetExtensions();
		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		// auto [extRes, extensionProperties] = context.enumerateInstanceExtensionProperties();
		// assert(extRes == vk::Result::eSuccess);
		auto extensionProperties = context.enumerateInstanceExtensionProperties();

		for (auto extension: requiredExtensions) {
			if (std::find_if(extensionProperties.begin(), extensionProperties.end(), [&extension](const vk::ExtensionProperties &extensionProperty) { return strcmp(extensionProperty.extensionName, extension) == 0; }) == extensionProperties.end()) {
				throw std::runtime_error("[Vulkan] Required window extension not supported: " + std::string{extension});
			}
		}

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
			.ppEnabledLayerNames = requiredLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
			.ppEnabledExtensionNames = requiredExtensions.data()
		};

		// auto [instRes, inst] = context.createInstance(createInfo);
		// assert(instRes == vk::Result::eSuccess);
		// instance = std::move(inst);
		instance = vk::raii::Instance(context, createInfo); {
			// 	auto [result, exts] = context.enumerateInstanceExtensionProperties();
			// 	std::cout << "available extensions:\n";
			auto exts = context.enumerateInstanceExtensionProperties();
			std::cout << "available vulkan extensions:\n";

			for (const auto &extension: exts) {
				std::cout << '\t' << extension.extensionName << '\n';
			}
			std::cout << std::endl;
		}
	}

	void Application::setupDebugMessenger() {
		if constexpr (!enableValidationLayers) return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
			.messageSeverity = severityFlags,
			.messageType = messageTypeFlags,
			.pfnUserCallback = &debugCallback
		};
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	bool Application::RatePhysicalDevice(const std::vector<vk::raii::PhysicalDevice>::value_type &device, uint32_t &score) {
		auto deviceProperties = device.getProperties();
		auto deviceFeatures = device.getFeatures();
		score = 0;

		// Application can't function without geometry shaders
		if (!deviceFeatures.geometryShader) {
			return false;
		}

		// Application can't function without tessellation shaders
		if (!deviceFeatures.tessellationShader) {
			return false;
		}

		// Discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			score += 1000;
		}

		// Maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		return true;
	}

	void Application::pickPhysicalDevice() {
		auto devices = instance.enumeratePhysicalDevices();
		if (devices.empty()) {
			throw std::runtime_error("[Vulkan] failed to find GPUs with Vulkan support!");
		}

		// Use an ordered map to automatically sort candidates by increasing score
		std::multimap<int, vk::raii::PhysicalDevice> candidates;

		for (const auto &device: devices) {
			uint32_t score;

			if (!RatePhysicalDevice(device, score)) continue;

			candidates.insert(std::make_pair(score, device));
		}

		// Check if the best candidate is suitable at all
		if (candidates.rbegin()->first > 0) {
			physicalDevice = candidates.rbegin()->second;
		}
		else {
			throw std::runtime_error("[Vulkan] failed to find a suitable GPU!");
		}
	}

	uint32_t Application::findQueueFamilies(vk::PhysicalDevice device, vk::QueueFlags queueType) {
		// find the index of the first queue family that supports graphics
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = device.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports graphics
		auto graphicsQueueFamilyProperty =
				std::find_if(queueFamilyProperties.begin(),
							 queueFamilyProperties.end(),
							 [queueType](vk::QueueFamilyProperties const &qfp) { return qfp.queueFlags & queueType; });

		return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
	}

	void Application::createLogicalDevice() {
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// Currently only return a graphic compatible family.
		float queuePriority = 0.0f;
		graphicsFamily = findQueueFamilies(physicalDevice, vk::QueueFlagBits::eGraphics);

		// determine a queueFamilyIndex that supports present
		// first check if the graphicsIndex is good enough
		presentFamily = physicalDevice.getSurfaceSupportKHR(graphicsFamily, *surface)
							? graphicsFamily
							: static_cast<uint32_t>(queueFamilyProperties.size());


		if (presentFamily == queueFamilyProperties.size()) {
			// the graphicsIndex doesn't support present -> look for another family index that supports both
			// graphics and present
			for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
				if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
					physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
					graphicsFamily = static_cast<uint32_t>(i);
					presentFamily = graphicsFamily;
					break;
				}
			}

			if (presentFamily == queueFamilyProperties.size()) {
				// there's nothing like a single family index that supports both graphics and present -> look for another
				// family index that supports present
				for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
					if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
						presentFamily = static_cast<uint32_t>(i);
						break;
					}
				}
			}
		}

		if ((graphicsFamily == queueFamilyProperties.size()) || (presentFamily == queueFamilyProperties.size())) {
			throw std::runtime_error("[Vulkan] Could not find a queue for graphics or present -> terminating");
		}

		// //transferQueue
		// transferFamily = findQueueFamilies(physicalDevice, [](const vk::QueueFamilyProperties &q, uint32_t i) { return q.queueFlags & vk::QueueFlagBits::eTransfer && !(q.queueFlags & vk::QueueFlagBits::eGraphics); });
		// if (transferFamily == queueFamilyProperties.size()) {
		// 	transferFamily = findQueueFamilies(physicalDevice, vk::QueueFlagBits::eTransfer);
		// 	if (transferFamily == queueFamilyProperties.size()) {
		// 		throw std::runtime_error("[Vulkan] Could not find a queue for transfer -> terminating");
		// 	}
		// }

		std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos{vk::DeviceQueueCreateInfo{.queueFamilyIndex = graphicsFamily, .queueCount = 1, .pQueuePriorities = &queuePriority}};

		bool graphic = true;
		bool present = false;
		bool transfer = false;

		if (graphicsFamily != presentFamily) {
			deviceQueueCreateInfos.push_back(vk::DeviceQueueCreateInfo{.queueFamilyIndex = presentFamily, .queueCount = 1, .pQueuePriorities = &queuePriority});
			present = true;
		}

		// if (graphicsFamily != transferFamily && presentFamily != transferFamily) {
		// 	transfer = true;
		// 	deviceQueueCreateInfos.push_back(vk::DeviceQueueCreateInfo{.queueFamilyIndex = transferFamily, .queueCount = 1, .pQueuePriorities = &queuePriority});
		// }

		vk::PhysicalDeviceFeatures physicalDeviceFeatures = physicalDevice.getFeatures();

		// Create a chain of feature structures
		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
			{}, // vk::PhysicalDeviceFeatures2 (empty for now)
			{.synchronization2 = true, .dynamicRendering = true,}, // Enable dynamic rendering from Vulkan 1.3
			{.extendedDynamicState = true} // Enable extended dynamic state from the extension
		};

		vk::PhysicalDeviceVulkan11Features features11{
			.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
			.shaderDrawParameters = true,
		};

		// Extensions needed for the application to work properly
		std::vector<const char *> deviceExtensions = {
			vk::KHRSwapchainExtensionName,
			vk::KHRSpirv14ExtensionName,
			vk::KHRSynchronization2ExtensionName,
			vk::KHRCreateRenderpass2ExtensionName
		};

		vk::DeviceCreateInfo deviceCreateInfo{
			.pNext = &features11,
			.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size()),
			.pQueueCreateInfos = deviceQueueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data()
		};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		graphicsQueue = vk::raii::Queue(device, graphicsFamily, 0);
		presentQueue = vk::raii::Queue(device, presentFamily, 0);
		// transferQueue = vk::raii::Queue(device, transferFamily, 0);
	}

	void Application::createVMA() {
		vma = std::make_unique<VulkanMemoryAllocator>(instance, physicalDevice, device);
	}

	void Application::cleanupVMA() {
		vma.reset();
	}

	void Application::createSurface() {
		VkSurfaceKHR tmpSurface;
		const bool result = SDL_Vulkan_CreateSurface(m_Window, *instance, nullptr, &tmpSurface);

		if (!result) {
			throw std::runtime_error("[SDL] Couldn't create a Vulkan Surface.\n" + std::string(SDL_GetError()));
		}

		surface = {instance, tmpSurface, nullptr};
	}

	void Application::createSwapChain() {
		auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);
		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

		auto swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
		auto swapChainExtent = chooseSwapExtent(surfaceCapabilities);

		assert(swapChainExtent.width > 0 && swapChainExtent.height > 0);
		swapChainExtent.width = std::max(swapChainExtent.width, 1u);
		swapChainExtent.height = std::max(swapChainExtent.height, 1u);

		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) ? surfaceCapabilities.maxImageCount : minImageCount;

		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{
			.flags = vk::SwapchainCreateFlagsKHR(),
			.surface = surface,
			.minImageCount = minImageCount,
			.imageFormat = swapChainSurfaceFormat.format, .imageColorSpace = swapChainSurfaceFormat.colorSpace,
			.imageExtent = swapChainExtent, .imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment, .imageSharingMode = vk::SharingMode::eExclusive,
			.preTransform = surfaceCapabilities.currentTransform, .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(surface)),
			.clipped = true, .oldSwapchain = nullptr
		};

		uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};

		if (graphicsFamily != presentFamily) {
			swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
			swapChainCreateInfo.queueFamilyIndexCount = 2;
			swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
			swapChainCreateInfo.queueFamilyIndexCount = 0; // Optional
			swapChainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
		}
		swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();

		this->swapChainImageFormat = swapChainSurfaceFormat.format;
		this->swapChainExtent = swapChainExtent;
	}

	void Application::createSwapChainViews() {
		swapChainImageViews.clear();

		vk::ImageViewCreateInfo imageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = swapChainImageFormat,
			.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		};

		for (vk::Image image: swapChainImages) {
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
	}

	void Application::createDescriptorSetLayout() {
		vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr);
		vk::DescriptorSetLayoutCreateInfo layoutInfo{
			.bindingCount = 1,
			.pBindings = &uboLayoutBinding,
		};
		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void Application::createGraphicsPipeline() {
		//Basic code, we could upgrade it with an all-in-one function that seatch and find every function name in the slang shader available.

		auto spirvCode = SlangCompiler::s_OneShotCompile("mesh");
		if (spirvCode.has_error()) {
			std::cerr << "Fail to compile mesh.slang" << std::endl;
			return;
		}


		auto shaderModule = createShaderModule(spirvCode.value());

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};

		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = attributeDescriptions.size(),
			.pVertexAttributeDescriptions = attributeDescriptions.data()
		};

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
		vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{
			.depthClampEnable = vk::False, .rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill, .cullMode = vk::CullModeFlagBits::eNone,
			.frontFace = vk::FrontFace::eCounterClockwise, .depthBiasEnable = vk::False,
			.depthBiasSlopeFactor = 1.0f, .lineWidth = 1.0f
		};

		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable = vk::True,
			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			.colorBlendOp = vk::BlendOp::eAdd,
			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			.alphaBlendOp = vk::BlendOp::eAdd,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
		};

		vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		std::vector dynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0, .pPushConstantRanges = nullptr};

		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainImageFormat};

		vk::GraphicsPipelineCreateInfo pipelineInfo{
			.pNext = &pipelineRenderingCreateInfo,
			.stageCount = shaderStages.size(), .pStages = shaderStages.data(),
			.pVertexInputState = &vertexInputInfo, .pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState, .pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling, .pColorBlendState = &colorBlending,
			.pDynamicState = &dynamicState, .layout = pipelineLayout, .renderPass = nullptr,
			.basePipelineHandle = VK_NULL_HANDLE, // Optional
			.basePipelineIndex = -1 // Optional
		};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
	}

	void Application::createCommandPool() {
		{
			vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = graphicsFamily};
			commandPool = vk::raii::CommandPool(device, poolInfo);
		}
		// {
		// 	vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = transferFamily};
		// 	transfersPool = vk::raii::CommandPool(device, poolInfo);
		// 	assert(*transfersPool);
		// }
	}

	void Application::createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc *const pixels = stbi_load("EngineAssets/Textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		vk::DeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});

		createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void *data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);
		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		vk::raii::Image textureImageTemp({});
		vk::raii::DeviceMemory textureImageMemoryTemp({});
		createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImageTemp, textureImageMemoryTemp);

		transitionImageLayout(textureImageTemp, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, QueueType::Transfer);
		copyBufferToImage(stagingBuffer, textureImageTemp, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), QueueType::Transfer);
		transitionImageLayout(textureImageTemp, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, QueueType::Graphics);
	}

	void Application::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory) {
		createImage(width, height, format, tiling, usage, properties, image, imageMemory, {graphicsFamily});
	}

	void Application::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory, const std::vector<uint32_t> &families) {
		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D, .format = format,
			.extent = {width, height, 1}, .mipLevels = 1, .arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1, .tiling = tiling,
			.usage = usage,
			.sharingMode = families.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = static_cast<uint32_t>(families.size()),
			.pQueueFamilyIndices = families.data()
		};

		image = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
		};
		imageMemory = vk::raii::DeviceMemory(device, allocInfo);
		image.bindMemory(imageMemory, 0);
	}

	void Application::createBuffer(const vk::DeviceSize size, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) {
		createBuffer(size, usage, properties, buffer, bufferMemory, {graphicsFamily});
	}

	void Application::createBuffer(const vk::DeviceSize size, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory, const std::vector<uint32_t> &families) {
		// const std::array families = {graphicsFamily, transferFamily};

		vk::BufferCreateInfo bufferInfo{
			.size = size,
			.usage = usage,
			.sharingMode = families.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = static_cast<uint32_t>(families.size()),
			.pQueueFamilyIndices = families.data()
		};

		buffer = vk::raii::Buffer(device, bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
		bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
	}

	void Application::createVertexBuffer(const std::vector<Vertex> &vertices) {
		createVertexBuffer(vertices.data(), vertices.size());
	}

	void Application::waitAndResetFence() {
		auto result = device.waitForFences(*transferFence, true, UINT64_MAX);
		assert(result == vk::Result::eSuccess);
		device.resetFences(*transferFence);
	}

	void Application::createVertexBuffer(const Vertex *vertices, const uint64_t count) {
		const vk::DeviceSize bufferSize = sizeof(Vertex) * count;

		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingBufferMemory = nullptr;
		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(dataStaging, vertices, bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer|vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

		copyBuffer(stagingBuffer, vertexBuffer, bufferSize, transferFence, commandPool,graphicsQueue);
		waitAndResetFence();
		//transferBufferQueue(vertexBuffer, QueueType::Transfer, QueueType::Graphics,vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eVertexInput);

	}

	void Application::createIndexBuffer(const uint32_t *indices, const uint32_t count) {
		vk::DeviceSize bufferSize = sizeof(indices[0]) * count;

		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void *data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices, (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize, transferFence, commandPool, graphicsQueue);
		waitAndResetFence();
		//transferBufferQueue(indexBuffer, QueueType::Transfer, QueueType::Graphics,vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eVertexInput);
	}

	void Application::createUniformBuffers() {
		uniformBuffers.clear();
		uniformBuffersMemory.clear();
		uniformBuffersMapped.clear();

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
			vk::raii::Buffer buffer({});
			vk::raii::DeviceMemory bufferMem({});

			createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem, {graphicsFamily});

			uniformBuffers.emplace_back(std::move(buffer));
			uniformBuffersMemory.emplace_back(std::move(bufferMem));
			uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, bufferSize));
		}
	}

	void Application::createDescriptorPool() {
		vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT);

		vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, .maxSets = MAX_FRAMES_IN_FLIGHT, .poolSizeCount = 1, .pPoolSizes = &poolSize};

		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void Application::createDescriptorSets() {
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool, .descriptorSetCount = static_cast<uint32_t>(layouts.size()), .pSetLayouts = layouts.data()};
		descriptorSets.clear();
		descriptorSets = device.allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
			vk::WriteDescriptorSet descriptorWrite{.dstSet = descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &bufferInfo};
			device.updateDescriptorSets(descriptorWrite, {});
		}
	}

	VulkanMesh Application::createVulkanMesh(const Vertex *vertex, const uint32_t vCount, const uint32_t *indices, const uint32_t iCount) {
		const uint64_t sizeVertices = sizeof(*vertex) * vCount;
		const uint64_t sizeIndices = sizeof(*indices) * iCount;

		const uint64_t offsetVertices = 0;
		const uint64_t offsetIndices = sizeof(*vertex) * vCount;

		const vk::DeviceSize bufferSize = sizeVertices + sizeIndices;

		// const std::vector<uint32_t> families = graphicsFamily != transferFamily ? std::vector<uint32_t>{graphicsFamily, transferFamily} : std::vector<uint32_t>{graphicsFamily};
		const std::vector<uint32_t> families = std::vector<uint32_t>{graphicsFamily};

		vk::BufferCreateInfo vBufferInfo{
			.size = sizeVertices,
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = families.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = static_cast<uint32_t>(families.size()),
			.pQueueFamilyIndices = families.data()
		};

		vk::BufferCreateInfo iBufferInfo{
			.size = sizeIndices,
			.usage = vk::BufferUsageFlagBits::eIndexBuffer,
			.sharingMode = families.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = static_cast<uint32_t>(families.size()),
			.pQueueFamilyIndices = families.data()
		};

		vk::raii::Buffer vertexBuffer = vk::raii::Buffer(device, vBufferInfo);
		vk::raii::Buffer indexBuffer = vk::raii::Buffer(device, iBufferInfo);

		const vk::MemoryRequirements vMemRequirements = vertexBuffer.getMemoryRequirements();
		const vk::MemoryRequirements iMemRequirements = indexBuffer.getMemoryRequirements();

		const vk::DeviceSize alignment = std::max(vMemRequirements.alignment, iMemRequirements.alignment);

		const vk::DeviceSize offsetIndex = MVT_ALIGN_SIZE(vMemRequirements.size, alignment);
		const vk::DeviceSize size = offsetIndex + MVT_ALIGN_SIZE(iMemRequirements.size, alignment);

		vk::MemoryAllocateInfo allocInfo{.allocationSize = size, .memoryTypeIndex = findMemoryType(vMemRequirements.memoryTypeBits | iMemRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
		vk::raii::DeviceMemory memory = vk::raii::DeviceMemory(device, allocInfo);

		vertexBuffer.bindMemory(*memory, 0);
		indexBuffer.bindMemory(*memory, offsetIndex);

		return VulkanMesh{std::move(memory), std::move(vertexBuffer), std::move(indexBuffer)};
	}

	void Application::createCommandBuffer() { {
			vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
			commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
		}
		// {
		// 	vk::CommandBufferAllocateInfo allocInfo{.commandPool = transfersPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
		// 	transferCommands = vk::raii::CommandBuffers(device, allocInfo);
		// 	assert(transferCommands.size() == MAX_FRAMES_IN_FLIGHT);
		// }
	}

	void Application::createSyncObjects() {
		presentCompleteSemaphores.clear();
		renderFinishedSemaphores.clear();
		inFlightFences.clear();
		transferFence.clear();

		presentCompleteSemaphores.reserve(swapChainImages.size());
		renderFinishedSemaphores.reserve(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
		}

		transferFence = {device, vk::FenceCreateInfo{}};
	}

	void Application::recordCommandBuffer(const uint32_t imageIndex) {
		commandBuffers[currentFrame].begin({});

		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		transition_image_layout(
			imageIndex,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{}, // srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
			vk::PipelineStageFlagBits2::eTopOfPipe, // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput // dstStage
		);

		// Clear buffer with clear color
		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo = {
			.imageView = swapChainImageViews[imageIndex],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor
		};

		vk::RenderingInfo renderingInfo = {
			.renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentInfo
		}; {
			commandBuffers[currentFrame].beginRendering(renderingInfo);

			commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
			commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
			commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

			commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});
			commandBuffers[currentFrame].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

			commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[currentFrame], nullptr);
			commandBuffers[currentFrame].drawIndexed(rectangle_indices.size(), 1, 0, 0, 0);

			commandBuffers[currentFrame].endRendering();
		}

		// After rendering, transition the swapchain image to PRESENT_SRC
		transition_image_layout(
			imageIndex,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite, // srcAccessMask
			{}, // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eBottomOfPipe // dstStage
		);

		commandBuffers[currentFrame].end();
	}

#define PRINT_GLM_VAR(VAR) std::cout << #VAR << ":\n" << glm::to_string(VAR) << std::endl;

	void Application::updateUniformBuffer(const uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		//
		// ubo.model = glm::identity<glm::mat4x4>();
		// ubo.view = glm::identity<glm::mat4x4>();
		// ubo.proj = glm::identity<glm::mat4x4>();

		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;


		memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void Application::transition_image_layout(
		uint32_t imageIndex,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask,
		vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask,
		vk::PipelineStageFlags2 dstStageMask
	) {
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapChainImages[imageIndex],
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		vk::DependencyInfo dependencyInfo = {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};

		commandBuffers[currentFrame].pipelineBarrier2(dependencyInfo);
	}

	// void Application::transferBufferQueue(const vk::Buffer &buffer, QueueType oldQueue, QueueType newQueue, vk::PipelineStageFlags src, vk::PipelineStageFlags dst) {
	//
	// 	uint32_t oldQ = getFamilyIndex(oldQueue);
	// 	uint32_t newQ = getFamilyIndex(newQueue);
	// 	vk::BufferMemoryBarrier barrier {
	// 		.srcQueueFamilyIndex =oldQ,
	// 		.dstQueueFamilyIndex = newQ,
	// 		.buffer = buffer,
	// 		.offset = 0,
	// 		.size = vk::WholeSize,
	// 	};
	//
	// 	auto cmd = beginSingleTimeCommands(newQueue);
	// 	cmd.pipelineBarrier(src, dst, {}, {}, barrier, {});
	// 	endSingleTimeCommands(cmd, newQueue);
	// }


	void Application::transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, QueueType type) {
		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;
		vk::ImageMemoryBarrier barrier{.oldLayout = oldLayout, .newLayout = newLayout, .image = image, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
			barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
			barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
			barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored; //transferFamily;
			barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored; //graphicsFamily;
			// type = QueueType::Graphics;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		auto cmd = beginSingleTimeCommands(type);
		cmd.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
		endSingleTimeCommands(cmd, type, false);
	}

	void Application::copyBufferToImage(const vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, QueueType queue) {
		auto cmd = beginSingleTimeCommands(queue);
		vk::BufferImageCopy region{.bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, .imageOffset = {0, 0, 0}, .imageExtent = {width, height, 1}};
		cmd.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
		endSingleTimeCommands(cmd, queue, false);
		// if (queue == QueueType::Graphics) {
		// 	waitAndResetFence();
		// }

	}

	vk::SurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
		for (const auto &availableFormat: availableFormats) {
			if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				return availableFormat;
			}
		}

		// Backup, the first one should be good enough for my use case if I don't have what I want.
		return availableFormats[0];
	}

	vk::PresentModeKHR Application::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
		// Target PC where energy is not a concern, Mailbox is better if available
		for (const auto &availablePresentMode: availablePresentModes) {
			if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
				return availablePresentMode;
			}
		}

		// Backup, Guaranteed to be available
		return vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D Application::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

		const auto [width, height] = GetFramebufferSize();

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	}

	vk::raii::ShaderModule Application::createShaderModule(const std::vector<char> &code) const {
		vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t *>(code.data())};
		vk::raii::ShaderModule shaderModule{device, createInfo};
		return std::move(shaderModule);
	}

	uint32_t Application::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void Application::copyBuffer(const vk::raii::Buffer &srcBuffer, const vk::raii::Buffer &vk_buffer, const vk::DeviceSize size, vk::Fence fence, vk::CommandPool pool, vk::Queue queue) {
		auto cmd = beginSingleTimeCommands(pool);
		assert(*cmd);
		// transferCommands[currentFrame].begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
		cmd.copyBuffer(srcBuffer, vk_buffer, vk::BufferCopy(0, 0, size));
		endSingleTimeCommands(cmd, queue, fence);
	}

	void Application::cleanupSwapChain() {
		swapChainImageViews.clear();

		swapChain = nullptr;
	}

	void Application::recreateSwapChain() {
		device.waitIdle();

		cleanupSwapChain();

		createSwapChain();
		createSwapChainViews();
	}

	vk::raii::CommandBuffer Application::beginSingleTimeCommands(QueueType type) {
		switch (type) {
			case QueueType::Present:
				return beginSingleTimeCommands(commandPool);
				break;
			case QueueType::Transfer:
			// 	return beginSingleTimeCommands(transfersPool);
			// 	break;
			// default:
			case QueueType::Graphics:
				return beginSingleTimeCommands(commandPool);
				break;
		}

		return beginSingleTimeCommands(commandPool);
	}

	vk::raii::CommandBuffer Application::beginSingleTimeCommands(vk::CommandPool pool) {
		vk::CommandBufferAllocateInfo allocInfo{.commandPool = pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
		vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

		vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		commandBuffer.begin(beginInfo);

		return commandBuffer;
	}

	void Application::endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer, QueueType type, bool fence) {
		switch (type) {
			case QueueType::Present:
				endSingleTimeCommands(commandBuffer, presentQueue, nullptr);
				break;
			case QueueType::Transfer:
				// endSingleTimeCommands(commandBuffer, transferQueue, fence ? *transferFence : nullptr);
				// break;
			case QueueType::Graphics:
				endSingleTimeCommands(commandBuffer, graphicsQueue, fence ? *transferFence : nullptr);
				break;
		}
	}

	void Application::endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer, vk::Queue queue, vk::Fence fence) {
		commandBuffer.end();

		vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
		queue.submit(submitInfo, fence);
		queue.waitIdle();
	}

	uint32_t Application::getFamilyIndex(const QueueType type) const {
		switch (type) {
			case QueueType::Present:
				return presentFamily;
				break;
			case QueueType::Transfer:
				// return transferFamily;
				// break;
			case QueueType::Graphics:
				return graphicsFamily;
				break;
		}

		return -1;
	}

	std::vector<const char *> Application::GetExtensions() {
		std::vector<const char *> vecExtensions;
		Uint32 extensionCount;
		char const *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

		vecExtensions.reserve(extensionCount + 2);

		vecExtensions.insert(vecExtensions.end(), extensions, extensions + extensionCount);

		if constexpr (enableValidationLayers) {
			vecExtensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

#ifdef __APPLE__
		vecExtensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif

		return std::move(vecExtensions);
	}

	vk::Flags<vk::InstanceCreateFlagBits> Application::GetInstanceFlags() {
#ifdef __APPLE__
		return vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#else
		return {};
#endif
	}

	std::pair<int, int> Application::GetFramebufferSize() {
		int width, height;
		SDL_GetWindowSizeInPixels(m_Window, &width, &height);
		return {width, height};
	}

	std::pair<int, int> Application::GetWindowSize() {
		int width, height;
		SDL_GetWindowSize(m_Window, &width, &height);
		return {width, height};
	}
} // MVT
