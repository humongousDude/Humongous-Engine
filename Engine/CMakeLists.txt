set(BUILD_SHARED_LIBS OFF)
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB IMGUI CONFIGURE_DEPENDS ../lib/imgui/*.cpp)

set(cur_path ${CMAKE_CURRENT_LIST_DIR})
cmake_path(GET cur_path PARENT_PATH CMAKE_PARENT_LIST_DIR)

add_compile_definitions(HGASSERTIONS_ENABLED)
add_definitions(-DHGASSETDIRPATH="${CMAKE_PARENT_LIST_DIR}/Assets/")

add_library(Engine STATIC ${SRCS} ${IMGUI}
    ../lib/imgui/backends/imgui_impl_vulkan.cpp
    ../lib/imgui/backends/imgui_impl_sdl3.cpp   
    )

target_include_directories(
  Engine
  PUBLIC core/inc
         gameplay/inc
         graphics/inc
         utils/inc
         systems/inc
         ../lib/imgui/
         ../lib/imgui/backends/
        )


find_package(Vulkan)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(gli CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_link_libraries(Engine PUBLIC SDL3::SDL3 glm::glm gli spdlog::spdlog Vulkan::Vulkan Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_include_directories(Engine PUBLIC ${TINYGLTF_INCLUDE_DIRS})

# Collect shader files
file(GLOB SHADER_FILES CONFIGURE_DEPENDS
     ${CMAKE_PARENT_LIST_DIR}/shaders/*.glsl)

# Find the Vulkan shader compiler (prefer glslc for SPIR-V generation)
find_program(VULKAN_GLSLC glslc)
if(NOT VULKAN_GLSLC)
  find_program(VULKAN_GLSLANG_VALIDATOR glslangValidator)
endif()

# Ensure at least one compiler is found
if(NOT VULKAN_GLSLC AND NOT VULKAN_GLSLANG_VALIDATOR)
  message(
    FATAL_ERROR
      "Neither glslc nor glslangValidator found for shader compilation.")
endif()

# Compile shaders
foreach(SHADER_FILE ${SHADER_FILES})
  get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
  get_filename_component(SHADER_STAGE ${SHADER_FILE} NAME_WLE)
  get_filename_component(SHADER_STAGE ${SHADER_STAGE} EXT)
  message(STATUS "Compiling ${SHADER_NAME}")

  string(REPLACE "." "" SHADER_STAGE ${SHADER_STAGE})

  message(STATUS "Stage: ${SHADER_STAGE}")

  set(SPIRV_FILE ${CMAKE_PARENT_LIST_DIR}/Assets/shaders/${SHADER_NAME}.spv)
  # Place SPIR-V files in a shaders directory

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
add_dependencies(Engine compile_shaders)
