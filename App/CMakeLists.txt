cmake_minimum_required(VERSION 3.26)

add_executable(App main.cpp)

if(MSVC)
    target_compile_options(App PUBLIC /utf-8)
    # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(RUNTIME_LIB_FLAG "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library type")
endif()

# Link the Engine AND the required system libraries to resolve the final errors.
target_link_libraries(App PRIVATE
	Engine
	legacy_stdio_definitions.lib # <--- ADD THIS LINE
)