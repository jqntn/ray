# Rationale

ray is a minimal Vulkan engine in plain C, growing toward a ray/path traced engine for games. This file records the philosophy and the decisions behind the code so future changes stay consistent with them.

## Philosophy

- **Ultra simple.** The smallest code that fully does the job. No abstraction, option or helper until something needs it. Prefer deleting code to adding it.
- **Latest drivers, newest features.** Assume an up-to-date driver and a current GPU. Use the newest core Vulkan (1.4) and newest extensions directly. No fallback paths, no compatibility branches.
- **Fail early and explicitly.** When a requirement is missing, stop at startup with a readable message, never continue into undefined behavior.
- **Standard and portable.** ISO C99, CMake, Khronos components. No platform `#ifdef`s or platform branches in CMake.
- **Self-contained build.** No Vulkan SDK, no package manager, no Git needed to build: CMake fetches and builds everything.

## Language and code style

- **Strict C99** for the `ray` target only: `C_STANDARD 99`, `C_STANDARD_REQUIRED ON`, `C_EXTENSIONS OFF` (`-std=c99` on GCC/Clang; MSVC has no C99 switch). The code must pass a pedantic C99 check: no POSIX or compiler extensions.
- **Formatting:** clang-format with Google style, `DerivePointerAlignment: false` (always `int* p`) and `InsertBraces: true` (braces on every control statement, stricter than Google).
- **Naming (clang-tidy, warnings as errors):** functions and types `UpperCamelCase`; variables, parameters and struct fields `snake_case`; global/static constants and enum values `kName`; macros `UPPER_CASE` with the `RAY_` prefix. Also enforced: `TODO(owner):` format, no redundant boolean comparisons.
- **No comments in code.**
- **LF line endings everywhere** (`.gitattributes`).
- **Errors:** `Check(result, "call")` exits on negative `VkResult`, printing its name via `string_VkResult` (Vulkan-Utility-Libraries). `Fail(message)` for everything else.
- **Fixed-size arrays with explicit limits** instead of heap allocation (`kMaxImages`, GPU, queue family and extension lists), failing if a limit is exceeded.
- **Commits:** atomic, Conventional Commits, subject line only.

## Build

- **CMake 3.28+**, the minimum for `EXCLUDE_FROM_ALL` in `FetchContent_Declare`.
- **All dependencies via FetchContent from GitHub tag archives** (`URL` + `URL_HASH SHA256`), not Git clones: about 23 MB instead of ~450 MB, configure about 3x faster, integrity checked. Bumping a version means updating its hash.
- **Pinned versions:** every Khronos repository and volk at one tag, `RAY_VULKAN_TAG` (`vulkan-sdk-1.4.x`), so headers, tools and layers always match. GLFW pinned separately.
- **FetchContent options:**
  - `EXCLUDE_FROM_ALL`: only what a target needs gets built (no tests, tools or examples).
  - `SYSTEM`: dependency headers produce no warnings and are ignored by clang-tidy.
  - `OVERRIDE_FIND_PACKAGE`: only on packages other dependencies look up with `find_package`, so they resolve to the fetched copy and never to an installed SDK.
- **One set of Vulkan headers:** `VOLK_PULL_IN_VULKAN OFF` and volk linked to the fetched `Vulkan::Headers`, so volk never picks up SDK or system headers.
- **Build requirements:** CMake, Python 3 (code generation inside SPIRV-Tools, glslang and the validation layers) and a C/C++ compiler. Linux also needs the X11 and Wayland development packages for GLFW and the validation layers.
- **First build cost:** most of it is the validation layers and SPIRV-Tools (about 36 CPU-minutes). Normal rebuilds only recompile ray.
- **Windows paths:** MSVC's `cl.exe` and `link.exe` are not long-path aware even with long paths enabled in Windows, so keep the clone and build directories short.
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` so clang-tidy sees the real flags.

## Running and validation

- **`cmake --build build --target run`** is the development entry point. It builds the validation layers (only this target does) and launches ray through `cmake -E env` with:
  - `VK_ADD_LAYER_PATH` pointing at the freshly built layers, the portable alternative to setting environment variables from code;
  - `VK_LOADER_DEBUG=error`, so the Vulkan loader names any missing layer or extension.
- **Validation is requested when `NDEBUG` is not defined.** If the layer is not found (exe launched directly), ray says so and continues without it.
- Messages go to the console; there is no debug messenger.

## Shaders

- **GLSL**, Vulkan's native shading language, compiled at build time by the fetched glslang (`--target-env vulkan1.4`, SPIR-V 1.6).
- **Embedded in the executable** as `uint32_t` arrays (`#include` of generated `.inc` files): no shader files to ship or load.
- HLSL (needs DXC, a large LLVM build) and Slang (large) were rejected for now; switching would only change the compile command.

