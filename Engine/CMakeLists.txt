file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS *.cpp)

set(cur_path ${CMAKE_CURRENT_LIST_DIR})
cmake_path(GET cur_path PARENT_PATH CMAKE_PARENT_LIST_DIR)

message(STATUS "parent list dir: ${CMAKE_PARENT_LIST_DIR}")

add_compile_definitions(HGASSERTIONS_ENABLED)
add_definitions(-DHGASSETDIRPATH="${CMAKE_PARENT_LIST_DIR}/Assets/")
set(VCPKG_ROOT $ENV{VCPKG_ROOT})

add_library(Engine STATIC ${SRCS})

target_include_directories(Engine PUBLIC core/inc gameplay/inc graphics/inc
                                         utils/inc systems/inc)

find_package(Vulkan)
target_link_libraries(Engine PUBLIC Vulkan::Vulkan)

find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(Engine PUBLIC Vulkan::Headers
                                    GPUOpen::VulkanMemoryAllocator)

find_package(VulkanMemoryAllocator CONFIG REQUIRED)

find_package(glfw3 REQUIRED)
target_link_libraries(Engine PUBLIC glfw)

find_package(glm CONFIG REQUIRED)
target_link_libraries(Engine PUBLIC glm::glm)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(Engine PUBLIC ${TINYGLTF_INCLUDE_DIRS})

find_package(gli CONFIG REQUIRED)
target_link_libraries(Engine PUBLIC gli)

find_package(Boost REQUIRED COMPONENTS log)
target_link_libraries(Engine PUBLIC Boost::boost Boost::log)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(Engine PUBLIC imgui::imgui)

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
