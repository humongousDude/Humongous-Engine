# **What is this?**
A game engine written in C++ that uses vulkan for rendering. This is a hobby project, so don't expect the greatest code

## Dependencies
* Vulkan SDK
* CMake
* Ninja

## Libraries:
* SDL3
* GLM
* VMA
* ImGui
* TinyGLTF
* GLI
* Spdlog


## Building
You need to install the above libraries with vcpkg first before trying to build

**You may need to specify the vcpkg root location manually**

I don't know how to automatically find vcpkg, so you may need to manually specify
the location of the root by using `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake`
or manually specifiying it in the CMakePresets.json file

First, run
``` bash
cmake . --preset debug
```
replace "debug" with "release" for a release build

then, in the scripts directory, run the corrosponding script for your OS

***For Windows***
```shell
    ./CompileAndRun.bat
```


***For Linux***
```bash
    ./CompileAndRun.sh
```
The engine is developed on Linux, and testing is currently only being done on Linux
