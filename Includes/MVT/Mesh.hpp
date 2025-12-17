//
// Created by ianpo on 29/10/2025.
//

#pragma once


#include "GLM.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace MVT {

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 uv;

		static vk::VertexInputBindingDescription getBindingDescription(const uint32_t binding = 0) {
			return { binding, sizeof(Vertex), vk::VertexInputRate::eVertex };
		}

		static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions(const uint32_t binding = 0) {
			return {
				vk::VertexInputAttributeDescription( 0, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) ),
				vk::VertexInputAttributeDescription( 1, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) ),
				vk::VertexInputAttributeDescription( 2, binding, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv) )
			};
		}
	};

	// inline constexpr std::array triangle = {
	// 	Vertex{glm::vec2{0.0f, -0.5f}, glm::vec3{1.0f, 1.0f, 1.0f}},
	// 	Vertex{glm::vec2{0.5f, 0.5f}, glm::vec3{0.0f, 1.0f, 0.0f}},
	// 	Vertex{glm::vec2{-0.5f, 0.5f}, glm::vec3{0.0f, 0.0f, 1.0f}}
	// };

	inline constexpr std::array rectangle_vertices = {
	Vertex{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	Vertex{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
	Vertex{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	};

	inline constexpr std::array rectangle_indices = {
		0u, 1u, 2u, 2u, 3u, 0u
	};

	inline constexpr std::array two_rectangle_vertices = {
	Vertex{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	Vertex{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		Vertex{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

	Vertex{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	Vertex{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	Vertex{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	Vertex{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
	};

	inline constexpr std::array two_rectangle_indices = {
		0u, 1u, 2u, 2u, 3u, 0u,
		4u, 5u, 6u, 6u, 7u, 4u,
	};

	struct Mesh {
	public:
	private:
	};
} // MVT

