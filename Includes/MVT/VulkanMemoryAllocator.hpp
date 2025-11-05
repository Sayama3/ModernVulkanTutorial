//
// Created by ianpo on 05/11/2025.
//

#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace MVT {
	class VmaBuffer;

	class VulkanMemoryAllocator {
	public: // Static
		static void Initialize(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

		static void Shutdown();

		static VmaAllocator Get();

	private: // Static
		inline static std::unique_ptr<VulkanMemoryAllocator> s_MemoryAllocator{nullptr};

	public: // Members
		VulkanMemoryAllocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

		~VulkanMemoryAllocator();

		VulkanMemoryAllocator(const VulkanMemoryAllocator &) = delete;

		VulkanMemoryAllocator &operator=(const VulkanMemoryAllocator &) = delete;

		VulkanMemoryAllocator(VulkanMemoryAllocator &&) noexcept = delete;

		VulkanMemoryAllocator &operator=(VulkanMemoryAllocator &&) noexcept = delete;

	public: // Members
		void createBuffer(const VkBufferCreateInfo *pBufferCreateInfo, const VmaAllocationCreateInfo *pAllocationCreateInfo, VmaBuffer *pBuffer);

	public: // Members
		VmaAllocator allocator;
	};

	using VulkanMemoryAllocatorPtr = std::unique_ptr<VulkanMemoryAllocator>;
} // MVT
