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

find_package(Vulkan REQUIRED)

if(NOT VULKAN_SDK)
	set(VULKAN_SDK $ENV{VULKAN_SDK})
endif()

## Prefer an explicit override – useful if you have a custom build of slang.
#set(SLANG_ROOT "" CACHE PATH "Path to the Vulkan SDK root containing slang (optional)")
#if(NOT SLANG_ROOT)
#	set(SLANG_ROOT ${VULKAN_SDK})
#endif()
#
#message(STATUS "Searching for Slang in: ${SLANG_ROOT}")
#
## Determine library name based on platform
#if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
#	set(_slang_lib_names slangd libslangd)   # static & import lib (should be for all of linux/macos/window)
#else()
#	set(_slang_lib_names slang  libslang)   # static & import lib (should be for all of linux/macos/window)
#endif()
#
#find_library(
#		SLANG_LIBRARY
#		NAMES ${_slang_lib_names}
#		PATHS "${SLANG_ROOT}/lib"
#		NO_DEFAULT_PATH
#		REQUIRED
#)
#
#if(NOT SLANG_LIBRARY)
#	message(FATAL_ERROR "Could not find Slang library in ${SLANG_ROOT}/lib")
#else()
#	message(STATUS "Found Slang library: ${SLANG_LIBRARY}")
#endif()
#
#add_library(slang::slang STATIC IMPORTED)
#
#set_target_properties(
#		slang::slang PROPERTIES
#		IMPORTED_LOCATION          "${SLANG_LIBRARY}"
#		INTERFACE_INCLUDE_DIRECTORIES "${SLANG_ROOT}/include"
#		INTERFACE_COMPILE_FEATURES   cxx_std_20
#)

add_subdirectory(Libraries)

set(SOURCES
		Sources/main.cpp
		Sources/Application.cpp
		Includes/MVT/Application.hpp
		Sources/SlangCompiler.cpp
		Includes/MVT/SlangCompiler.hpp
		Includes/MVT/Expected.hpp
		Includes/MVT/GLM.hpp
		Includes/MVT/Mesh.cpp
		Includes/MVT/Mesh.hpp
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

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm-header-only spdlog::spdlog_header_only xxhash)
target_link_libraries(${PROJECT_NAME} PUBLIC assimp::assimp tinyobjloader)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui)
target_link_libraries(${PROJECT_NAME} PUBLIC slang)


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