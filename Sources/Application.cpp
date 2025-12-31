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
#include <tiny_obj_loader.h>

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
		createColorResources();
		createDepthResources();

		createDescriptorSetLayout();
		createGraphicsPipeline();

		createCommandPool();
		createCommandBuffer();
		createSyncObjects();

		createTextureImage();

		std::array textures = {"EngineAssets/Textures/viking_room.png"};

		loadModel("EngineAssets/Models/viking_room.obj", textures.data(), textures.size());

		createVertexBuffer(two_rectangle_vertices);
		createIndexBuffer(two_rectangle_indices);
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

		m_Meshes.clear();

		presentCompleteSemaphores.clear();
		renderFinishedSemaphores.clear();
		inFlightFences.clear();
		transferFence.clear();

		texture.clear();
		// textureSampler.clear();
		// textureView.clear();
		// textureImageMemory.clear();
		// textureImage.clear();


		commandBuffers.clear();
		// transferCommands.clear();

		commandPool.clear();
		//transfersPool.clear();

		graphicsPipeline.clear();

		pipelineLayout.clear();

		descriptorSetLayout.clear();

		colorImages.clear();
		colorImageMemories.clear();
		colorImageViews.clear();

		depthImageViews.clear();
		depthImageMemory.clear();
		depthImages.clear();

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
		frameCount += 1;
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

	bool Application::RatePhysicalDevice(const vk::PhysicalDevice device, uint32_t &score) {
		const auto deviceProperties = device.getProperties();
		const auto deviceFeatures = device.getFeatures();
		score = 0;

		// Application can't function without geometry shaders
		if (!deviceFeatures.geometryShader) {
			return false;
		}

		// Application can't function without tessellation shaders
		if (!deviceFeatures.tessellationShader) {
			return false;
		}

		// // Application can't function without Anisotropy Sampler
		if (deviceFeatures.samplerAnisotropy) {
			score += deviceProperties.limits.maxSamplerAnisotropy * 10;
		}

		score *= static_cast<uint32_t>(getMaxUsableSampleCount(device));

		// Maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		// Discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			score *= 10;
		}

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
			msaaSamples = getMaxUsableSampleCount();
			const auto properties = physicalDevice.getProperties();
			std::cout << "Select GPU '" << &properties.deviceName[0] << "'" << std::endl;
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
			{.features = {.samplerAnisotropy = physicalDeviceFeatures.samplerAnisotropy}}, // vk::PhysicalDeviceFeatures2 (empty for now)
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
		swapChainImageViews.reserve(swapChainImages.size());

		for (vk::Image image: swapChainImages) {
			swapChainImageViews.push_back(createImageView(image, swapChainImageFormat, vk::ImageAspectFlagBits::eColor, 1));
		}
	}

	void Application::createDescriptorSetLayout() {
		std::array bindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{
			.bindingCount = bindings.size(),
			.pBindings = bindings.data(),
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

		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = msaaSamples,
			.sampleShadingEnable = vk::False
		};

		vk::PipelineDepthStencilStateCreateInfo depthStencil{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False,
		};

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

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineInfo{
			{
				.pNext = &pipelineRenderingCreateInfo,
				.stageCount = shaderStages.size(), .pStages = shaderStages.data(),
				.pVertexInputState = &vertexInputInfo, .pInputAssemblyState = &inputAssembly,
				.pViewportState = &viewportState, .pRasterizationState = &rasterizer,
				.pMultisampleState = &multisampling, .pDepthStencilState = &depthStencil, .pColorBlendState = &colorBlending,
				.pDynamicState = &dynamicState, .layout = pipelineLayout, .renderPass = nullptr,
				.basePipelineHandle = VK_NULL_HANDLE, // Optional
				.basePipelineIndex = -1, // Optional
			},
			{
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &swapChainImageFormat,
				.depthAttachmentFormat = depthFormat
			}
		};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo.get<vk::GraphicsPipelineCreateInfo>());
	}

	void Application::createCommandPool() { {
			vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = graphicsFamily};
			commandPool = vk::raii::CommandPool(device, poolInfo);
		}
		// {
		// 	vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = transferFamily};
		// 	transfersPool = vk::raii::CommandPool(device, poolInfo);
		// 	assert(*transfersPool);
		// }
	}

	void Application::createColorResources() {
		const vk::Format colorFormat = swapChainImageFormat;

		colorImages.clear();
		colorImageMemories.clear();
		colorImageViews.clear();

		colorImages.reserve(depthCount);
		colorImageMemories.reserve(depthCount);
		colorImageViews.reserve(depthCount);

		for (uint64_t i = 0; i < depthCount; ++i) {
			colorImages.emplace_back(nullptr);
			colorImageMemories.emplace_back(nullptr);
			colorImageViews.emplace_back(nullptr);

			createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,  vk::MemoryPropertyFlagBits::eDeviceLocal, colorImages.back(), colorImageMemories.back());
			colorImageViews.back() = createImageView(colorImages.back(), colorFormat, vk::ImageAspectFlagBits::eColor, 1);
		}
	}

	void Application::createDepthResources() {
		// One memory allocation for all the depths images needed.
		depthFormat = findDepthFormat();

		depthImageViews.clear();
		depthImageMemory.clear();
		depthImages.clear();

		depthImages.reserve(depthCount);
		depthImageViews.reserve(depthCount);

		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D, .format = depthFormat,
			.extent = {swapChainExtent.width, swapChainExtent.height, 1}, .mipLevels = 1, .arrayLayers = 1,
			.samples = msaaSamples, .tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			.sharingMode = vk::SharingMode::eExclusive,
		};

		for (int i = 0; i < depthCount; ++i) {
			depthImages.push_back(vk::raii::Image(device, imageInfo));
		}

		vk::MemoryRequirements memRequirements = depthImages.front().getMemoryRequirements();
		const auto offset = memRequirements.size % memRequirements.alignment == 0 ? memRequirements.size : ((memRequirements.size / memRequirements.alignment) + 1) * memRequirements.alignment;
		const auto total = memRequirements.size + offset * (depthCount - 1);
		vk::MemoryAllocateInfo allocInfo{
			.allocationSize = total,
			.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
		};

		depthImageMemory = vk::raii::DeviceMemory(device, allocInfo);

		for (int i = 0; i < depthCount; ++i) {
			depthImages[i].bindMemory(depthImageMemory, i * offset);
			depthImageViews.push_back(createImageView(depthImages[i], depthFormat, vk::ImageAspectFlagBits::eDepth, 1));
		}
	}

	VkTexture Application::createTextureImage(const char *path) {
		VkTexture texture;

		int texWidth, texHeight, texChannels;
		stbi_uc *const pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		vk::DeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		texture.width = texWidth;
		texture.height = texHeight;
		texture.channels = 4;
		texture.CalcMipLevels();

		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});

		createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void *data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);
		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		// vk::raii::Image textureImageTemp({});
		// vk::raii::DeviceMemory textureImageMemoryTemp({});
		texture.format = vk::Format::eR8G8B8A8Srgb;
		createImage(texture.width, texture.height, texture.mipLevels, vk::SampleCountFlagBits::e1, texture.format, vk::ImageTiling::eOptimal,  vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, texture.image, texture.memory);

		transitionImageLayout(texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, QueueType::Transfer, texture.mipLevels);
		copyBufferToImage(stagingBuffer, texture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), QueueType::Transfer);
		// transitionImageLayout(texture.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, QueueType::Graphics, texture.mipLevels);
		//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
		generateMipmaps(texture.image, texture.format, texture.width, texture.height, texture.mipLevels);

		texture.view = createImageView(texture.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, texture.mipLevels);
		texture.sampler = createImageSampler();

		return texture;
	}

	void Application::generateMipmaps(vk::Image image, vk::Format imageFormat, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {

		// Check if image format supports linear blit-ing
		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);
		if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
			// TODO: Do a software resizing with std_image_resize for example.
			throw std::runtime_error("texture image format does not support linear blitting!");
		}


		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands(QueueType::Graphics);

		vk::ImageMemoryBarrier barrier = {
			.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
			.dstAccessMask = vk::AccessFlagBits::eTransferRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		uint32_t mipWidth = texWidth;
		uint32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; ++i) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

			vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
			offsets[0] = vk::Offset3D(0, 0, 0);
			offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
			dstOffsets[0] = vk::Offset3D(0, 0, 0);
			dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);
			vk::ImageBlit blit = { .srcSubresource = {}, .srcOffsets = offsets,
								.dstSubresource =  {}, .dstOffsets = dstOffsets };
			blit.srcSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
			blit.dstSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i, 0, 1);
			commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

			barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

		endSingleTimeCommands(commandBuffer, QueueType::Graphics);
	}

	void Application::createTextureImage() {
		// const char* path = "EngineAssets/Textures/texture.jpg";
		const char* path = "EngineAssets/Textures/viking_room.png";

		texture = createTextureImage(path);
	}

	// void Application::createTextureImageView() {
	// 	textureView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
	// }

	vk::Format Application::findSupportedFormat(const std::vector<vk::Format> &candidates, const vk::ImageTiling tiling, const vk::FormatFeatureFlags features) {
		for (const auto format: candidates) {
			vk::FormatProperties props = physicalDevice.getFormatProperties(format);
			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	vk::Format Application::findDepthFormat() {
		return findSupportedFormat(
			{vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	bool Application::hasStencilComponent(const vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	// void Application::createTextureSampler() {
	// 	textureSampler = createImageSampler();
	// }

	void Application::createImage(uint32_t width, uint32_t height, uint32_t mipLevel, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory) {
		createImage(width, height, mipLevel, numSamples, format, tiling, usage, properties, image, imageMemory, {graphicsFamily});
	}

	void Application::createImage(uint32_t width, uint32_t height, uint32_t mipLevel, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory, const std::vector<uint32_t> &families) {
		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D, .format = format,
			.extent = {width, height, 1}, .mipLevels = mipLevel, .arrayLayers = 1,
			.samples = numSamples, .tiling = tiling,
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

	vk::raii::ImageView Application::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags imageAspect, const uint32_t mipLevels) {
		vk::ImageViewCreateInfo viewInfo{
			.image = image, .viewType = vk::ImageViewType::e2D,
			.format = format, .subresourceRange = {imageAspect, 0, mipLevels, 0, 1}
		};
		return vk::raii::ImageView(device, viewInfo);
	}

	vk::raii::Sampler Application::createImageSampler() {
		vk::SamplerCreateInfo samplerInfo{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0.0f,
			.anisotropyEnable = vk::False,
			.maxAnisotropy = 1.0f,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways,
			.minLod = 0.0f,
			.maxLod = vk::LodClampNone,
			.unnormalizedCoordinates = vk::False,
		};

		vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();
		if (supportedFeatures.samplerAnisotropy) {
			vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
			samplerInfo.anisotropyEnable = vk::True;
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		}

		return vk::raii::Sampler{
			device, samplerInfo
		};
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

	void Application::loadModel(const char *cModelPath, const char** cTexturesPaths, uint32_t textureCount) {
		auto models = loadModel(cModelPath);
		auto& model = models[0];
		model.textures.reserve(textureCount);
		for (uint32_t i = 0; i < textureCount; ++i) {
			model.textures.emplace_back(createTextureImage(cTexturesPaths[i]));
		}

		m_Meshes = std::move(models);
	}

	std::vector<MVT::VkMesh> Application::loadModel(const char *cpath) {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, cpath)) {
			throw std::runtime_error(warn + err);
		}

		vertices.reserve(attrib.vertices.size());
		indices.reserve(attrib.vertices.size());

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};
		uniqueVertices.reserve(attrib.vertices.size());

		// We’re going to combine all the faces in the file into a single model, so just iterate over all the shapes
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = {1.0f, 1.0f, 1.0f};

				if (!uniqueVertices.contains(vertex)) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}

				indices.push_back(uniqueVertices[vertex]);
			}
		}

		VkMesh mesh = createMesh(vertices.data(), static_cast<uint32_t>(vertices.size()), indices.data(), indices.size());
		std::vector<MVT::VkMesh> meshes;
		meshes.emplace_back(std::move(mesh));
		return std::move(meshes);
	}

	MVT::VkMesh Application::createMesh(const Vertex *pVertices, uint32_t verticesCount) {
		std::vector<uint32_t> indices{};
		indices.resize(verticesCount, ~0u);
		for (uint32_t i = 0; i < verticesCount; ++i) {
			indices[i] = i;
		}

		return createMesh(pVertices, verticesCount, indices.data(), verticesCount);
	}

	MVT::VkMesh Application::createMesh(const Vertex *pVertices, const uint32_t verticesCount, const uint32_t *pIndices, const uint32_t indicesCount) {
		MVT::VkMesh mesh{};

		auto vert = makeVertexBuffer(pVertices, verticesCount);
		mesh.m_VertexBuffer = std::move(vert.first);
		mesh.m_VertexMemory = std::move(vert.second);

		auto ind = makeIndexBuffer(pIndices, indicesCount);
		mesh.m_IndexBuffer = std::move(ind.first);
		mesh.m_IndicesMemory = std::move(ind.second);

		mesh.indicesCount = indicesCount;
		mesh.vertexCount = verticesCount;

		return std::move(mesh);
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

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

		copyBuffer(stagingBuffer, vertexBuffer, bufferSize, transferFence, commandPool, graphicsQueue);
		waitAndResetFence();
		//transferBufferQueue(vertexBuffer, QueueType::Transfer, QueueType::Graphics,vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eVertexInput);
	}

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Application::makeVertexBuffer(const Vertex *vertices, const uint64_t count) {
		std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> pair{nullptr, nullptr};
		const vk::DeviceSize bufferSize = sizeof(Vertex) * count;

		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingBufferMemory = nullptr;
		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(dataStaging, vertices, bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, pair.first, pair.second);

		copyBuffer(stagingBuffer, pair.first, bufferSize, transferFence, commandPool, graphicsQueue);
		waitAndResetFence();
		//transferBufferQueue(pair.first, QueueType::Transfer, QueueType::Graphics,vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eVertexInput);

		return std::move(pair);
	}

	void Application::createIndexBuffer(const uint32_t *indices, const uint32_t count) {
		indices_count = count;
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

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Application::makeIndexBuffer(const uint32_t *indices, const uint32_t count) {
		std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> pair{nullptr, nullptr};

		// indices_count = count;
		vk::DeviceSize bufferSize = sizeof(indices[0]) * count;
		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void *data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices, (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, pair.first, pair.second);

		copyBuffer(stagingBuffer, pair.first, bufferSize, transferFence, commandPool, graphicsQueue);
		waitAndResetFence();
		//transferBufferQueue(pair.first, QueueType::Transfer, QueueType::Graphics,vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eVertexInput);

		return std::move(pair);
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
		std::array poolSize{
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT),
		};

		vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, .maxSets = MAX_FRAMES_IN_FLIGHT, .poolSizeCount = poolSize.size(), .pPoolSizes = poolSize.data()};

		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void Application::createDescriptorSets() {
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool, .descriptorSetCount = static_cast<uint32_t>(layouts.size()), .pSetLayouts = layouts.data()};
		descriptorSets.clear();
		descriptorSets = device.allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
			vk::DescriptorImageInfo imageInfo{.sampler = texture.sampler, .imageView = texture.view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

			std::array descriptors{
				vk::WriteDescriptorSet{.dstSet = descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &bufferInfo},
				vk::WriteDescriptorSet{.dstSet = descriptorSets[i], .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .pImageInfo = &imageInfo},
			};
			device.updateDescriptorSets(descriptors, {});
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

		auto& swapImg = swapChainImages[imageIndex];
		auto& swapVw = swapChainImageViews[imageIndex];
		auto& depthImg = depthImages[frameCount % depthCount];
		auto& depthVw = depthImageViews[frameCount % depthCount];
		auto& colorImg = colorImages[frameCount % depthCount];
		auto& colorVw = colorImageViews[frameCount % depthCount];

		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		transition_image_layout(
			swapImg,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{}, // srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput // dstStage
			, vk::ImageAspectFlagBits::eColor
		);
		transition_image_layout(
			colorImg,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite, // srcAccessMask
			vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput // dstStage
			, vk::ImageAspectFlagBits::eColor
		);
		transition_image_layout(
			depthImg,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Clear buffer with clear color
#ifdef MVT_DEBUG
		vk::ClearValue clearColor = vk::ClearColorValue(1.0f, 0.0f, 0.5f, 1.0f);
#else
		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
#endif
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo attachmentInfo = {
			.imageView = colorVw,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.resolveMode        = vk::ResolveModeFlagBits::eAverage,
			.resolveImageView   = swapVw,
			.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor
		};

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = depthVw,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth
		};

		vk::RenderingInfo renderingInfo = {
			.renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo,
		}; {
			commandBuffers[currentFrame].beginRendering(renderingInfo);

			commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
			commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
			commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

			commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});
			commandBuffers[currentFrame].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

			commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[currentFrame], nullptr);
			commandBuffers[currentFrame].drawIndexed(indices_count, 1, 0, 0, 0);

			for (auto& mesh : m_Meshes) {
				commandBuffers[currentFrame].bindVertexBuffers(0, *mesh.m_VertexBuffer, {0});
				commandBuffers[currentFrame].bindIndexBuffer(*mesh.m_IndexBuffer, 0, vk::IndexType::eUint32);

				commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[currentFrame], nullptr);
				commandBuffers[currentFrame].drawIndexed(mesh.indicesCount, 1, 0, 0, 0);
			}

			commandBuffers[currentFrame].endRendering();
		}

		// After rendering, transition the swapchain image to PRESENT_SRC
		transition_image_layout(
			swapImg,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite, // srcAccessMask
			{}, // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eBottomOfPipe // dstStage
			, vk::ImageAspectFlagBits::eColor
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
		vk::Image image,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask,
		vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask,
		vk::PipelineStageFlags2 dstStageMask,
		vk::ImageAspectFlags image_aspect_flags
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
			.image = image, //swapChainImages[imageIndex],
			.subresourceRange = {
				.aspectMask = image_aspect_flags,
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


	void Application::transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, QueueType type, uint32_t mipLevels) {
		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;
		vk::ImageMemoryBarrier barrier{.oldLayout = oldLayout, .newLayout = newLayout, .image = image, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1}};

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
		createDepthResources();
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

	vk::SampleCountFlagBits Application::getMaxUsableSampleCount() const {
		return getMaxUsableSampleCount(physicalDevice);
	}

	vk::SampleCountFlagBits Application::getMaxUsableSampleCount(const vk::PhysicalDevice device) const {
		const vk::PhysicalDeviceProperties physicalDeviceProperties = device.getProperties();

		const vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
		if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
		if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
		if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
		if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
		if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

		return vk::SampleCountFlagBits::e1;

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
