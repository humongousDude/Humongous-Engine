# TODO: rewrite this

set(cur_path ${CMAKE_CURRENT_LIST_DIR})

# TODO: Switch from gli to something else
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

cmake_path(GET cur_path PARENT_PATH CMAKE_PARENT_LIST_DIR)


add_compile_definitions(HGASSERTIONS_ENABLED)
add_definitions(-DHGASSETDIRPATH="${CMAKE_PARENT_LIST_DIR}/Assets/")

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS core/*.cpp gameplay/*.cpp graphics/*.cpp systems/*.cpp utils/*.cpp)

add_subdirectory(ThirdParty/SDL EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/gli EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/glm EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/VulkanMemoryAllocator EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/tinygltf EXCLUDE_FROM_ALL)

# There has got to be a better way to do this

file(GLOB IMGUI CONFIGURE_DEPENDS ./ThirdParty/imgui/*.cpp)
add_library(Engine STATIC ${SRCS} ${IMGUI}
    ./ThirdParty/imgui/backends/imgui_impl_vulkan.cpp
    ./ThirdParty/imgui/backends/imgui_impl_sdl3.cpp   
    )

target_include_directories(
    Engine
    PUBLIC core/inc
           gameplay/inc
           graphics/inc
           utils/inc
           systems/inc
           ThirdParty/imgui/
           ThirdParty/imgui/backends/
           ThirdParty/SDL/include/SDL3/ 
           ThirdParty/gli/gli/
           ThirdParty/gli/gli/core/
           ThirdParty/glm/glm/ext/
           ThirdParty/glm/glm/gtc/
           ThirdParty/glm/glm/gtx/
           ThirdParty/glm/glm/simd/
           ThirdParty/glm/glm/detail/
           ThirdParty/spdlog/include/spdlog/
           ThirdParty/spdlog/include/spdlog/cfg/
           ThirdParty/spdlog/include/spdlog/fmt/
           ThirdParty/spdlog/include/spdlog/fmt/bundled/
           ThirdParty/spdlog/include/spdlog/sinks/
           ThirdParty/spdlog/include/spdlog/details/
           ThirdParty/VulkanMemoryAllocator/include/
)

find_package(Vulkan)

target_link_libraries(Engine PUBLIC SDL3::SDL3 glm::glm gli spdlog::spdlog Vulkan::Vulkan Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_include_directories(Engine PUBLIC ThirdParty/tinygltf/)

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
