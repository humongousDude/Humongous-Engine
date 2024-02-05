cmake_minimum_required(VERSION 3.26)
project(frfrthistime)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS src/*.cpp)

add_compile_definitions(HGASSERTIONS_ENABLED)

add_executable(main src/main.cpp ${SRCS})

target_include_directories(main PUBLIC src/core/inc src/gameplay/inc
                                       src/graphics/inc src/utils/inc)

find_package(glfw3 REQUIRED)
target_link_libraries(main PRIVATE glfw)

find_package(Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(main PRIVATE Vulkan::Headers
                                   GPUOpen::VulkanMemoryAllocator)

find_package(glm CONFIG REQUIRED)
target_link_libraries(main PRIVATE glm::glm)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(main PRIVATE fmt::fmt)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(main PRIVATE imgui::imgui)

find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(main PRIVATE tinyobjloader::tinyobjloader)

find_package(Stb REQUIRED)
target_include_directories(main PRIVATE ${Stb_INCLUDE_DIR})

# Collect shader files
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS shaders/*.glsl)

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

  set(SPIRV_FILE ${CMAKE_CURRENT_BINARY_DIR}/compiledShaders/${SHADER_NAME}.spv
  )# Place SPIR-V
  # files in a shaders

  add_custom_command(
    TARGET main
    COMMAND ${VULKAN_GLSLC} -fshader-stage=${SHADER_STAGE} ${SHADER_FILE} -o
            ${SPIRV_FILE} DEPENDS ${SHADER_FILE} FILE
    COMMENT "Compiling ${SHADER_FILE} to ${SPIRV_FILE}\n"
    VERBATIM)
endforeach()

add_custom_target(compile_shaders DEPENDS ${SPIRV_FILES})
add_dependencies(main compile_shaders)
