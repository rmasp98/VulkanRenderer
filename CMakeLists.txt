cmake_minimum_required(VERSION 3.14)
project(VulkanCpp)

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
    glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.3.5
)

set(UPDATE_DEPS ON)
FetchContent_MakeAvailable(Vulkan-Headers Vulkan-ValidationLayers glfw)

add_executable(VulkanCpp 
    main.cpp 
    window.cpp 
    render_api.cpp 
    device.cpp 
    pipeline.cpp
    command.cpp 
    utils.cpp
)

# TODO: Figure out why the below command didn't add the standard flag?
#target_compile_features(VulkanCpp PUBLIC cxx_std_17)
set_target_properties(VulkanCpp
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
target_compile_options(VulkanCpp PRIVATE -Wall -Wextra -Werror)
target_link_libraries(VulkanCpp Vulkan-Headers glfw vulkan)
target_include_directories(VulkanCpp
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        ${PROJECT_SOURCE_DIR}
)
target_compile_definitions(VulkanCpp PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

