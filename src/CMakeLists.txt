add_library(VulkanRenderer
    instance.cpp
    device_api.cpp 
)

set_target_properties(VulkanRenderer
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

target_compile_options(VulkanRenderer PRIVATE -Wall -Wextra -Werror)

target_link_libraries(VulkanRenderer Vulkan-Headers spirv-reflect-static glfw)

target_include_directories(VulkanRenderer
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(VulkanRenderer PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)