#ifndef DOMI_API_H
#define DOMI_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// === Entity ===
uint32_t domi_entity_create(void);
void domi_entity_destroy(uint32_t entity);

// === Transform ===
void domi_transform_set_position(uint32_t entity, float x, float y, float z);
void domi_transform_get_position(uint32_t entity, float* x, float* y, float* z);
void domi_transform_set_rotation(uint32_t entity, float x, float y, float z, float w);
void domi_transform_set_scale(uint32_t entity, float x, float y, float z);

// === Sprite (2D) ===
void domi_sprite_set_texture(uint32_t entity, const char* path);
void domi_sprite_set_color(uint32_t entity, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// === Mesh (3D) ===
void domi_mesh_set_model(uint32_t entity, const char* path);
void domi_mesh_set_material(uint32_t entity, const char* path);

// === Camera ===
void domi_camera_set_perspective(uint32_t entity, float fov, float near_plane, float far_plane);
void domi_camera_set_orthographic(uint32_t entity, float size, float near_plane, float far_plane);
void domi_camera_set_active(uint32_t entity);

// === Light ===
void domi_light_set_type(uint32_t entity, int type); // 0=directional, 1=point, 2=spot
void domi_light_set_color(uint32_t entity, float r, float g, float b);
void domi_light_set_intensity(uint32_t entity, float intensity);

// === Input ===
bool domi_input_is_key_pressed(int key);
bool domi_input_is_key_down(int key);
float domi_input_get_axis(const char* name);

// === Audio ===
void domi_audio_play(const char* path, bool loop);
void domi_audio_stop(const char* path);

// === Debug ===
void domi_log(const char* message);

// === Script Lifecycle (called by engine) ===
void script_init(void);
void script_update(float dt);
void script_fixed_update(void);
void script_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
