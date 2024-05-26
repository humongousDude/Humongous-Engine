# **Humongous Engine**

The repo for my game-engine, Humongous Engine

This uses Vulkan and C++, and only targets windows currently

**About the Speed of updates**

As this is also my first ever real project, and basically my first time trying to
use Vulkan in a non-tutorial environment, There will sometimes be a large number of days between updates.

So, I'm sorry in advance about the speed of development.

**You can view the projects Trello Here: https://trello.com/b/lc23IYtF/humongous-engine**

### Tiny Note
Sorry about the lack of updates, I'm really struggling with adding frustum culling. Any help would be appreciated!

### Frustum Culling Note
I've decided to give up on Frustum Culling for now. Its been the main reason for the large break, its demotivated me entirely.
All lines related to Frustum culling will be removed for now. I'll return to it when I'm ready


## Dependencies
* Vulkan SDK
* CMake
* Ninja


## Libraries used:
* GLFW3
* GLM
* VMA
* ImGui (trying to use it, but failing miserably)
* TinyGLTF 
* GLI 
* Boost.log


## To Build

**NOTE**

You need to install the above libraries with vcpkg first before trying to build

**You may need to specify the vcpkg root location manually**

I don't know how to automatically find vcpkg, so you may need to manually specify
the location of the root by using `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake`

First, in the outermost CMakeLists.txt directory, run
``` shell
cmake . --preset <preset>
```

**\<preset> is either "release" or "debug"**

**To Install the libraries**

Run `vcpkg install <library>:<install triplet>`

and then
``` shell
./build
```

