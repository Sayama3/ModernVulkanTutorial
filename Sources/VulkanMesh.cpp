//
// Created by ianpo on 05/11/2025.
//

#include "MVT/VulkanMesh.hpp"

#define MVT_ALIGN_SIZE(size, alignement) (size % alignment == 0 ? size : ((size / alignment) + 1) * alignment)

namespace MVT {

	VulkanMesh::VulkanMesh(vk::raii::DeviceMemory memory, vk::raii::Buffer vertexBuffer, vk::raii::Buffer indexBuffer) : memory(std::move(memory)), vertexBuffer(std::move(vertexBuffer)), indexBuffer(std::move(indexBuffer))
	{
	}

	VulkanMesh::~VulkanMesh() = default;
} // MVT