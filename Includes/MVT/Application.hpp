//
// Created by ianpo on 13/10/2025.
//

#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace MVT {

	struct WindowParameters {
		uint32_t Width;
		uint32_t Height;
		bool Resizable;
	};

	class Application {
	public: // Vulkan Specific
		static inline const std::vector validationLayers = {
			"VK_LAYER_KHRONOS_validation",
		};

#ifdef NDEBUG
		static inline constexpr bool enableValidationLayers = false;
#else
		static inline constexpr bool enableValidationLayers = true;
#endif

	public:
		Application();
		~Application();
		void run();
	private:
		void initWindow(const char *windowName, WindowParameters parameters = {1600, 900, false});


		void initVulkan(const char *appName);
		void mainLoop();
		void cleanup();
	private:// Vulkan Specific
		void createInstance(const char *appName);
		void setupDebugMessenger();

		bool RatePhysicalDevice(const std::vector<vk::raii::PhysicalDevice>::value_type &device, uint32_t &score);
		void pickPhysicalDevice();
		uint32_t findQueueFamilies(vk::PhysicalDevice device, vk::QueueFlags queueType);
		void createLogicalDevice();
		void createSurface();

	private: // Window Specific
		/// Get all the extensions including compatibility layer and stuff like that.
		/// @return Necessary Extensions
		std::vector<const char*> GetExtensions();

		vk::Flags<vk::InstanceCreateFlagBits> GetInstanceFlags();
	private: // Window Specific
		struct SDL_Window *m_Window{nullptr};
		bool m_ShouldClose = false;
	private: // Vulkan Specicif
		vk::raii::Context  context;
		vk::raii::Instance instance = nullptr;
		vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
		vk::raii::PhysicalDevice physicalDevice = nullptr;
		vk::raii::Device device = nullptr;
		vk::raii::Queue graphicsQueue = nullptr;
		vk::raii::SurfaceKHR surface = nullptr;
		vk::raii::Queue presentQueue = nullptr;
	};
} // MVT