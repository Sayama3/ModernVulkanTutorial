cmake_minimum_required(VERSION 4.0)

project(ModernVulkanTutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT "20")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(CPP_RTTI_ENABLED ON)
set(BUILD_SHARED_LIBS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED slang)

if(NOT VULKAN_SDK)
	set(VULKAN_SDK $ENV{VULKAN_SDK})
endif()


add_subdirectory(Libraries)

set(SOURCES
		Sources/main.cpp
		Sources/Application.cpp
		Includes/MVT/Application.hpp
		Sources/SlangCompiler.cpp
		Includes/MVT/SlangCompiler.hpp
		Includes/MVT/Expected.hpp
		Includes/MVT/GLM.hpp
		Sources/Mesh.cpp
		Includes/MVT/Mesh.hpp
		Sources/VulkanMesh.cpp
		Includes/MVT/VulkanMesh.hpp
		Sources/VmaUsage.cpp
		Sources/VulkanMemoryAllocator.cpp
		Includes/MVT/VulkanMemoryAllocator.hpp
		Sources/VmaBuffer.cpp
		Includes/MVT/VmaBuffer.hpp
		Includes/MVT/UniformBufferObject.hpp
		Sources/STB_ThirdParty.cpp
		Sources/VulkanAllocation.cpp
		Includes/MVT/VulkanAllocation.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_RADIANS=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_ENABLE_EXPERIMENTAL=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_LEFT_HANDED=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC NOMINMAX=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
#target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_HPP_NO_EXCEPTIONS=1)

target_include_directories(${PROJECT_NAME} PUBLIC Includes)
target_include_directories(${PROJECT_NAME} PRIVATE Sources)
target_include_directories(${PROJECT_NAME} PUBLIC ThirdParty)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm-header-only spdlog::spdlog_header_only xxhash)
target_link_libraries(${PROJECT_NAME} PUBLIC assimp::assimp tinyobjloader)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::slang)


target_precompile_headers(${PROJECT_NAME} PUBLIC
		# Multithreading headers
		<thread>
		<atomic>
		<future>
		<condition_variable>
		<mutex>
		<shared_mutex>
		<latch>
		<barrier>

		# Utility stuff
		<functional>
		<utility>
		<algorithm>
		<memory>
		<source_location>
		<chrono>
		<coroutine>
		<bitset>
		<execution>
		<concepts>
		<type_traits>
		<typeinfo>

		# Stream related
		<fstream>
		<iostream>

		# Exception related stuff
		<exception>
		<stdexcept>

		# String & IO manipulation
		<filesystem>
		<string>
		<string_view>

		# Containers
		<deque>
		<array>
		<vector>
		<unordered_set>
		<set>
		<unordered_map>
		<map>
		<any>
		<span>
		<variant>
		<optional>
		<tuple>

		# C-Types Helpers
		<cstdio>
		<cstdlib>
		<cstdint>
		<cstring>
		<cmath>
)

if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
	file(CREATE_LINK ${CMAKE_SOURCE_DIR}/EngineAssets/ ${CMAKE_CURRENT_BINARY_DIR}/EngineAssets/ RESULT copy_result COPY_ON_ERROR SYMBOLIC)
	if(NOT (copy_result EQUAL 0))
		message(WARNING
				"Copy made instead of symlink.\n"
				"${copy_result}"
		)
	endif()
else()
	file(COPY ${CMAKE_SOURCE_DIR}/EngineAssets/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/EngineAssets/)
endif()