# Domi Engine

A small C++11 game engine experiment built on top of SDL3, using WebAssembly (WAMR) as a scripting backend.

## Features

- **2D & 3D rendering** via SDL GPU / SDL Renderer
- **Entity-Component-System (ECS)** for game objects
- **WASM scripting** with C/C++ script code compiled to `.wasm`
- **Multi-threaded script updates** using a C++11 thread pool
- **Scene management** with loading / unloading
- **Input, audio, and math utilities**
- **Low-poly 3D sample game**: collect spinning cubes in the main scene

## Project Structure

```
.
├── assets/            # Audio, fonts, models, shaders, textures
├── engine/            # Engine source and headers
│   ├── include/domi/  # Public engine headers
│   └── src/           # Implementation (core, ecs, input, render, script, ...)
├── scripts/src/       # C++ source files compiled to WASM
├── script_api/        # `domi_api.h` exposed to scripts
├── main.cpp           # Executable entry point and sample scenes
├── CMakeLists.txt     # Build configuration
├── sdl3/              # Installed SDL3 prefix
├── wamr/              # Installed WAMR (iwasm) prefix
└── emsdk/             # Emscripten SDK (used for WASM compilation)
```

## Sample Game

The default `MainScene` is a small low-poly 3D game:

- You control a cube on a large ground plane.
- Use **WASD / Arrow keys** to move around.
- Walk into the smaller spinning cubes to collect them.
- Each collected cube increases your score; a new wave spawns when all cubes are collected.
- Press **R** to switch to the 2D demo scene.

## Build Requirements

- macOS (ARM64) — the current setup is Apple Silicon
- CMake 3.16+
- C++11 / C11 compatible compiler
- SDL3 (installed under `./sdl3`)
- WAMR / iwasm (installed under `./wamr`)
- One of the following WASM compilers:
  - Emscripten (under `./emsdk`)
  - Homebrew LLVM with wasm32-wasi support
  - WASI SDK

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

The build produces:

- `build/domi_game` — the main game executable
- `build/scripts/player_2d.wasm` — sample WASM script

## Run

```bash
cd build
./domi_game
```

### Controls

| Key | Action |
|-----|--------|
| `WASD` / Arrow keys | Move the 2D player (SecondScene) or the 3D player (MainScene) |
| `Space` | Switch player color (SecondScene) |
| `R` | Switch between MainScene (3D collect game) and SecondScene (2D demo) |

## Writing Scripts

Scripts are written in C/C++ and compiled to WebAssembly. They communicate with the engine through the API in `script_api/domi_api.h`.

A minimal script implements four lifecycle functions:

```c
#include "domi_api.h"

void script_init(void) {
    uint32_t e = domi_entity_create();
    domi_transform_set_position(e, 100.0f, 100.0f, 0.0f);
    domi_sprite_set_color(e, 255, 0, 0, 255);
}

void script_update(float dt) {
    // called every frame from the thread pool
}

void script_fixed_update(void) {
    // called on the fixed timestep
}

void script_cleanup(void) {
    // cleanup
}
```

To load a script from C++:

```cpp
script->loadScript(entity, "scripts/my_script.wasm");
```

### Available Native API

- **Entity**: `domi_entity_create`, `domi_entity_destroy`
- **Transform**: `domi_transform_set_position`, `domi_transform_get_position`, `domi_transform_set_rotation`, `domi_transform_set_scale`
- **2D Sprite**: `domi_sprite_set_texture`, `domi_sprite_set_color`
- **3D Mesh**: `domi_mesh_set_model`
- **Camera**: `domi_camera_set_perspective`, `domi_camera_set_orthographic`, `domi_camera_set_active`
- **Light**: `domi_light_set_type`, `domi_light_set_color`, `domi_light_set_intensity`
- **Input**: `domi_input_is_key_pressed`, `domi_input_is_key_down`, `domi_input_get_axis`
- **Audio**: `domi_audio_play`, `domi_audio_stop`
- **Debug**: `domi_log`

## Architecture Notes

- **Rendering** runs on the main thread.
- **Script updates** are dispatched to a worker thread pool; shared engine state is protected by a global mutex inside the native API wrappers.
- **Scenes** own their entities. When a scene is unloaded, call `script->unloadScript(entity)` before `world->clear()` for any entity that has a script attached.

## License

This is an experimental learning project. No specific license is set yet.
