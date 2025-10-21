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
	void Application::run() {
		initWindow("Modern Vulkan");
		initVulkan("Modern Vulkan");
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
		if (m_Window) {
			SDL_DestroyWindow(m_Window);
			m_Window = nullptr;
		}
	}

	void Application::createInstance(const char *appName) {
		constexpr vk::ApplicationInfo appInfo{"Hello Triangle", VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0), vk::ApiVersion14};

		const auto [extensions, count] = GetExtensions();
		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		// auto [extRes, extensionProperties] = context.enumerateInstanceExtensionProperties();
		// assert(extRes == vk::Result::eSuccess);
		auto extensionProperties = context.enumerateInstanceExtensionProperties();

		for (uint64_t i = 0; i < count; ++i) {
			const char *const extension = extensions[i];

			if (std::find_if(extensionProperties.begin(), extensionProperties.end(), [&extension](const vk::ExtensionProperties &extensionProperty) { return strcmp(extensionProperty.extensionName, extension) == 0; }) == extensionProperties.end()) {
				throw std::runtime_error("Required window extension not supported: " + std::string{extension});
			}
		}

		vk::InstanceCreateInfo createInfo{
			{},
			&appInfo,
			count,
			extensions
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

	std::pair<const char * const *, uint32_t> Application::GetExtensions() {
		Uint32 extensionCount;
		char const *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
		return {extensions, extensionCount};
	}
} // MVT
