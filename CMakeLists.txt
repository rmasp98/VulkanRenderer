cmake_minimum_required(VERSION 3.14)
project(VulkanCpp)

# TODO: set vulkan version for consistency
include(FetchContent)
FetchContent_Declare(
    Vulkan-Headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers
  GIT_TAG        v1.2.200
)

FetchContent_Declare(
    Vulkan-ValidationLayers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-ValidationLayers
  GIT_TAG        v1.2.200
)

FetchContent_Declare(
    SPIRV-Reflect
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect
  GIT_TAG        sdk-1.3.216.0
)

# Needed to retrieve dependencies for ValidationLayers
set(UPDATE_DEPS ON)

# SPIRV-Reflection configuration
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
FetchContent_MakeAvailable(Vulkan-Headers Vulkan-ValidationLayers SPIRV-Reflect)

add_subdirectory(src)
add_subdirectory(test)

