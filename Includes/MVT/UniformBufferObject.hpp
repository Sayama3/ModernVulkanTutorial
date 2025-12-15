//
// Created by ianpo on 14/11/2025.
//

#pragma once

#include "GLM.hpp"

namespace MVT {
	struct UniformBufferObject {
		glm::mat4x4 model;
		glm::mat4x4 view;
		glm::mat4x4 proj;
	};
}