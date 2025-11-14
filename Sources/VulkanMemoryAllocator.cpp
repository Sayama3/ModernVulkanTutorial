//
// Created by ianpo on 05/11/2025.
//

#include "MVT/VulkanMemoryAllocator.hpp"
#include "MVT/VmaBuffer.hpp"

namespace MVT {
	void VulkanMemoryAllocator::Initialize(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device) {
		s_MemoryAllocator = std::make_unique<VulkanMemoryAllocator>(instance, physicalDevice, device);
	}

	void VulkanMemoryAllocator::Shutdown() {
		s_MemoryAllocator.reset();
	}

	VmaAllocator VulkanMemoryAllocator::Get() {
		return s_MemoryAllocator ? s_MemoryAllocator->allocator : VmaAllocator{};
	}

	VulkanMemoryAllocator::VulkanMemoryAllocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device) {
		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
		allocatorCreateInfo.physicalDevice = physicalDevice;
		allocatorCreateInfo.device = device;
		allocatorCreateInfo.instance = instance;
		allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

		vmaCreateAllocator(&allocatorCreateInfo, &allocator);
	}

	VulkanMemoryAllocator::~VulkanMemoryAllocator() {
		vmaDestroyAllocator(allocator);
	}

	void VulkanMemoryAllocator::createBuffer(const VkBufferCreateInfo *pBufferCreateInfo, const VmaAllocationCreateInfo *pAllocationCreateInfo, VmaBuffer *pBuffer) {
		*pBuffer = {pBuffer->allocator, pBufferCreateInfo, pAllocationCreateInfo};
	}
} // MVT