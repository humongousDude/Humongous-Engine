cmake_minimum_required(VERSION 3.26)
project(HumongousEngine)

# ========================================================================================
# Dependency Management with FetchContent
# ========================================================================================
include(FetchContent)

# Set a common place for FetchContent to store sources if you want
# set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_dependencies")

# --- Declare where to find your dependencies ---
# We point it to the local source directories you already have.
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

# --- Set options for the dependencies BEFORE making them available ---
# This prevents them from trying to build tests, examples, etc.
# For SDL3
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE) # Build as a DLL
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

# For spdlog
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Build as static lib

# For OpenAL-Soft
set(LIBTYPE "SHARED" CACHE STRING "" FORCE) # Build as a DLL
set(ALSOFT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)


# --- Make the dependencies available to your project ---
# This pulls their targets (like SDL3::SDL3) into your build.
FetchContent_MakeAvailable(SDL3 spdlog OpenAL32 Sndfile)

# ========================================================================================
# Project Subdirectories
# ========================================================================================
add_subdirectory(Engine)
add_subdirectory(App)
