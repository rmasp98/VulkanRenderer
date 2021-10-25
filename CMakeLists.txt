cmake_minimum_required(VERSION 3.11)
project(VulkanCpp)

add_subdirectory(Vulkan-Headers)
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)
add_subdirectory(glslang)

set(SPIRV_HEADERS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/SPIRV-Headers/include")
add_subdirectory(Vulkan-ValidationLayers)
add_subdirectory(glfw)

add_executable(VulkanCpp 
    main.cpp 
    window.cpp 
    render_api.cpp 
    device.cpp 
    pipeline.cpp
    command.cpp 
    utils.cpp)

# TODO: Figure out why the below command didn't add the standard flag?
#target_compile_features(VulkanCpp PUBLIC cxx_std_17)
set_target_properties(VulkanCpp
    PROPERTIES
        CXX_STANDARD 17
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

