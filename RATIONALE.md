# Rationale

ray is a minimal Vulkan engine in plain C. Its goal is a ray/path traced engine for games. This file records the design rules. Follow them in every change.

## Principles

- **Ultra simple.** Write the smallest code that does the job. Add an abstraction, option or helper only when code needs it. Prefer deletion to addition.
- **Latest drivers.** Assume a current GPU and an up-to-date driver. Use Vulkan 1.4 and the newest extensions directly, with no fallback paths.
- **Fail early and explicitly.** Stop at startup with a readable message when a requirement is missing. Never continue into undefined behavior. The validation layer is the one exception: it is a development aid, so ray continues without it.
- **Portable.** Use ISO C99, CMake and Khronos components. Do not branch on the platform in code or CMake.
- **Self-contained.** CMake fetches and builds every dependency. The build needs no Vulkan SDK, no package manager and no Git.

## Code

- **Strict C99.** The `ray` target sets `C_STANDARD 99` with extensions off. The code must pass a pedantic C99 check. MSVC has no C99 switch, so CMake passes no flag there.
- **Format.** clang-format uses Google style, left pointer alignment (`int* p`) and braces on every control statement. Files use LF line endings.
- **Names.** clang-tidy enforces Google naming as errors: `UpperCamelCase` functions and types, `snake_case` variables and fields, `kName` constants and enum values, `RAY_` macros.
- **No comments in code.**
- **Errors.** `Check` stops on a negative `VkResult` and prints its name. `Fail` stops with a message. `Allocate` stops when `malloc` fails.
- **Limits.** Use fixed-size arrays with explicit limits, not heap allocation. Stop when a limit is too small. Exception: lists whose length comes from Vulkan or GLFW use a heap array of that length. These are the physical devices, queue families and swapchain images (the usual two-call pattern), and the instance extensions.
- **Commits.** Make atomic commits with a Conventional Commits subject line only.

## Build

- **CMake 3.28+.** It is the first version with `EXCLUDE_FROM_ALL` in `FetchContent_Declare`.
- **Archives, not Git.** Each dependency comes from a GitHub tag archive with a SHA-256 hash. This downloads about 23 MB instead of about 450 MB and checks integrity. A version bump needs a new hash.
- **One Vulkan version.** All Khronos repositories and volk use one tag, `RAY_VULKAN_TAG`. Headers, tools and layers then always match.
- **FetchContent options.**
  - `EXCLUDE_FROM_ALL` builds only what a target needs.
  - `SYSTEM` hides warnings from dependency headers, also from clang-tidy.
  - `OVERRIDE_FIND_PACKAGE` makes `find_package` find the fetched copy, never an installed SDK.
- **One set of headers.** volk links the fetched `Vulkan::Headers` (`VOLK_PULL_IN_VULKAN OFF`).
- **Requirements.** CMake, Python 3 and a C/C++ compiler. SPIRV-Tools, glslang and the validation layers generate code with Python. Linux also needs the X11 and Wayland development packages.
- **Cost.** The first build spends most of its time on the validation layers and SPIRV-Tools (about 36 CPU-minutes). Later builds compile only ray.
- **Short paths on Windows.** MSVC `cl.exe` and `link.exe` do not support long paths. Keep the clone and build directories short.
- **Tooling.** CMake exports `compile_commands.json` for clang-tidy. clang-tidy also rejects `RAY_COUNT` on a pointer (`bugprone-sizeof-expression`).

## Run

- Use `cmake --build build --target run-vvl` during development. Only this target builds the validation layers. It starts ray through `cmake -E env` with `VK_ADD_LAYER_PATH`, which points the loader at the built layers, and `VK_INSTANCE_LAYERS`, which enables validation in any build type. This replaces platform-specific code and layer code in ray.
- A debug messenger is active in every build, however ray starts. It prints loader and validation errors. Its settings are also chained into `vkCreateInstance`, so it catches a missing layer or extension.
- After a validation error, ray stops at the start of the next frame. An error in the last frame or during teardown makes ray fail at exit. Do not exit inside the callback, because the process can hang in the driver.

## Shaders

- **GLSL**, Vulkan's native language. The fetched glslang compiles it at build time to SPIR-V 1.6 (`--target-env vulkan1.4`).
- **Embedded.** ray stores the SPIR-V as `uint32_t` arrays. No shader files ship.
- **Not HLSL or Slang.** HLSL needs DXC and Slang is large. A switch changes only the compile command.

## Vulkan

