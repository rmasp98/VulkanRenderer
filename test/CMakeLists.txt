FetchContent_Declare(
    glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.3.5
)
FetchContent_MakeAvailable(glfw)

add_executable(VulkanTest
  main.cpp
  window.cpp
)

set_target_properties(VulkanTest
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

target_include_directories(VulkanTest
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(VulkanTest VulkanRenderer glfw)
target_compile_definitions(VulkanTest PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)