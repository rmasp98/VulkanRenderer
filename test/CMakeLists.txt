FetchContent_Declare(
    glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.3.5
)

FetchContent_Declare(
    glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glfw glm)



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

target_link_libraries(VulkanTest VulkanRenderer glfw glm)
target_compile_definitions(VulkanTest PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

# Stuff to automatically compile shaders
# file(GLOB_RECURSE GLSL_SOURCE_FILES
#     "${PROJECT_SOURCE_DIR}/shaders/*.frag"
#     "${PROJECT_SOURCE_DIR}/shaders/*.vert"
#     "${PROJECT_SOURCE_DIR}/shaders/*.comp"
#     )

# foreach(GLSL ${GLSL_SOURCE_FILES})
#   message(STATUS "BUILDING SHADER")
#   get_filename_component(FILE_NAME ${GLSL} NAME)
#   set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
#   message(STATUS ${GLSL})
#   add_custom_command(
#     OUTPUT ${SPIRV}
#     COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
#     DEPENDS ${GLSL})
#   list(APPEND SPIRV_BINARY_FILES ${SPIRV})
# endforeach(GLSL)

# add_custom_target(
#     Shaders 
#     DEPENDS ${SPIRV_BINARY_FILES}
#     )