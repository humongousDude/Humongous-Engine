# App/CMakeLists.txt
cmake_minimum_required(VERSION 3.26)

add_executable(App main.cpp)

if(MSVC)
    target_compile_options(App PUBLIC /utf-8)
    set(RUNTIME_LIB_FLAG "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library type")
endif()

target_link_libraries(App PRIVATE
	Engine
)

if(WIN32)
    function(copy_dll_from_target target_name)
        add_custom_command(TARGET App POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${target_name}>"
                "$<TARGET_FILE_DIR:App>"
            COMMENT "Copying $<TARGET_FILE_NAME:${target_name}>"
        )
    endfunction()

    copy_dll_from_target(SDL3::SDL3)
    copy_dll_from_target(OpenAL)
endif()

