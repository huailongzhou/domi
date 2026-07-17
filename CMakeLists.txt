cmake_minimum_required(VERSION 3.16)
project(domi_engine VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures")
endif()

# Paths to installed dependencies
set(SDL3_ROOT "${CMAKE_SOURCE_DIR}/sdl3" CACHE PATH "SDL3 install prefix")
set(WAMR_ROOT "${CMAKE_SOURCE_DIR}/wamr" CACHE PATH "WAMR install prefix")
set(FREETYPE_ROOT "${CMAKE_SOURCE_DIR}/freetype" CACHE PATH "FreeType install prefix")
set(FREETYPE_MINGW_ROOT "${CMAKE_SOURCE_DIR}/freetype-mingw" CACHE PATH "FreeType MinGW install prefix")

# Find SDL3
find_package(SDL3 REQUIRED PATHS ${SDL3_ROOT} NO_DEFAULT_PATH)

# Find WAMR
find_package(iwasm REQUIRED PATHS ${WAMR_ROOT}/lib/cmake/iwasm NO_DEFAULT_PATH)

# Pick the FreeType install prefix for the current toolchain.
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_C_COMPILER MATCHES "mingw")
    set(FREETYPE_ACTIVE_ROOT ${FREETYPE_MINGW_ROOT})
else()
    set(FREETYPE_ACTIVE_ROOT ${FREETYPE_ROOT})
endif()
set(FREETYPE_INCLUDE_DIR "${FREETYPE_ACTIVE_ROOT}/include/freetype2")
set(FREETYPE_LIBRARY "${FREETYPE_ACTIVE_ROOT}/lib/libfreetype.a")

# Engine library
add_library(domi_engine STATIC
    engine/src/math/math.cpp
    engine/src/core/app.cpp
    engine/src/core/window.cpp
    engine/src/core/thread_pool.cpp
    engine/src/ecs/ecs.cpp
    engine/src/input/input.cpp
    engine/src/script/script.cpp
    engine/src/render/2d/render2d.cpp
    engine/src/render/canvas2d.cpp
    engine/src/render/render_queue.cpp
    engine/src/render/renderer.cpp
    engine/src/render/render_texture.cpp
    engine/src/render/render_command_buffer.cpp
    engine/src/render/render_list.cpp
    engine/src/render/draw_batch.cpp
    engine/src/render/pass/geometry_pass.cpp
    engine/src/render/pass/shadow_pass.cpp
    engine/src/render/pass/lighting_pass.cpp
    engine/src/render/pass/composite_pass.cpp
    engine/src/render/pass/ui_pass.cpp
    engine/src/render/render_node.cpp
    engine/src/render/material_io.cpp
    engine/src/scene/scene.cpp
    engine/src/scene/scene_loader.cpp
    engine/src/scene/scene_manager.cpp
    engine/src/scene/scene_function.cpp
    engine/src/backend/sdl_backend.cpp
    engine/src/audio/audio.cpp
    engine/src/ui/ui.cpp
    engine/src/ui/font.cpp
    tools/generators/material_generator.cpp
    tools/generators/tree_generator.cpp
    tools/generators/cloud_generator.cpp
    tools/generators/rock_generator.cpp
    tools/generators/house_generator.cpp
    tools/generators/horizon_generator.cpp
    tools/generators/3d/cube3d_generator.cpp
    tools/generators/3d/car3d_generator.cpp
    tools/generators/3d/tree3d_generator.cpp
)

target_include_directories(domi_engine PUBLIC
    ${CMAKE_SOURCE_DIR}/engine/include
    ${CMAKE_SOURCE_DIR}/tools/generators
    ${WAMR_ROOT}/include
    ${FREETYPE_INCLUDE_DIR}
)

target_link_libraries(domi_engine PUBLIC
    SDL3::SDL3
    iwasm::vmlib
    ${FREETYPE_LIBRARY}
)

# Main executable
add_executable(domi_game
    main.cpp
    demo/game2d_scene.cpp
    demo/game3d_scene.cpp
    demo/menu_scene.cpp
    demo/second_scene.cpp
)
target_link_libraries(domi_game PRIVATE domi_engine)

# Dear ImGui (vendored, for tools)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# Scene editor
add_executable(domi_editor
    editor/editor_main.cpp
    editor/editor_scene.cpp
)
target_include_directories(domi_editor PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/demo
)
target_link_libraries(domi_editor PRIVATE domi_engine imgui)

# Scripts (WASM compilation)
set(WASM_COMPILER "auto" CACHE STRING "WASM compiler: auto, emscripten, brew-llvm, wasi-sdk")

# Try to find Emscripten
if(EXISTS ${CMAKE_SOURCE_DIR}/emsdk/upstream/emscripten/emcc)
    set(EMSCRIPTEN_EMCC ${CMAKE_SOURCE_DIR}/emsdk/upstream/emscripten/emcc)
endif()

# Try to find Homebrew LLVM
if(EXISTS /opt/homebrew/opt/llvm/bin/clang)
    set(BREW_LLVM_CLANG /opt/homebrew/opt/llvm/bin/clang)
