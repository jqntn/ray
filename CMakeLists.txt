cmake_minimum_required(VERSION 3.28)
project(ray C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SPIRV_WERROR OFF)
set(VOLK_PULL_IN_VULKAN OFF)
set(RAY_VULKAN_TAG vulkan-sdk-1.4.363.0)

include(FetchContent)

FetchContent_Declare(
  VulkanHeaders
  URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e
  EXCLUDE_FROM_ALL SYSTEM OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(
  VulkanUtilityLibraries
  URL https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=a5308b62e3bac84cf45c4d7dd5b830aabace4847fdb71f4fd7db3be034c2bebf
  EXCLUDE_FROM_ALL SYSTEM OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(
  SPIRV-Headers
  URL https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=5b3a0d099f5c7649e29c0938476c462e71d66ad89a0bde42fa1ba3eb476956c9
  EXCLUDE_FROM_ALL SYSTEM OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(
  SPIRV-Tools
  URL https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=e6c83a215538fbfd265ccf398d3877985055df1273dbed73912bd05307664688
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  glslang
  URL https://github.com/KhronosGroup/glslang/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=cda24ec765e6d2845d106b7caa593f34127195a88524cf87821ff0088fb6edec
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  VulkanValidationLayers
  URL https://github.com/KhronosGroup/Vulkan-ValidationLayers/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=36720627913a591ae8ffc0f24b3c04f98af65242c9b7215d1944a85ff5b70e00
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  volk
  URL https://github.com/zeux/volk/archive/refs/tags/${RAY_VULKAN_TAG}.tar.gz
  URL_HASH SHA256=1547d8d74395d4048fb3f4a6313da56db626e89b55db238d0d7e8944c8a645f3
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/archive/refs/tags/3.5.1.tar.gz
  URL_HASH SHA256=5234f4f29473e9a06bc7847d8371858dd135d38466eeeaa652fdc9f8f9ff0c20
  EXCLUDE_FROM_ALL SYSTEM)

FetchContent_MakeAvailable(
  VulkanHeaders VulkanUtilityLibraries SPIRV-Headers SPIRV-Tools glslang
  VulkanValidationLayers volk glfw)

target_link_libraries(volk PUBLIC Vulkan::Headers)

add_executable(ray src/main.c)
set_target_properties(ray PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
                                     C_EXTENSIONS OFF)
target_compile_definitions(ray PRIVATE GLFW_INCLUDE_VULKAN VK_NO_PROTOTYPES)
target_include_directories(ray PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ray PRIVATE glfw volk)

add_custom_target(
  run
  COMMAND ${CMAKE_COMMAND} -E env VK_ADD_LAYER_PATH=$<TARGET_FILE_DIR:vvl>
          $<TARGET_FILE:ray>
  USES_TERMINAL)
add_dependencies(run ray vvl)

foreach(shader triangle.vert triangle.frag)
  add_custom_command(
    OUTPUT ${shader}.inc
    COMMAND glslang-standalone -V --target-env vulkan1.4 -x -o ${shader}.inc
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
    DEPENDS shaders/${shader} glslang-standalone)
  target_sources(ray PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${shader}.inc)
endforeach()
