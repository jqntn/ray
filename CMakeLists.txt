cmake_minimum_required(VERSION 3.28)
project(ray C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SPIRV_WERROR OFF)
set(VOLK_PULL_IN_VULKAN OFF)
set(RAY_VULKAN_TAG vulkan-sdk-1.4.363.0)

include(FetchContent)

FetchContent_Declare(
  VulkanHeaders
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(
  VulkanUtilityLibraries
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(
  SPIRV-Headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(
  SPIRV-Tools
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  VulkanValidationLayers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-ValidationLayers.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG ${RAY_VULKAN_TAG}
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.5.1
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL SYSTEM)

FetchContent_MakeAvailable(
  VulkanHeaders VulkanUtilityLibraries SPIRV-Headers SPIRV-Tools glslang
  VulkanValidationLayers volk glfw)

target_link_libraries(volk PUBLIC Vulkan::Headers)

add_executable(ray main.c)
set_target_properties(ray PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
                                     C_EXTENSIONS OFF)
target_compile_definitions(ray PRIVATE GLFW_INCLUDE_VULKAN VK_NO_PROTOTYPES)
target_include_directories(ray PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ray PRIVATE glfw volk)
add_dependencies(ray vvl)

add_custom_target(
  run
  COMMAND ${CMAKE_COMMAND} -E env VK_ADD_LAYER_PATH=$<TARGET_FILE_DIR:vvl>
          $<TARGET_FILE:ray>
  USES_TERMINAL)
add_dependencies(run ray)

foreach(shader triangle.vert triangle.frag)
  add_custom_command(
    OUTPUT ${shader}.inc
    COMMAND glslang-standalone -V --target-env vulkan1.4 -x -o ${shader}.inc
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
    DEPENDS shaders/${shader} glslang-standalone)
  target_sources(ray PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${shader}.inc)
endforeach()
