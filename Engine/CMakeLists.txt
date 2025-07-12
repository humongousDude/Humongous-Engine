cmake_minimum_required(VERSION 3.26)
project(Engine)

include(ExternalProject)

if(MSVC)
    set(RUNTIME_LIB_FLAG "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library type") 
endif()

set(ENGINE_SOURCES
    # --- Core ---
    core/src/allocator.cpp
    core/src/asserts.cpp
    core/src/engine.cpp
    core/src/logger.cpp
    
    # --- Systems ---
    systems/src/asset_manager.cpp
    systems/src/globals.cpp
    systems/src/resource_manager.cpp

    # --- Utils ---
    utils/src/extra.cpp
    utils/src/images.cpp

    # --- Graphics ---
    graphics/src/abstractions/buffer.cpp
    graphics/src/abstractions/descriptor_layout.cpp
    graphics/src/abstractions/descriptor_pool.cpp
    graphics/src/abstractions/descriptor_pool_growable.cpp
    graphics/src/abstractions/descriptor_writer.cpp
    graphics/src/render_systems/simple_render_system.cpp
    graphics/src/render_systems/skybox_render_system.cpp
    graphics/src/instance.cpp
    graphics/src/logical_device.cpp
    graphics/src/material.cpp
    graphics/src/model.cpp
    graphics/src/model_instance.cpp
    graphics/src/physical_device.cpp
    graphics/src/render_pipeline.cpp
    graphics/src/renderer.cpp
    graphics/src/scene.cpp
    graphics/src/skybox.cpp
    graphics/src/swapchain.cpp
    graphics/src/texture.cpp
    graphics/src/vulkan_app.cpp
    graphics/src/window.cpp

    # --- Audio ---
    audio/src/audio_engine.cpp
    audio/src/audio_source.cpp

    # --- Gameplay ---
    gameplay/src/entity_component_system/components/audio_source_component.cpp
    gameplay/src/entity_component_system/components/bounding_box_component.cpp
    gameplay/src/entity_component_system/components/entity_component.cpp
    gameplay/src/entity_component_system/components/model_component.cpp
    gameplay/src/entity_component_system/components/name_component.cpp
    gameplay/src/entity_component_system/components/transform_component.cpp
    gameplay/src/entity_component_system/entity.cpp
    gameplay/src/entity_component_system/sparse_set.cpp
    gameplay/src/ui/ui.cpp
    gameplay/src/ui/widget.cpp
    gameplay/src/camera.cpp
    gameplay/src/keyboard_handler.cpp
    gameplay/src/scene_handler.cpp
    gameplay/src/world.cpp

    # --- ImGui Sources ---
    ThirdParty/imgui/imgui.cpp
    ThirdParty/imgui/imgui_demo.cpp
    ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_tables.cpp
    ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/misc/cpp/imgui_stdlib.cpp
    ThirdParty/imgui/backends/imgui_impl_sdl3.cpp
    ThirdParty/imgui/backends/imgui_impl_vulkan.cpp
)

set(ENGINE_HEADERS
   # --- Core ---
    core/inc/allocator.hpp
    core/inc/asserts.hpp
    core/inc/engine.hpp
    core/inc/logger.hpp
    
    # --- Systems ---
    systems/inc/asset_manager.hpp
    systems/inc/globals.hpp
    systems/inc/resource_manager.hpp

    # --- Utils ---
    utils/inc/extra.hpp
    utils/inc/images.hpp

    # --- Graphics ---
    graphics/inc/abstractions/buffer.hpp
    graphics/inc/abstractions/descriptor_layout.hpp
    graphics/inc/abstractions/descriptor_pool.hpp
    graphics/inc/abstractions/descriptor_pool_growable.hpp
    graphics/inc/abstractions/descriptor_writer.hpp
    graphics/inc/render_systems/simple_render_system.hpp
    graphics/inc/render_systems/skybox_render_system.hpp
    graphics/inc/instance.hpp
    graphics/inc/logical_device.hpp
    graphics/inc/material.hpp
    graphics/inc/model.hpp
    graphics/inc/model_instance.hpp
    graphics/inc/physical_device.hpp
    graphics/inc/render_pipeline.hpp
    graphics/inc/renderer.hpp
    graphics/inc/scene.hpp
    graphics/inc/skybox.hpp
    graphics/inc/swapchain.hpp
    graphics/inc/texture.hpp
    graphics/inc/vulkan_app.hpp
    graphics/inc/window.hpp

    # --- Audio ---
    audio/inc/audio_engine.hpp
    audio/inc/audio_source.hpp

    # --- Gameplay ---
    gameplay/inc/entity_component_system/components/audio_source_component.hpp
    gameplay/inc/entity_component_system/components/bounding_box_component.hpp
    gameplay/inc/entity_component_system/components/entity_component.hpp
    gameplay/inc/entity_component_system/components/model_component.hpp
    gameplay/inc/entity_component_system/components/name_component.hpp
    gameplay/inc/entity_component_system/components/transform_component.hpp
    gameplay/inc/entity_component_system/entity.hpp
    gameplay/inc/entity_component_system/sparse_set.hpp
    gameplay/inc/ui/ui.hpp
    gameplay/inc/ui/widget.hpp
    gameplay/inc/camera.hpp
    gameplay/inc/keyboard_handler.hpp
    gameplay/inc/scene_handler.hpp
    gameplay/inc/world.hpp

    # --- ImGui Sources ---
    ThirdParty/imgui/imgui.h
    ThirdParty/imgui/misc/cpp/imgui_stdlib.h
    ThirdParty/imgui/backends/imgui_impl_sdl3.h
    ThirdParty/imgui/backends/imgui_impl_vulkan.h
)

