//
// Created by ianpo on 29/10/2025.
//

#include "MVT/Mesh.hpp"
#include <stdexcept>

namespace MVT {
	// VkMesh::VkMesh(vk::raii::Device* pDevice, const Vertex *vertices, uint32_t vertices_count) {
	// 	if (vertices_count % 3u != 0) {
	// 		throw std::runtime_error("Vertex Count is not a multiple of 3.");
	// 	}
	// 	std::vector<uint32_t> indices{vertices_count, ~0u};
	// 	for (uint32_t i = 0; i < vertices_count; ++i) {
	// 		indices[i] = i;
	// 	}
	//
	// 	create_mesh(pDevice, vertices, vertices_count, indices.data(), vertices_count);
	// }
	//
	// VkMesh::VkMesh(vk::raii::Device* pDevice, const Vertex *vertices, uint32_t vertices_count, const uint32_t *indices, uint32_t indices_count) {
	// 	create_mesh(pDevice, vertices, vertices_count, indices, indices_count);
	// }

	VkMesh::~VkMesh() {
		clear();
	}

	VkMesh::VkMesh(VkMesh &&o) noexcept {
		swap(o);
	}

	VkMesh & VkMesh::operator=(VkMesh &&o) noexcept {
		swap(o);
		return *this;
	}

	void VkMesh::swap(VkMesh &o) noexcept {
		std::swap(textures, o.textures);
		// std::swap(uniformBuffers, o.uniformBuffers);
		// std::swap(uniformBuffersMemory, o.uniformBuffersMemory);
		// std::swap(uniformBuffersMapped, o.uniformBuffersMapped);
		std::swap(m_VertexBuffer, o.m_VertexBuffer);
		std::swap(m_VertexMemory, o.m_VertexMemory);
		std::swap(m_IndexBuffer, o.m_IndexBuffer);
		std::swap(m_IndicesMemory, o.m_IndicesMemory);
		std::swap(indicesCount, o.indicesCount);
		std::swap(vertexCount, o.vertexCount);
	}
} // MVT