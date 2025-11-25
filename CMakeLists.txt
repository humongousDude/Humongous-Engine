cmake_minimum_required(VERSION 3.26)
project(HumongousEngine LANGUAGES CXX C)

include(FetchContent)
FetchContent_Declare(
    SDL3
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/SDL"
)

FetchContent_Declare(
    spdlog
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/spdlog"
)

FetchContent_Declare(
    OpenAL32 
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/openal-soft"
)

FetchContent_Declare(
    sndfile 
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/libsndfile"
)

FetchContent_Declare(
    eigen
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/eigen"
)

FetchContent_Declare(
    MeshOptimizer
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/meshoptimizer"
)

FetchContent_Declare(
    GoogleTest
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/googletest"
)

FetchContent_Declare(
    Ktx
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/KTX-Software"
    CMAKE_ARGS  -DCMAKE_CXX_STANDARD=17
                -DCMAKE_CXX_STANDARD_REQUIRED=ON
)

FetchContent_Declare(
    VMA
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/VulkanMemoryAllocator"
)

FetchContent_Declare(
    tinygltf
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/tinygltf"
)

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE) 
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) 

set(LIBTYPE "SHARED" CACHE STRING "" FORCE) 
set(ALSOFT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS_CTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 spdlog OpenAL32 Sndfile eigen meshoptimizer GoogleTest Ktx GoogleTest VMA tinygltf)

function(suppress_warnings_recursive_impl TARGET_DIR)
    get_property(is_defined DIRECTORY "${TARGET_DIR}" PROPERTY LISTFILE_STACK SET)
    if(NOT is_defined)
        return()
    endif()

    get_property(targets DIRECTORY "${TARGET_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
        get_target_property(target_type ${target} TYPE)
        
        # FIX: Apply SYSTEM to ALL targets (including INTERFACE_LIBRARY)
        # This forces consumers (your Engine) to treat these includes as -isystem
        set_target_properties(${target} PROPERTIES SYSTEM ON)

        # Only apply compile flags to targets that actually compile binary objects
        if(${target_type} STREQUAL "EXECUTABLE" OR 
           ${target_type} STREQUAL "STATIC_LIBRARY" OR 
           ${target_type} STREQUAL "SHARED_LIBRARY" OR 
           ${target_type} STREQUAL "MODULE_LIBRARY" OR 
           ${target_type} STREQUAL "OBJECT_LIBRARY")
            
            target_compile_options(${target} PRIVATE 
                $<$<CXX_COMPILER_ID:MSVC>:/W0>
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
            )
        endif()
    endforeach()

    get_property(subdirs DIRECTORY "${TARGET_DIR}" PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirs})
        suppress_warnings_recursive_impl("${subdir}")
    endforeach()
endfunction()

set(FETCHED_LIBS 
    SDL3 spdlog OpenAL32 sndfile eigen MeshOptimizer 
    GoogleTest Ktx VMA tinygltf
)

foreach(LIB ${FETCHED_LIBS})
    string(TOLOWER ${LIB} LIB_LOWER)
    if(DEFINED ${LIB_LOWER}_SOURCE_DIR)
        suppress_warnings_recursive_impl("${${LIB_LOWER}_SOURCE_DIR}")
    endif()
endforeach()

add_subdirectory(Engine)
add_subdirectory(App)
