# ray

Requires CMake 3.28+, Git, Python 3 and a C/C++ compiler. Everything else is fetched and built by CMake.

```sh
cmake -B build
cmake --build build
cmake --build build --target run
```

On Linux, GLFW and the validation layers also need the X11 and Wayland development packages.
