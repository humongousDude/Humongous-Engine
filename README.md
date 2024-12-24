# **Humongous Engine**

UPDATE: sorry for large delays between updates, currently learning linear algebra

The repo for my game-engine, Humongous Engine

This uses Vulkan and C++, and only targets windows currently

## Dependencies
* Vulkan SDK
* CMake
* Ninja


## Libraries used:
* SDL3
* GLM
* VMA
* ImGui
* TinyGLTF
* GLI
* Spdlog


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

