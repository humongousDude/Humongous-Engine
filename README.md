The repo for my game-engine, Humongous Engine

This uses Vulkan and C++, and only targets windows currently

Dependencies:
* Vulkan SDK
* CMake
* Ninja

Libraries used:
* GLFW3
* GLM
* VMA
* ImGui
* TinyObjLoader 
* STB


***To Build***

* NOTE:
You need to install the above libraries with vcpkg first before trying to build
*


First run
``` shell
cmake . --preset debug-llvm-dynamic
```

and then
``` shell
./build
```

