//
// Created by ianpo on 29/10/2025.
//

#pragma once

#include "GLM.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace MVT {

	struct Vertex {
		glm::vec2 pos;
		glm::vec3 color;

		static vk::VertexInputBindingDescription getBindingDescription(const uint32_t binding = 0) {
			return { binding, sizeof(Vertex), vk::VertexInputRate::eVertex };
		}

		static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions(const uint32_t binding = 0) {
			return {
				vk::VertexInputAttributeDescription( 0, binding, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) ),
				vk::VertexInputAttributeDescription( 1, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) )
			};
		}
	};

	inline const std::vector<Vertex> triangle = {
		{glm::vec2{0.0f, -0.5f}, glm::vec3{1.0f, 1.0f, 1.0f}},
		{glm::vec2{0.5f, 0.5f}, glm::vec3{0.0f, 1.0f, 0.0f}},
		{glm::vec2{-0.5f, 0.5f}, glm::vec3{0.0f, 0.0f, 1.0f}}
	};

	struct Mesh {
	public:
	private:
	};
} // MVT