//
// Created by ianpo on 13/10/2025.
//

#pragma once

#include <complex.h>
#include <SDL3/SDL.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include "MVT/Mesh.hpp"
#include "MVT/VulkanMemoryAllocator.hpp"
#include "MVT/VulkanMesh.hpp"

namespace MVT {
	template<typename Func>
	concept VkQueueFinder = requires(const Func &f, const vk::QueueFamilyProperties &q, uint32_t i)
	{
		{ f(q, i) } -> std::convertible_to<uint32_t>;
	};

	template<typename T, typename Array>
	concept ArrayData = requires(const Array &array)
	{
		{ array.size()->std::template convertible_to<uint64_t> };
		{ array.data()->std::template convertible_to<const T *> };
	};

	struct WindowParameters {
		uint32_t Width;
		uint32_t Height;
		bool Resizable;
	};

	enum class QueueType {
		Graphics,
		Present,
		Transfer,
	};

	class Application {
	public: // Vulkan Specific
		static inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;
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
		void initWindow(const char *windowName, WindowParameters parameters = {1600, 900, true});

		void initVulkan(const char *appName);

		void mainLoop();

		void cleanup();

	private: // High Level Vulkan Specific
		void drawFrame();


		template<class T>
		static vk::VertexInputBindingDescription getBindingDescription(const uint32_t binding = 0) {
			return {binding, sizeof(T), vk::VertexInputRate::eVertex};
		}

	private: // Low Level Vulkan Specific
		void createInstance(const char *appName);

		void setupDebugMessenger();

		bool RatePhysicalDevice(const std::vector<vk::raii::PhysicalDevice>::value_type &device, uint32_t &score);

		void pickPhysicalDevice();

		uint32_t findQueueFamilies(vk::PhysicalDevice device, vk::QueueFlags queueType);


		template<VkQueueFinder Func>
		uint32_t findQueueFamilies(vk::PhysicalDevice device, Func &&pred) {
			// find the index of the first queue family that supports graphics
			std::vector<vk::QueueFamilyProperties> queueFamilyProperties = device.getQueueFamilyProperties();

			for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
				if (pred(queueFamilyProperties[i], i)) {
					return i;
				}
			}

			return queueFamilyProperties.size();
		}

		void createLogicalDevice();

		void createVMA();

		void cleanupVMA();

		void createSurface();

		void createSwapChain();

		void createSwapChainViews();

		void createDescriptorSetLayout();

		void createGraphicsPipeline();

		void createCommandPool();

		void createTextureImage();

