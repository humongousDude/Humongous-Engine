cmake_minimum_required(VERSION 3.26)
cmake_policy(VERSION 3.26)

include(ExternalProject)

set(EXTERNAL_INSTALL_LOCATION "${CMAKE_BINARY_DIR}/_external_install")

# ===================================================================
#  STEP 1: Define the function to build external projects
# ===================================================================
function(add_external_submodule_project_and_config name src_dir cmake_options)
    set(_project_name "${name}_external_project")
    set(_install_path "${EXTERNAL_INSTALL_LOCATION}/${name}")
    set(_lib_path "${_install_path}/lib") # <-- Simplified path for clarity
    set(_include_path "${_install_path}/include")

    set(CMAKE_ARGS_LIST)
    foreach(opt ${cmake_options})
        list(APPEND CMAKE_ARGS_LIST "-D${opt}")
    endforeach()
    list(APPEND CMAKE_ARGS_LIST "-DBUILD_SHARED_LIBS=OFF")

    ExternalProject_Add(
        ${_project_name}
        SOURCE_DIR              "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/${src_dir}"
        BINARY_DIR              "${CMAKE_BINARY_DIR}/${name}-build"
        INSTALL_DIR             "${_install_path}"
        CMAKE_ARGS              ${CMAKE_ARGS_LIST}
                                -DCMAKE_INSTALL_PREFIX=${_install_path}
                                -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
                                -DCMAKE_BUILD_TYPE=$<CONFIG>
                                ${RUNTIME_LIB_FLAG} # <--- CORRECTLY PLACED
        CMAKE_GENERATOR         "${CMAKE_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
        CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
    )
    
    # Set variables for the parent scope
    set(${name}_EXTERNAL_PROJECT_TARGET "${_project_name}" PARENT_SCOPE)
    set(${name}_LIB_PATH "${_lib_path}" PARENT_SCOPE)
    set(${name}_INCLUDE_PATH "${_include_path}" PARENT_SCOPE)
endfunction()


# ===================================================================
#  STEP 2: Configure all external projects
# ===================================================================
set(ALL_EXTERNAL_PROJECT_TARGETS)

# --- SDL3 ---
add_external_submodule_project_and_config(SDL3 "SDL"
    "SDL_TEST=OFF" "SDL_SHARED=OFF" "SDL_STATIC=ON" "SDL_CODE_ANALYSIS=OFF")
list(APPEND ALL_EXTERNAL_PROJECT_TARGETS ${SDL3_EXTERNAL_PROJECT_TARGET})

# --- spdlog ---
add_external_submodule_project_and_config(spdlog "spdlog"
    "SPDLOG_BUILD_EXAMPLES=OFF" "SPDLOG_BUILD_TESTS=OFF" "SPDLOG_BUILD_BENCH=OFF" "SPDLOG_INSTALL=ON" "SPDLOG_BUILD_SHARED=OFF")
list(APPEND ALL_EXTERNAL_PROJECT_TARGETS ${spdlog_EXTERNAL_PROJECT_TARGET})

# --- OpenAL-Soft ---
add_external_submodule_project_and_config(OpenALSoft "openal-soft"
    "ALSOFT_BUILD_EXAMPLES=OFF" "ALSOFT_INSTALL=ON" "LIBTYPE=STATIC")
list(APPEND ALL_EXTERNAL_PROJECT_TARGETS ${OpenALSoft_EXTERNAL_PROJECT_TARGET})

# --- libsndfile ---
add_external_submodule_project_and_config(libsndfile "libsndfile"
    "BUILD_PROGRAMS=OFF" "BUILD_EXAMPLES=OFF" "ENABLE_EXTERNAL_LIBS=OFF" "BUILD_STATIC_LIBS=ON" "BUILD_SHARED_LIBS=OFF") # <-- Corrected to OFF
list(APPEND ALL_EXTERNAL_PROJECT_TARGETS ${libsndfile_EXTERNAL_PROJECT_TARGET})


