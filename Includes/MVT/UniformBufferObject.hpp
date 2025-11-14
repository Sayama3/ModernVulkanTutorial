//
// Created by ianpo on 14/11/2025.
//

#pragma once

#include "GLM.hpp"

namespace MVT {
	struct UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};
}