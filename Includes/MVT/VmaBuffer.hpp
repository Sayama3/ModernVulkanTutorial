//
// Created by ianpo on 05/11/2025.
//

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace MVT {
	class VulkanMemoryAllocator;

	class VmaBuffer {
		friend VulkanMemoryAllocator;
	public:
		VmaBuffer() = default;
		VmaBuffer(VmaAllocator allocator, const VkBufferCreateInfo* pBufferInfo, const VmaAllocationCreateInfo* pAllocInfo);
		~VmaBuffer();

		VmaBuffer(const VmaBuffer& o) = delete;
		VmaBuffer& operator=(const VmaBuffer& o) = delete;

		VmaBuffer(VmaBuffer&& o) noexcept;
		VmaBuffer& operator=(VmaBuffer&& o) noexcept;

	public:
		void swap(VmaBuffer& o) noexcept;

	public:
		vk::Buffer operator*(){return buffer;}
		vk::Buffer* operator->(){return &buffer;}

	public:
		VmaAllocationInfo GetAllocationInfo();

	private:
		vk::Buffer buffer = nullptr;
		VmaAllocator allocator = nullptr;
		VmaAllocation allocation = nullptr;
	};
} // MVT