- **volk** opens the Vulkan loader and loads every function pointer. ray needs no `vulkan-1.lib`, so no SDK. Device functions bypass the loader.
- **GLFW** creates the window and the surface. It uses volk's `vkGetInstanceProcAddr` (`glfwInitVulkanLoader`), so ray opens the loader once. The target defines `GLFW_INCLUDE_VULKAN` and `VK_NO_PROTOTYPES`, so include order does not matter.
- **Requirements.** Vulkan 1.4 (checked at startup), `VK_KHR_swapchain`, `VK_KHR_swapchain_maintenance1`, `VK_KHR_surface_maintenance1`, `VK_KHR_get_surface_capabilities2` and `VK_EXT_debug_utils`.
- **Features.** Dynamic rendering, synchronization2, extended dynamic state and maintenance5. With maintenance5, the pipeline takes SPIR-V directly and ray creates no shader modules.
- **GPU.** ray uses the first discrete GPU, or else the first GPU. One queue does graphics and presentation.
- **Frame loop.** One frame is in flight. Each frame starts with a wait on two fences: the last submit and the last present. After a resize, ray recreates the swapchain and destroys the old one at once, because the fences prove that nothing uses it. ray never calls `vkDeviceWaitIdle`. While the window is minimized, ray waits for events.
- **Swapchain.** FIFO present mode (vsync, available everywhere, power-efficient), `minImageCount + 1` images, the first surface format and opaque composition. The presentation engine keeps `minImageCount` images, so the spare one lets an acquire return at once. The image count is independent of the frames in flight.
- **Window.** It opens maximized.

## Rejected

| Option | Reason |
|---|---|
| Vulkan SDK | The most standard setup, but the build must be self-contained. |
| Linking the loader | Needs the SDK's `vulkan-1.lib`. |
| Hand-written function loading | One line per function. volk is two files. |
| vk-bootstrap | C++ only. |
| Setting `VK_ADD_LAYER_PATH` in code | Needs platform branches and puts a build path in the binary. |
| `VK_EXT_shader_object` | A path tracer has no pipeline explosion. Compute shaders have no draw state, and the extension does not support ray tracing shaders. |
| Fallbacks for missing extensions | They contradict "latest drivers". |
| EXT names of promoted extensions | Current drivers expose the KHR name. Both names need extra code for old drivers only. |
| Mozilla style | Google naming is complete, and clang-tidy enforces all of it. ray keeps the mandatory braces from Mozilla. |

## Deferred

These options are not rejected. Each waits for its trigger. Direction and Limitations list the other deferred items.

| Option | Trigger |
|---|---|
| MAILBOX present mode | Latency matters more than power. MAILBOX renders frames that the screen never shows, and not every platform has it. |
| Two frames in flight | The CPU waits idle on GPU work, for example once TLAS updates and uploads run every frame. A GPU timestamp profile shows the gap. |
| More swapchain_maintenance1 features | ray needs a vsync toggle, scaling during a resize or swapchain memory on first use. |
| Object names and command labels (`VK_EXT_debug_utils`) | ray has many resources and passes, for example in the path tracer. |
| Validation in the IDE debugger | You debug in an IDE. Today only the `run-vvl` target enables validation. |
| `CMAKE_INTERMEDIATE_DIR_STRATEGY SHORT` | A build hits the Windows path limit again. |
| GPU choice by Vulkan 1.4 support | A discrete GPU without Vulkan 1.4 hides an integrated GPU with it. |

## Direction: path tracing

- **Tracing.** Use `VK_KHR_ray_query` in compute shaders over `VK_KHR_acceleration_structure`: one bottom-level structure per mesh, one top-level structure per scene. Both need buffer device address and descriptor indexing. Add the ray tracing pipeline only if hit shaders must differ.
- **First step.** Trace a triangle. A compute shader traces one ray per pixel into a storage image, and a tonemap pass writes the image to the swapchain.
- **Frame.** Tracing writes ray's own HDR images: accumulation, history and denoiser input. Only the last pass touches the swapchain: a fullscreen triangle tonemaps into it with the graphics pipeline. This also avoids storage usage on the swapchain format, which drivers do not guarantee. ray acquires the swapchain image just before that pass, and only that pass waits on the acquire semaphore, so tracing never waits for the display.
- **Present.** FIFO stays. A path tracer usually runs below the refresh rate, so MAILBOX gains nothing. To converge faster while the camera is still, run several sample passes per presented frame within a time budget. Measure with GPU timestamps (`vkCmdWriteTimestamp2`), not the frame rate.
- **Frames in flight.** Two when the trigger in Deferred fires, not three: a third adds latency and no throughput while the GPU is the bottleneck. Only what the CPU writes or tracks per frame doubles: the command buffer, the camera and TLAS instance uploads, the acquire semaphore and the fences. GPU-only images (accumulation, history, reservoirs) stay single, because one queue already runs frames in order. Index per-frame resources by frame slot, never by swapchain image index. A timeline semaphore can replace the submit fences.
- **Scenes.** Load glTF with cgltf (single-file C99). Evaluate PBR in one shader. Materials are data, and all textures sit in one bindless array.
- **Transparency.** Handle cutout and partial alpha in the ray query candidate loop. Treat glass and water as a BSDF with Fresnel, refraction and absorption. Use diffuse transmission for thin surfaces and a random walk for subsurface scattering.
- **Real time.** Most of the work is temporal accumulation, denoising and upscaling.
- **Dense geometry.** Clusters (meshlets) map to `VK_NV_cluster_acceleration_structure`, NVIDIA-only at the time of writing. Rasterize only for a hybrid pass, with few pipelines.

## Limitations

- Tested only on Windows with MSVC and Ninja. Linux is not tested.
- macOS does not work. CMake cannot reasonably fetch MoltenVK, and the code has no portability setup.
- Validation runs only through the `run-vvl` target.
- The image stretches with the window.
- The hashes depend on GitHub archives staying byte-identical.
