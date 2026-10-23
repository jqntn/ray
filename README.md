# ray

Requires CMake 3.28+, Python 3 and a C/C++ compiler. Everything else is fetched and built by CMake.

```sh
cmake -B build
cmake --build build
```

On Linux, GLFW and the Vulkan validation layers also need the X11 and Wayland development packages.
