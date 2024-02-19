project(vkutil LANGUAGES CXX)

# ----------------
# Project options.
# ----------------

option(VKUTIL_USE_GLFW_WINDOW "Use GLFW window")

if (NOT DEFINED VKUTIL_VK_VERSION)
    set(VKUTIL_VK_VERSION 1000000)
endif()

# ----------------
# Enable standard library module.
# ----------------

file(DOWNLOAD https://github.com/stripe2933/Cpp20Module/raw/main/std.cmake
    ${PROJECT_BINARY_DIR}/std.cmake
)
include(${PROJECT_BINARY_DIR}/std.cmake)

# ----------------
# External dependencies.
# ----------------

if (VKUTIL_USE_GLFW_WINDOW)
    find_package(GLFW3 REQUIRED)
    find_package(glm CONFIG REQUIRED)
endif()
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
CPMAddPackage("gh:stripe2933/VulkanMemoryAllocator-Hpp#master")

# ----------------
# Module configurations for external dependencies.
# ----------------

file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/Vulkan.cmake
    ${PROJECT_BINARY_DIR}/Vulkan.cmake
)
include(${PROJECT_BINARY_DIR}/Vulkan.cmake)

file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/VulkanMemoryAllocator-Hpp.cmake
    ${PROJECT_BINARY_DIR}/VulkanMemoryAllocator-Hpp.cmake
)
include(${PROJECT_BINARY_DIR}/VulkanMemoryAllocator-Hpp.cmake)
target_compile_definitions(VulkanMemoryAllocator-Hpp_module PUBLIC
    VMA_VULKAN_VERSION=${VKUTIL_VK_VERSION}
)

if (VKUTIL_USE_GLFW_WINDOW)
    set(glm_SOURCE_DIR /Users/stripe2933/.vcpkg-clion/vcpkg/installed/arm64-osx/include) # TODO: hard-coded path.
    file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/glm.cmake
        ${PROJECT_BINARY_DIR}/glm.cmake
    )
    include(${PROJECT_BINARY_DIR}/glm.cmake)
endif()

# ----------------
# CMake scripts.
# ----------------

include(cmake/CompileVulkanShader.cmake)

# ----------------
# Project executable.
# ----------------

add_library(vkutil)
target_include_directories(vkutil PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_sources(vkutil PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
        ${PROJECT_SOURCE_DIR}/vkutil.cppm
        ${PROJECT_SOURCE_DIR}/interface/buffer.cppm
        ${PROJECT_SOURCE_DIR}/interface/ref_holder.cppm
        ${PROJECT_SOURCE_DIR}/interface/image.cppm
        ${PROJECT_SOURCE_DIR}/interface/lifetime_holder.cppm
        ${PROJECT_SOURCE_DIR}/interface/pipeline.cppm
        ${PROJECT_SOURCE_DIR}/interface/rendering.cppm
        ${PROJECT_SOURCE_DIR}/interface/transfer.cppm
        ${PROJECT_SOURCE_DIR}/interface/vertex.cppm
)
target_link_libraries(vkutil PUBLIC
    VulkanHppModule
    VulkanMemoryAllocator-Hpp::module
)

if (VKUTIL_USE_GLFW_WINDOW)
    target_sources(vkutil PUBLIC
        FILE_SET CXX_MODULES
        FILES ${PROJECT_SOURCE_DIR}/interface/window.cppm
    )
    target_link_libraries(vkutil PUBLIC
        glfw
        glm::module
    )
    target_compile_definitions(vkutil PRIVATE
        VKUTIL_USE_GLFW_WINDOW
        GLFW_INCLUDE_NONE
    )
endif()