endif()

# Try to find WASI SDK
if(DEFINED ENV{WASI_SDK_DIR})
    set(WASI_SDK_DIR $ENV{WASI_SDK_DIR})
    set(WASI_CLANG ${WASI_SDK_DIR}/bin/clang)
endif()

# Try to find a system clang with wasm32-wasi support
if(NOT SYSTEM_CLANG)
    find_program(CLANG_CANDIDATE clang)
    if(CLANG_CANDIDATE)
        execute_process(
            COMMAND ${CLANG_CANDIDATE} --target=wasm32-wasi -dM -E -x c -
            INPUT_FILE /dev/null
            OUTPUT_QUIET ERROR_QUIET
            RESULT_VARIABLE CLANG_WASM_OK
        )
        if(CLANG_WASM_OK EQUAL 0)
            set(SYSTEM_CLANG ${CLANG_CANDIDATE})
        endif()
    endif()
endif()

if(WASM_COMPILER STREQUAL "auto")
    if(EMSCRIPTEN_EMCC)
        set(WASM_COMPILER "emscripten")
    elseif(BREW_LLVM_CLANG)
        set(WASM_COMPILER "brew-llvm")
    elseif(SYSTEM_CLANG)
        set(WASM_COMPILER "system-clang")
    elseif(WASI_CLANG)
        set(WASM_COMPILER "wasi-sdk")
    else()
        set(WASM_COMPILER "none")
    endif()
endif()

message(STATUS "WASM compiler: ${WASM_COMPILER}")

if(WASM_COMPILER STREQUAL "emscripten" AND EMSCRIPTEN_EMCC)
    add_custom_target(scripts ALL)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/scripts
        COMMAND ${CMAKE_COMMAND} -E env PATH="/opt/homebrew/opt/python@3.13/libexec/bin:$ENV{PATH}"
            ${EMSCRIPTEN_EMCC}
            ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
            -I${CMAKE_SOURCE_DIR}/script_api
            -s STANDALONE_WASM=1
            -s EXPORTED_FUNCTIONS='["_script_init","_script_update","_script_fixed_update","_script_cleanup"]'
            -s EXPORTED_RUNTIME_METHODS='[]'
            -nostdlib
            -Wl,--no-entry
            -Wl,--allow-undefined
            -O2
            -o ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
                ${CMAKE_SOURCE_DIR}/script_api/domi_api.h
        COMMENT "Compiling player_2d.cpp to WASM (Emscripten)"
    )

    add_custom_target(player_2d_wasm DEPENDS ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm)
    add_dependencies(scripts player_2d_wasm)

elseif(WASM_COMPILER STREQUAL "brew-llvm" AND BREW_LLVM_CLANG)
    add_custom_target(scripts ALL)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
        COMMAND ${BREW_LLVM_CLANG}
            --target=wasm32-wasi
            -O2
            -nostartfiles
            -Wl,--no-entry
            -Wl,--export-all
            -Wl,--allow-undefined
            -I${CMAKE_SOURCE_DIR}/script_api
            -o ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
            ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
                ${CMAKE_SOURCE_DIR}/script_api/domi_api.h
        COMMENT "Compiling player_2d.cpp to WASM (Homebrew LLVM)"
    )

    add_custom_target(player_2d_wasm DEPENDS ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm)
    add_dependencies(scripts player_2d_wasm)

elseif(WASM_COMPILER STREQUAL "wasi-sdk" AND WASI_CLANG)
    add_custom_target(scripts ALL)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
        COMMAND ${WASI_CLANG}
            --target=wasm32-wasi
            -O2
            -nostartfiles
            -Wl,--no-entry
            -Wl,--export-all
            -Wl,--allow-undefined
            -I${CMAKE_SOURCE_DIR}/script_api
            -o ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
            ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
                ${CMAKE_SOURCE_DIR}/script_api/domi_api.h
        COMMENT "Compiling player_2d.cpp to WASM (WASI SDK)"
    )

    add_custom_target(player_2d_wasm DEPENDS ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm)
    add_dependencies(scripts player_2d_wasm)

elseif(WASM_COMPILER STREQUAL "system-clang" AND SYSTEM_CLANG)
    add_custom_target(scripts ALL)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/scripts
        COMMAND ${SYSTEM_CLANG}
            --target=wasm32-wasi
            -O2
            -nostartfiles
            -Wl,--no-entry
            -Wl,--export-all
            -Wl,--allow-undefined
            -I${CMAKE_SOURCE_DIR}/script_api
            -o ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm
            ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/src/player_2d.cpp
                ${CMAKE_SOURCE_DIR}/script_api/domi_api.h
        COMMENT "Compiling player_2d.cpp to WASM (system clang)"
    )

    add_custom_target(player_2d_wasm DEPENDS ${CMAKE_BINARY_DIR}/scripts/player_2d.wasm)
    add_dependencies(scripts player_2d_wasm)
endif()