## Vulkan architecture

- **volk** is a meta-loader in front of the real Vulkan loader (`vulkan-1.dll` / `libvulkan.so.1`): no link to `vulkan-1.lib` (so no SDK), all functions including extensions under their real names, device functions fetched with `vkGetDeviceProcAddr` so per-frame calls bypass the loader.
- **GLFW** creates the window and the surface. It receives volk's `vkGetInstanceProcAddr` through `glfwInitVulkanLoader`, so the Vulkan loader is opened only once. `GLFW_INCLUDE_VULKAN` and `VK_NO_PROTOTYPES` are defined for the whole target, so include order never matters.
- **Required:** Vulkan 1.4 (checked explicitly), `VK_KHR_swapchain`, `VK_KHR_swapchain_maintenance1`, and the instance extensions `VK_KHR_surface_maintenance1` and `VK_KHR_get_surface_capabilities2`.
- **Core features used:** dynamic rendering (no render passes or framebuffers), synchronization2, extended dynamic state (viewport and scissor with count), maintenance5 (SPIR-V passed directly to the pipeline, no `VkShaderModule` objects).
- **GPU:** the first discrete GPU, otherwise the first GPU. One queue from a family supporting graphics and presentation.
- **Frame loop:** one frame in flight with one acquire semaphore, one render semaphore and two fences (last submit, last present). Each frame starts by waiting on both fences. On resize (GLFW framebuffer callback, out-of-date or suboptimal results) the swapchain is recreated and the old one destroyed immediately, because both fences prove the GPU and the presentation engine are done with it. No `vkDeviceWaitIdle`. A minimized window waits for events.
- **Swapchain:** FIFO present mode (vsync, guaranteed everywhere, power-efficient), `minImageCount + 1` images, the first surface format, opaque composition. MAILBOX only if latency ever matters more than power.
- **The window starts maximized.**

## Decisions considered and rejected

| Option | Why not |
|---|---|
| Installing the Vulkan SDK | The most standard and minimal setup, but contradicts the self-contained build goal. |
| Linking the loader directly | Requires the SDK's `vulkan-1.lib`. |
| Hand-rolled function loading | One lookup line per function, growing with the engine; volk is two files. |
| vk-bootstrap | C++ only. |
| Setting `VK_ADD_LAYER_PATH` in code | Needed `_putenv_s`/`setenv` platform branches and baked an absolute build path into the binary. |
| `VK_EXT_shader_object` | Solves pipeline-combination explosions that a path tracer does not have: compute shaders carry no draw state, ray tracing shaders are not supported, materials live in data. |
| Fallback paths for missing extensions | Contradict "latest drivers"; the swapchain_maintenance1 fallback was removed. |
| EXT variants of promoted extensions | KHR only. |
| Mozilla code style | Its formatting is Google plus mandatory braces (kept); Google naming is complete and fully enforceable by clang-tidy. |

## Direction: ray/path tracing for games

- **Tracing:** `VK_KHR_ray_query` in compute shaders, over `VK_KHR_acceleration_structure` (bottom-level per mesh, top-level per scene). Needs buffer device address and descriptor indexing (core). The ray tracing pipeline only if specialized hit shaders become necessary.
- **First milestone:** a ray-traced triangle. One compute shader traces one ray per pixel into a storage image, which is copied to the swapchain image. The window, swapchain, synchronization and resize handling stay.
- **Scenes:** glTF meshes loaded with cgltf (single-file C99). PBR (metallic-roughness) evaluated in one shader: hit data from barycentrics and vertex buffers, materials as data, textures in one bindless array.
- **Transparency:** cutout and stochastic alpha in the ray query candidate loop; glass and water as a BSDF (Fresnel, Snell, Beer-Lambert absorption; glTF transmission, IOR and volume); thin translucency as diffuse transmission; subsurface scattering as a random walk or an approximation. Shadow rays may pass tinted through thin glass as a real-time compromise.
- **Real time:** temporal accumulation, denoising (SVGF or ReSTIR-style sampling) and upscaling are where most of the work will be.
- **Dense geometry:** meshlet-style clusters map to cluster acceleration structures (`VK_NV_cluster_acceleration_structure`), NVIDIA-only at the time of writing. Rasterization (mesh shaders into a visibility buffer) only if a hybrid pass is ever needed, kept to a handful of pipelines.

## Known limitations

- Tested only on Windows with MSVC and Ninja. Linux is untested.
- macOS is not supported: MoltenVK cannot reasonably be fetched with CMake, and the portability enumeration setup is absent.
- Validation runs only through the `run` target.
- The image stretches with the window (no aspect-ratio correction).
- Dependency hashes rely on GitHub keeping tag archives byte-identical.
