//
// Created by ianpo on 05/11/2025.
//

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>

#include "MVT/Mesh.hpp"

namespace MVT {
	class VulkanMesh {
	public:
		VulkanMesh() = default;
		// VulkanMesh(vk::raii::Device& device, const Vertex* vertex, uint32_t vCount, const uint32_t* indices, uint32_t iCount, uint32_t graphicsFamily, uint32_t transferFamily);
		VulkanMesh( vk::raii::DeviceMemory memory, vk::raii::Buffer vertexBuffer, vk::raii::Buffer indexBuffer);
		~VulkanMesh();
	public:
		vk::raii::DeviceMemory memory = nullptr;
		vk::raii::Buffer vertexBuffer = nullptr;
		vk::raii::Buffer indexBuffer = nullptr;
	};
} // MVT