set(SHADER_SOURCES
    #frag    
    ../shaders/frag/pbr.frag.glsl
    ../shaders/frag/skybox.frag.glsl
    ../shaders/frag/nothing.frag.glsl
    ../shaders/frag/unlit.frag.glsl

    #vert
    ../shaders/vert/skybox.vert.glsl
    ../shaders/vert/simple.vert.glsl
    ../shaders/vert/depth.vert.glsl
    ../shaders/vert/lighting.vert.glsl
    ../shaders/vert/nothing.vert.glsl

    #comp
    ../shaders/comp/brdflut.comp.glsl
    ../shaders/comp/irradiance.comp.glsl
    ../shaders/comp/lighting.comp.glsl
    ../shaders/comp/mip.comp.glsl
    ../shaders/comp/occlusion.comp.glsl
    ../shaders/comp/prefiltredmap.comp.glsl
    
)
set(SHADER_INCLUDES
    ../shaders/includes/inputs.glsl
)

# Convert relative paths to full paths for the custom command
set(SHADER_SOURCES_FULL)
foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(FULL_PATH ${SHADER_FILE} ABSOLUTE)
    list(APPEND SHADER_SOURCES_FULL ${FULL_PATH})
endforeach()

set(SHADER_INCLUDES_FULL)
foreach(INCLUDE_FILE ${SHADER_INCLUDES})
    get_filename_component(FULL_PATH ${INCLUDE_FILE} ABSOLUTE)
    list(APPEND SHADER_INCLUDES_FULL ${FULL_PATH})
endforeach()


# ========================================================================================
# Engine Library Target
# ========================================================================================

add_library(Engine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_sources(Engine PRIVATE ${SHADER_SOURCES_FULL})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/shaders PREFIX "Shaders" FILES ${SHADER_SOURCES_FULL})

if(MSVC)
    target_compile_options(Engine PUBLIC /utf-8)
endif()

target_compile_definitions(Engine PUBLIC
    HGASSERTIONS_ENABLED
    HGASSETDIRPATH="${CMAKE_SOURCE_DIR}/Assets/"
)

target_include_directories(Engine PUBLIC
    # Local includes
    core/inc
    gameplay/inc
    graphics/inc
    systems/inc
    utils/inc
    audio/inc
    
    # Third-party includes
    "ThirdParty/glm"
    "ThirdParty/gli"
    "ThirdParty/VulkanMemoryAllocator/include"
    "ThirdParty/tinygltf"
    "ThirdParty/imgui"
    "ThirdParty/imgui/backends"
)

find_package(Vulkan REQUIRED)

target_link_libraries(Engine PUBLIC
    SDL3::SDL3
    spdlog::spdlog
    OpenAL
    sndfile
    Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(Engine PUBLIC
        gdi32 shell32 ole32 imm32 winmm version user32 setupapi dinput8 dxguid oleaut32 advapi32
    )
endif()

# ========================================================================================
# Shader Compilation Target
# ========================================================================================

find_program(VULKAN_GLSLC glslc REQUIRED)

set(SPIRV_FILES) 

foreach(SHADER_FILE ${SHADER_SOURCES_FULL})
    get_filename_component(PARENT_DIR ${SHADER_FILE} DIRECTORY)
    get_filename_component(SHADER_STAGE ${PARENT_DIR} NAME)

    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE "${CMAKE_SOURCE_DIR}/Assets/shaders/${SHADER_NAME}.spv")
    list(APPEND SPIRV_FILES ${SPIRV_FILE})

    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${VULKAN_GLSLC}
                --target-env=vulkan1.3
                -fshader-stage=${SHADER_STAGE}
                -I"${CMAKE_SOURCE_DIR}/shaders/includes" 
                ${SHADER_FILE}
                -g 
                -o ${SPIRV_FILE}
        DEPENDS ${SHADER_FILE} ${SHADER_INCLUDES_FULL}
        COMMENT "Compiling ${SHADER_NAME} to ${SPIRV_FILE}"
        VERBATIM
    )
endforeach()

add_custom_target(
    compile_shaders
    ALL 
    DEPENDS ${SPIRV_FILES}
    COMMENT "Compiling all GLSL shaders to SPIR-V"
)

add_dependencies(Engine compile_shaders)
