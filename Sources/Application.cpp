//
// Created by ianpo on 13/10/2025.
//

#include "MVT/Application.hpp"

#include <vulkan/vulkan.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <utility>
#include <algorithm>

namespace MVT {

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
		if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}

	Application::Application() {
		initWindow("Modern Vulkan");
		initVulkan("Modern Vulkan");
	}

	Application::~Application() {
		cleanup();
	}

	void Application::run() {
		mainLoop();
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
		createSwapChain();
	}

	void Application::mainLoop() {
		while (!m_ShouldClose) {
			// Handle SDL Events
			{
				SDL_Event e;
				while (SDL_PollEvent(&e) != 0) {
					// ImGui_ImplSDL3_ProcessEvent(&e);
					if (e.type == SDL_EVENT_QUIT) {
						m_ShouldClose = true;
					}

					switch (e.type) {
						default: {
							break;
						};
					}
				}
			}

			// Rest of the code
		}
	}

	void Application::cleanup() {
		swapChain.clear();

		presentQueue.clear();
		graphicsQueue.clear();

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

	void Application::createInstance(const char *appName) {
		vk::ApplicationInfo appInfo{ .pApplicationName   = appName,
			.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
			.pEngineName        = "No Engine",
			.engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
			.apiVersion         = vk::ApiVersion14 };
		// Get the required layers
		std::vector<char const*> requiredLayers;
		if constexpr (enableValidationLayers) {
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties = context.enumerateInstanceLayerProperties();
		if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
			return std::ranges::none_of(layerProperties,
									   [requiredLayer](auto const& layerProperty)
									   { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		}))
		{
			throw std::runtime_error("[Vulkan] One or more required layers are not supported!");
		}

		const auto requiredExtensions = GetExtensions();
		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		// auto [extRes, extensionProperties] = context.enumerateInstanceExtensionProperties();
		// assert(extRes == vk::Result::eSuccess);
		auto extensionProperties = context.enumerateInstanceExtensionProperties();

		for (auto extension : requiredExtensions) {
				if (std::find_if(extensionProperties.begin(), extensionProperties.end(), [&extension](const vk::ExtensionProperties &extensionProperty) { return strcmp(extensionProperty.extensionName, extension) == 0; }) == extensionProperties.end()) {
				throw std::runtime_error("[Vulkan] Required window extension not supported: " + std::string{extension});
			}
		}

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo        = &appInfo,
			.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
			.ppEnabledLayerNames     = requiredLayers.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
			.ppEnabledExtensionNames = requiredExtensions.data()
		};

		// auto [instRes, inst] = context.createInstance(createInfo);
		// assert(instRes == vk::Result::eSuccess);
		// instance = std::move(inst);
		instance = vk::raii::Instance(context, createInfo);

		// {
		// // 	auto [result, exts] = context.enumerateInstanceExtensionProperties();
		// // 	std::cout << "available extensions:\n";
		// 	auto exts = context.enumerateInstanceExtensionProperties();
		// 	std::cout << "available extensions:\n";
		//
		// 	for (const auto& extension : exts) {
		// 		std::cout << '\t' << extension.extensionName << '\n';
		// 	}
		// 	std::cout << std::endl;
		// }
	}

	void Application::setupDebugMessenger() {
		if constexpr (!enableValidationLayers) return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
		vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation );
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

		for (const auto& device : devices) {
			uint32_t score;

			if (!RatePhysicalDevice(device, score)) continue;

			candidates.insert(std::make_pair(score, device));
		}

		// Check if the best candidate is suitable at all
		if (candidates.rbegin()->first > 0) {
			physicalDevice = candidates.rbegin()->second;
		} else {
			throw std::runtime_error("[Vulkan] failed to find a suitable GPU!");
		}
	}

	uint32_t Application::findQueueFamilies(vk::PhysicalDevice device, vk::QueueFlags queueType) {
		// find the index of the first queue family that supports graphics
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = device.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports graphics
		auto graphicsQueueFamilyProperty =
				std::find_if( queueFamilyProperties.begin(),
							  queueFamilyProperties.end(),
							  [queueType]( vk::QueueFamilyProperties const & qfp ) { return qfp.queueFlags & queueType; } );

		return static_cast<uint32_t>( std::distance( queueFamilyProperties.begin(), graphicsQueueFamilyProperty ) );
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

		if ( presentFamily == queueFamilyProperties.size() ) {
			// the graphicsIndex doesn't support present -> look for another family index that supports both
			// graphics and present
			for ( size_t i = 0; i < queueFamilyProperties.size(); i++ )
			{
				if ( ( queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics ) &&
					 physicalDevice.getSurfaceSupportKHR( static_cast<uint32_t>( i ), *surface ) )
				{
					graphicsFamily = static_cast<uint32_t>( i );
					presentFamily  = graphicsFamily;
					break;
				}
			}

			if ( presentFamily == queueFamilyProperties.size() )
			{
				// there's nothing like a single family index that supports both graphics and present -> look for another
				// family index that supports present
				for ( size_t i = 0; i < queueFamilyProperties.size(); i++ )
				{
					if ( physicalDevice.getSurfaceSupportKHR( static_cast<uint32_t>( i ), *surface ) )
					{
						presentFamily = static_cast<uint32_t>( i );
						break;
					}
				}
			}
		}

		if ( ( graphicsFamily == queueFamilyProperties.size() ) || ( presentFamily == queueFamilyProperties.size() ) )
		{
			throw std::runtime_error( "[Vulkan] Could not find a queue for graphics or present -> terminating" );
		}


		std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
		if (graphicsFamily == presentFamily) {
			deviceQueueCreateInfos = {
				vk::DeviceQueueCreateInfo{.queueFamilyIndex = graphicsFamily, .queueCount = 1, .pQueuePriorities = &queuePriority},
			};
		} else {
			deviceQueueCreateInfos = {
				vk::DeviceQueueCreateInfo{.queueFamilyIndex = graphicsFamily, .queueCount = 1, .pQueuePriorities = &queuePriority},
				vk::DeviceQueueCreateInfo{.queueFamilyIndex = presentFamily, .queueCount = 1, .pQueuePriorities = &queuePriority},
			};
		}

		vk::PhysicalDeviceFeatures physicalDeviceFeatures = physicalDevice.getFeatures();

		// Create a chain of feature structures
		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
			{},                               // vk::PhysicalDeviceFeatures2 (empty for now)
			{.dynamicRendering = true },      // Enable dynamic rendering from Vulkan 1.3
			{.extendedDynamicState = true }   // Enable extended dynamic state from the extension
		};

		// Extensions needed for the application to work properly
		std::vector<const char*> deviceExtensions = {
			vk::KHRSwapchainExtensionName,
			vk::KHRSpirv14ExtensionName,
			vk::KHRSynchronization2ExtensionName,
			vk::KHRCreateRenderpass2ExtensionName
		};

		vk::DeviceCreateInfo deviceCreateInfo{
			.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size()),
			.pQueueCreateInfos = deviceQueueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data()
		};

		device = vk::raii::Device( physicalDevice, deviceCreateInfo );
		graphicsQueue = vk::raii::Queue( device, graphicsFamily, 0 );
		presentQueue = vk::raii::Queue( device, presentFamily, 0 );
	}

	void Application::createSurface() {

		VkSurfaceKHR tmpSurface;
		const bool result = SDL_Vulkan_CreateSurface(m_Window, *instance, nullptr, &tmpSurface);

		if (!result) {
			throw std::runtime_error("[SDL] Couldn't create a Vulkan Surface.\n" + std::string(SDL_GetError()));
		}

		surface = {instance, tmpSurface, nullptr};
	}

	vk::SurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				return availableFormat;
			}
		}

		// Backup, the first one should be good enough for my use case if I don't have what I want.
		return availableFormats[0];
	}

	vk::PresentModeKHR Application::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
		// Target PC where energy is not a concern, Mailbox is better if available
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
				return availablePresentMode;
			}
		}

		// Backup, Guaranteed to be available
		return vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D Application::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

		const auto [width, height] = GetFramebufferSize();

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	}

	void Application::createSwapChain() {
		auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( surface );
		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR( surface );
		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR( surface );

		auto swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
		auto swapChainExtent = chooseSwapExtent(surfaceCapabilities);

		auto minImageCount = std::max( 3u, surfaceCapabilities.minImageCount );
		minImageCount = ( surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount ) ? surfaceCapabilities.maxImageCount : minImageCount;

		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{
			.flags = vk::SwapchainCreateFlagsKHR(), .
			surface = surface, .minImageCount = minImageCount,
			.imageFormat = swapChainSurfaceFormat.format, .imageColorSpace = swapChainSurfaceFormat.colorSpace,
			.imageExtent = swapChainExtent, .imageArrayLayers =1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment, .imageSharingMode = vk::SharingMode::eExclusive,
			.preTransform = surfaceCapabilities.currentTransform, .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR( surface )),
			.clipped = true, .oldSwapchain = nullptr };

		uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};

		if (graphicsFamily != presentFamily) {
			swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
			swapChainCreateInfo.queueFamilyIndexCount = 2;
			swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
			swapChainCreateInfo.queueFamilyIndexCount = 0; // Optional
			swapChainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
		}
		swapChain = vk::raii::SwapchainKHR( device, swapChainCreateInfo );
		swapChainImages = swapChain.getImages();

		this->swapChainImageFormat = swapChainSurfaceFormat.format;
		this->swapChainExtent = swapChainExtent;
	}

	std::vector<const char *> Application::GetExtensions() {
		std::vector<const char*> vecExtensions;
		Uint32 extensionCount;
		char const *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

		vecExtensions.reserve(extensionCount + 2);

		vecExtensions.insert(vecExtensions.end(), extensions, extensions + extensionCount);

		if constexpr (enableValidationLayers) {
			vecExtensions.push_back(vk::EXTDebugUtilsExtensionName );
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
