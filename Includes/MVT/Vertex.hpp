//
// Created by ianpo on 30/12/2025.
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
			return {binding, sizeof(Vertex), vk::VertexInputRate::eVertex};
		}

		static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions(const uint32_t binding = 0) {
			return {
				vk::VertexInputAttributeDescription(0, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
				vk::VertexInputAttributeDescription(1, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
				vk::VertexInputAttributeDescription(2, binding, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv))
			};
		}

		bool operator==(const Vertex &other) const {
			return pos == other.pos && color == other.color && uv == other.uv;
		}

		bool operator!=(const Vertex &other) const {
			return !(*this == other);
		}
	};
}

namespace std {
	template<>
	struct hash<MVT::Vertex> {
		size_t operator()(MVT::Vertex const &vertex) const noexcept {
			return ((hash<glm::vec3>()(vertex.pos) ^
					 (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				   (hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}
