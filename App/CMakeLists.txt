cmake_minimum_required(VERSION 3.26)

project(Application LANGUAGES CXX C)

function(configure_executable target)
    target_link_libraries(${target} PRIVATE
        Engine
    )

    if(MSVC)
        target_compile_options(${target} PUBLIC /utf-8)
        set(RUNTIME_LIB_FLAG "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library type")
    endif()

    if(WIN32)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                $<$<AND:$<BOOL:WIN32>,$<STREQUAL:$<CXX_COMPILER_ID>,GNU>>:-Wa,-mbig-obj>
            )
        endif()
    endif()

    if(UNIX AND CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

add_executable(App main.cpp)
configure_executable(App)

add_executable(AppTests test_main.cpp)
configure_executable(AppTests)

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
