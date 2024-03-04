# **Humongous Engine**

The repo for my game-engine, Humongous Engine

This uses Vulkan and C++, and only targets windows currently

**About the Speed of updates**

As this is also my first ever real project, and basically my first time trying to
use Vulkan in a non tutorial environment, There will sometimes be a large number of days between updates.

So, I'm sorry in advance about the speed of development.

##You can view the projects Trello Here: https://trello.com/b/lc23IYtF/humongous-engine**

###### also i'm like 14 so i have school lol.

## Dependencies
* Vulkan SDK
* CMake
* Ninja

## Libraries used:
* GLFW3
* GLM
* VMA
* ImGui
* FastGLTF
* GLI 
* Boost.log


## To Build

**NOTE**

You need to install the above libraries with vcpkg first before trying to build

First run
``` shell
cmake . --preset <preset>
```

**Available Presets**
* debug-llvm-dynamic
* release-llvm-dynamic
* debug-llvm-static
* release-llvm-static
* debug-mingw-dynamic
* release-mingw-dynamic
* debug-mingw-static
* release-mingw-static

**To Install the libraries**

Run `vcpkg install <library>:<install triplet>`

and then
``` shell
./build
```

