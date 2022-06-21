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



# Needed to retrieve dependencies for ValidationLayers
set(UPDATE_DEPS ON)
FetchContent_MakeAvailable(Vulkan-Headers Vulkan-ValidationLayers)

add_subdirectory(src)
add_subdirectory(test)

