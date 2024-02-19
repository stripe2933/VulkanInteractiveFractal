project(vkbase LANGUAGES CXX)

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

find_package(Vulkan REQUIRED)

# ----------------
# Module configurations for external dependencies.
# ----------------

file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/Vulkan.cmake
    ${PROJECT_BINARY_DIR}/Vulkan.cmake
)
include(${PROJECT_BINARY_DIR}/Vulkan.cmake)

# ----------------
# Project executable.
# ----------------

add_library(vkbase)
target_sources(vkbase PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
        ${PROJECT_SOURCE_DIR}/vkbase.cppm
        ${PROJECT_SOURCE_DIR}/interface/builders.cppm
        ${PROJECT_SOURCE_DIR}/interface/details/range_utils.cppm
        ${PROJECT_SOURCE_DIR}/interface/details/ref_holder.cppm
        ${PROJECT_SOURCE_DIR}/interface/primitives.cppm
)
target_link_libraries(vkbase PUBLIC
    VulkanHppModule
)