# ===================================================================
#  STEP 3: Define your main Engine library
# ===================================================================
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    core/*.cpp
    gameplay/*.cpp
    graphics/*.cpp
    systems/*.cpp
    utils/*.cpp
    audio/*.cpp
)
file(GLOB IMGUI CONFIGURE_DEPENDS ./ThirdParty/imgui/*.cpp)
add_library(Engine ${SRCS} ${IMGUI}
    ./ThirdParty/imgui/backends/imgui_impl_vulkan.cpp
    ./ThirdParty/imgui/backends/imgui_impl_sdl3.cpp
)

# THIS IS THE CORRECT WAY TO SET THE RUNTIME LIBRARY FOR MSVC
if(MSVC)
    target_compile_options(Engine PUBLIC /utf-8)
    # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(RUNTIME_LIB_FLAG "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library type")
endif()

# ===================================================================
#  STEP 4: Configure the Engine library with paths from dependencies
# ===================================================================

# --- Compile Definitions ---
target_compile_definitions(Engine PUBLIC
    HGASSERTIONS_ENABLED
    HGASSETDIRPATH="${CMAKE_SOURCE_DIR}/Assets/"
)

# --- Include Directories ---
target_include_directories(Engine PUBLIC
    # Header-only libraries
    "ThirdParty/glm"
    "ThirdParty/gli"
    "ThirdParty/VulkanMemoryAllocator/include"
    "ThirdParty/tinygltf"
    "ThirdParty/imgui/"
    "ThirdParty/imgui/backends/"

    # Includes for built libraries
    "${SDL3_INCLUDE_PATH}"
    "${spdlog_INCLUDE_PATH}"
    "${OpenALSoft_INCLUDE_PATH}"
    "${libsndfile_INCLUDE_PATH}"

    # Engine files
    core/inc/
    gameplay/inc/
    audio/inc/
    graphics/inc/
    systems/inc/
    utils/inc/
)

find_package(Vulkan REQUIRED)


target_link_libraries(Engine PUBLIC
    # Use generator expressions for configuration-specific linking
    "${SDL3_LIB_PATH}/SDL3.lib"

    debug     "${spdlog_LIB_PATH}/spdlogd.lib"
    optimized "${spdlog_LIB_PATH}/spdlog.lib"

    "${OpenALSoft_LIB_PATH}/OpenAL32.lib"
    "${libsndfile_LIB_PATH}/sndfile.lib"
    
    Vulkan::Vulkan
    legacy_stdio_definitions.lib
)

if(WIN32)
    target_link_libraries(Engine PUBLIC gdi32 shell32 ole32 imm32 winmm version)
endif()

add_dependencies(Engine ${ALL_EXTERNAL_PROJECT_TARGETS})

# --- Shader Compilation (No changes needed here) ---
find_program(VULKAN_GLSLC glslc)
if(NOT VULKAN_GLSLC)
  find_program(VULKAN_GLSLANG_VALIDATOR glslangValidator)
endif()

if(NOT VULKAN_GLSLC AND NOT VULKAN_GLSLANG_VALIDATOR)
  message(FATAL_ERROR "Neither glslc nor glslangValidator found for shader compilation.")
endif()

message(STATUS "glslc found at: ${VULKAN_GLSLC}")



file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/shaders/vert/*.glsl
     ${CMAKE_SOURCE_DIR}/shaders/frag/*.glsl
     ${CMAKE_SOURCE_DIR}/shaders/comp/*.glsl
     ${CMAKE_SOURCE_DIR}/shaders/includes/*.glsl
)
message(STATUS "CMAKE LIST PARENT: ${CMAKE_SOURCE_DIR}")

foreach(SHADER_FILE ${SHADER_FILES})
  get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
  get_filename_component(SHADER_STAGE ${SHADER_FILE} NAME_WLE)
  get_filename_component(SHADER_STAGE ${SHADER_STAGE} EXT)
  string(REPLACE "." "" SHADER_STAGE ${SHADER_STAGE})

  message(STATUS "working on shader ${SHADER_FILE}")
  
  set(SPIRV_FILE ${CMAKE_SOURCE_DIR}/Assets/shaders/${SHADER_NAME}.spv)

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND ${VULKAN_GLSLC} -fshader-stage=${SHADER_STAGE} ${SHADER_FILE} -g -o
            ${SPIRV_FILE}
    DEPENDS ${SHADER_FILE}
    COMMENT "Compiling ${SHADER_FILE} to ${SPIRV_FILE}"
    VERBATIM)
  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SPIRV_FILES}) 

if (MSVC)
    set_property(TARGET compile_shaders PROPERTY FOLDER "Hidden CMake Targets") # Puts it in a folder
    set_property(TARGET compile_shaders PROPERTY EXCLUDE_FROM_ALL TRUE) # Hides it from default build
    set_property(TARGET compile_shaders PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_ALL TRUE) # More hiding
endif()

add_dependencies(Engine compile_shaders)