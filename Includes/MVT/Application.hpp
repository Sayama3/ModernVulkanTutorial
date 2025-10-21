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
	public:
		void run();
	private:
		void initWindow(const char *windowName, WindowParameters parameters = {1600, 900, false});
		void initVulkan(const char *appName);
		void mainLoop();
		void cleanup();
	private:// Vulkan Specific
		void createInstance(const char *appName);
	private: // Window Specific
		std::pair<const char* const *, uint32_t> GetExtensions();
	private: // Window Specific
		struct SDL_Window *m_Window{nullptr};
		bool m_ShouldClose = false;
	private: // Vulkan Specicif
		vk::raii::Context  context;
		vk::raii::Instance instance = nullptr;
	};
} // MVT