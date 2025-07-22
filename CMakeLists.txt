cmake_minimum_required(VERSION 3.26)
project(HumongousEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF) 

include(FetchContent)

# Set a common place for FetchContent to store sources if you want
# set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_dependencies")
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

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE) 
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) 

set(LIBTYPE "SHARED" CACHE STRING "" FORCE) 
set(ALSOFT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 spdlog OpenAL32 Sndfile eigen meshoptimizer)

add_subdirectory(Engine)
add_subdirectory(App)