		void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory);

		void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory, const std::vector<uint32_t> &families);

		void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory);

		void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory, const std::vector<uint32_t> &families);


		template<uint64_t count>
		inline void createVertexBuffer(const std::array<Vertex, count> &vertices) {
			createVertexBuffer(vertices.data(), count);
		}

		void createVertexBuffer(const std::vector<Vertex> &vertices);

		void waitAndResetFence();

		void createVertexBuffer(const Vertex *vertices, uint64_t count);

		template<uint64_t count>
		void createIndexBuffer(const std::array<uint32_t, count> &indices) { createIndexBuffer(indices.data(), indices.size()); }

		void createIndexBuffer(const std::vector<uint32_t> &indices) { createIndexBuffer(indices.data(), indices.size()); }

		void createIndexBuffer(const uint32_t *indices, uint32_t count);

		void createUniformBuffers();

		void createDescriptorPool();

		void createDescriptorSets();

		template<ArrayData<Vertex> VertexArray, ArrayData<uint32_t> IndiceArray>
		VulkanMesh createVulkanMesh(const VertexArray &vertex, const IndiceArray &indices) {
			return createVulkanMesh(vertex.data(), vertex.size(), indices.data(), indices.size());
		}

		VulkanMesh createVulkanMesh(const Vertex *vertex, uint32_t vCount, const uint32_t *indices, uint32_t iCount);

		void createCommandBuffer();

		void createSyncObjects();

		void recordCommandBuffer(uint32_t imageIndex);

		void updateUniformBuffer(uint32_t currentImage);

		void transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask);

		// void transferBufferQueue(const vk::Buffer &buffer, QueueType oldQueue, QueueType newQueue, vk::PipelineStageFlags src = vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlags dst = vk::PipelineStageFlagBits::eAllCommands);

		void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, QueueType type);

		void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, QueueType queue);

		vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);

		vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);

		vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

		[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const;

		uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

		void copyBuffer(const vk::raii::Buffer &srcBuffer, const vk::raii::Buffer &vk_buffer, vk::DeviceSize size, vk::Fence fence, vk::CommandPool pool, vk::Queue queue);

		void cleanupSwapChain();

		void recreateSwapChain();

		vk::raii::CommandBuffer beginSingleTimeCommands(QueueType type);

		vk::raii::CommandBuffer beginSingleTimeCommands(vk::CommandPool pool);

		void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer, QueueType type, bool fence = false);

		void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer, vk::Queue queue, vk::Fence fence = nullptr);

		[[nodiscard]] uint32_t getFamilyIndex(QueueType type) const;

	private: // Window Specific
		/// Get all the extensions including compatibility layer and stuff like that.
		/// @return Necessary Extensions
		std::vector<const char *> GetExtensions();

		vk::Flags<vk::InstanceCreateFlagBits> GetInstanceFlags();

		std::pair<int, int> GetFramebufferSize();

		std::pair<int, int> GetWindowSize();

	private: // Window Specific
		struct SDL_Window *m_Window{nullptr};
		bool m_ShouldClose = false;

	private: // Vulkan Specicif
		vk::raii::Context context;
		vk::raii::Instance instance = nullptr;
		vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
		vk::raii::PhysicalDevice physicalDevice = nullptr;
		vk::raii::Device device = nullptr;

		VulkanMemoryAllocatorPtr vma;

		uint32_t graphicsFamily{};
		vk::raii::Queue graphicsQueue = nullptr;
		vk::raii::SurfaceKHR surface = nullptr;

		uint32_t presentFamily{};
		vk::raii::Queue presentQueue = nullptr;

		// uint32_t transferFamily{};
		// vk::raii::Queue transferQueue = nullptr;

		vk::Format swapChainImageFormat = vk::Format::eUndefined;
		vk::Extent2D swapChainExtent;
		vk::raii::SwapchainKHR swapChain = nullptr;
		std::vector<vk::Image> swapChainImages;
		std::vector<vk::raii::ImageView> swapChainImageViews;
		vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
		vk::raii::PipelineLayout pipelineLayout = nullptr;
		vk::raii::Pipeline graphicsPipeline = nullptr;

		// vk::raii::CommandPool transfersPool = nullptr;
		// // std::vector<vk::raii::CommandBuffer> transferCommands{};
		vk::raii::Fence transferFence = nullptr;

		vk::raii::CommandPool commandPool = nullptr;

		vk::raii::Image textureImage = nullptr;
		vk::raii::DeviceMemory textureImageMemory = nullptr;

		vk::raii::Buffer vertexBuffer = nullptr;
		vk::raii::DeviceMemory vertexBufferMemory = nullptr;

		vk::raii::Buffer indexBuffer = nullptr;
		vk::raii::DeviceMemory indexBufferMemory = nullptr;

		std::vector<vk::raii::Buffer> uniformBuffers;
		std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
		std::vector<void *> uniformBuffersMapped;

		vk::raii::DescriptorPool descriptorPool = nullptr;
		std::vector<vk::raii::DescriptorSet> descriptorSets;

		bool framebufferResized = false;
		bool windowMinimized = false;

		// Frame in Flights parameters
		uint32_t semaphoreIndex{0};
		uint32_t currentFrame{0};
		std::vector<vk::raii::CommandBuffer> commandBuffers = {};
		std::vector<vk::raii::Semaphore> presentCompleteSemaphores = {};
		std::vector<vk::raii::Semaphore> renderFinishedSemaphores = {};
		//std::vector<vk::raii::Fence> drawFences = {};
		std::vector<vk::raii::Fence> inFlightFences;
	};
} // MVT
