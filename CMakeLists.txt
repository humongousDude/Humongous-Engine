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

function(make_target_system_includes TARGET_NAME)
    get_target_property(INCLUDES ${TARGET_NAME} INTERFACE_INCLUDE_DIRECTORIES)
    if(INCLUDES)
        set_target_properties(${TARGET_NAME} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
        
        target_include_directories(${TARGET_NAME} 
            SYSTEM INTERFACE ${INCLUDES}
        )
    endif()
endfunction()

make_target_system_includes(VulkanMemoryAllocator)
make_target_system_includes(tinygltf)
make_target_system_includes(OpenAL)

add_subdirectory(Engine)
add_subdirectory(App)
