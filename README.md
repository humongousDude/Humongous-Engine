The repo for my game-engine, Humongous Engine

This uses Vulkan and C++, and only targets windows currently

*** About the Speed of updates ***
As this is also my first ever real project, and basically my first time trying to
use Vulkan in a non tutorial environment, There will sometimes be a large number of days between updates.

So, I'm sorry in advance about the speed of development.

also i'm like 14 so i have school lol.

***Dependencies***
* Vulkan SDK
* CMake
* Ninja

Libraries used:
* GLFW3
* GLM
* VMA
* ImGui
* FastGLTF
* GLI 


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

