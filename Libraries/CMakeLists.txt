cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(GLM_ENABLE_CXX_20 ON)
set(GLM_ENABLE_CXX_23 ON)
set(GLM_BUILD_LIBRARY OFF)
set(GLM_ENABLE_FAST_MATH ON)
set(GLM_ENABLE_EXPERIMENTAL ON)
FetchContent_Declare( glm
		GIT_REPOSITORY "https://github.com/g-truc/glm.git"
)

set(XXHASH_BUILD_XXHSUM OFF)
FetchContent_Declare(xxHash
		GIT_REPOSITORY "https://github.com/Cyan4973/xxHash.git"
		GIT_TAG "7aee8d0a341bb574f7c139c769e1db115b42cc3c"
		SOURCE_SUBDIR  "build/cmake"
)

# set(SPDLOG_BUILD_PIC ON) # build position independent code
FetchContent_Declare( Spdlog
		GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
		GIT_TAG "v1.15.3"
)

set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
FetchContent_Declare(Assimp
		GIT_REPOSITORY "https://github.com/assimp/assimp.git"
		GIT_TAG "v6.0.2"
)

set(TINYOBJLOADER_USE_DOUBLE OFF) # "Build library with double precision instead of single (float)"
set(TINYOBJLOADER_WITH_PYTHON OFF) # "Build Python module(for developer). Use pyproject.toml/setup.py to build Python module for end-users"
set(TINYOBJLOADER_PREFER_LOCAL_PYTHON_INSTALLATION OFF) # "Prefer locally-installed Python interpreter than system or conda/brew installed Python. Please specify your Python interpreter   with `Python3_EXECUTABLE` cmake option if you enable this option."
FetchContent_Declare(TinyObjLoader
		GIT_REPOSITORY "https://github.com/tinyobjloader/tinyobjloader.git"
		GIT_TAG "release"
)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
set(SDL_VULKAN ON)
set(SDL_RENDER_VULKAN ON)
FetchContent_Declare(SDL3
		GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
		GIT_TAG "release-3.2.24"
)

#set(VMA_BUILD_SAMPLES OFF)
#set(VMA_BUILD_DOCUMENTATION OFF)
#set(VMA_ENABLE_INSTALL OFF)
#FetchContent_Declare(VulkanMemoryAllocator
#		GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator"
#		GIT_TAG "v3.3.0"
#)


set(IMGUI_STDLIB ON)
set(IMGUI_SDL3 ON)
set(IMGUI_VULKAN ON)
FetchContent_Declare(
		IMGUI
		GIT_REPOSITORY "https://github.com/Sayama3/imgui.git"
		GIT_TAG "docking"
)
FetchContent_MakeAvailable(IMGUI)
FetchContent_GetProperties(IMGUI SOURCE_DIR IMGUI_SOURCE_DIR)
get_filename_component(IMGUI_PARENT_DIR "${IMGUI_SOURCE_DIR}" DIRECTORY)

# Configure Slang options before making it available
set(SLANG_EMBED_CORE_MODULE ON)
set(SLANG_EMBED_CORE_MODULE_SOURCE ON)
set(SLANG_LIB_TYPE STATIC)
set(SLANG_ENABLE_TESTS OFF)
set(SLANG_ENABLE_EXAMPLES OFF)
set(SLANG_ENABLE_GFX OFF)
set(SLANG_OVERRIDE_IMGUI_PATH "${CMAKE_BINARY_DIR}")
FetchContent_Declare(slang
		GIT_REPOSITORY "https://github.com/shader-slang/slang.git"
		GIT_TAG "v2025.19.1"
		PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/slang-imgui.patch
)

set(SLANG_OVERRIDE_IMGUI_PATH "${IMGUI_PARENT_DIR}" CACHE PATH "" FORCE)

FetchContent_MakeAvailable(glm xxHash Spdlog Assimp SDL3 TinyObjLoader slang)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
