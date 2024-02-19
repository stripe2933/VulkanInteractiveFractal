cmake_minimum_required(VERSION 3.28)
project(VulkanInteractiveFractal)

set(CMAKE_CXX_STANDARD 23)

# ----------------
# Enable standard library module.
# ----------------

file(DOWNLOAD https://github.com/stripe2933/Cpp20Module/raw/main/std.cmake
    ${CMAKE_BINARY_DIR}/std.cmake
)
include(${CMAKE_BINARY_DIR}/std.cmake)

# ----------------
# Install CPM.cmake.
# ----------------

file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/cpm.cmake
    ${CMAKE_BINARY_DIR}/CPM.cmake
)
include(${CMAKE_BINARY_DIR}/CPM.cmake)

# ----------------
# Internal dependencies.
# ----------------

add_subdirectory(vkbase)
set(VKUTIL_USE_GLFW_WINDOW ON)
add_subdirectory(vkutil)

# Enable Dynamic dispatch loader in VulkanHppModule.
target_compile_definitions(VulkanHppModule PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

# ----------------
# Project executable.
# ----------------

add_executable(VulkanInteractiveFractal main.cpp)
target_link_libraries(VulkanInteractiveFractal PRIVATE
    vkbase
    vkutil
)

# ----------------
# Compile shaders.
# ----------------

compile_shader(VulkanInteractiveFractal
    FORMAT spv
    SOURCES
        shaders/fractal.comp
)