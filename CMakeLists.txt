cmake_minimum_required(VERSION 3.26 FATAL_ERROR)
project(VulkanInteractiveFractal)

set(CMAKE_CXX_STANDARD 23)

# --------------------
# Enable C++20 module support in CMake.
# You can omit this code when you're using CMake ≥ 3.28.
# --------------------

if (CMAKE_VERSION VERSION_LESS "3.28.0")
    if(CMAKE_VERSION VERSION_LESS "3.27.0")
        set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "2182bf5c-ef0d-489a-91da-49dbc3090d2a")
    else()
        set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "aa1f7df0-828a-4fcd-9afc-2dc80491aca7")
    endif()
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
else()
    cmake_policy(VERSION 3.28)
endif()

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