#include "domi_api.h"

static uint32_t player;
static float speed = 300.0f;
static int colorIndex = 0;

// Simple color palette for the player sprite.
static const struct {
    uint8_t r, g, b;
} colors[] = {
    {0,   200, 255}, // cyan
    {255, 80,  80},  // red
    {80,  255, 80},  // green
    {255, 255, 80},  // yellow
    {200, 80,  255}, // purple
};
static const int colorCount = sizeof(colors) / sizeof(colors[0]);

void script_init(void) {
    player = domi_entity_create();
    domi_transform_set_position(player, 640.0f, 360.0f, 0.0f);
    domi_sprite_set_texture(player, "assets/textures/player.png");
    domi_sprite_set_color(player, colors[0].r, colors[0].g, colors[0].b, 255);
    domi_log("Player script initialized!");
}

void script_update(float dt) {
    float x, y, z;
    domi_transform_get_position(player, &x, &y, &z);

    float h = domi_input_get_axis("Horizontal");
    float v = domi_input_get_axis("Vertical");

    x += h * speed * dt;
    y += v * speed * dt;

    // Clamp to a safe play area inside the 1280x720 window.
    if (x < 40.0f)  x = 40.0f;
    if (x > 1240.0f) x = 1240.0f;
    if (y < 40.0f)  y = 40.0f;
    if (y > 680.0f) y = 680.0f;

    domi_transform_set_position(player, x, y, z);

    if (domi_input_is_key_pressed(57)) { // space
        colorIndex = (colorIndex + 1) % colorCount;
        domi_sprite_set_color(player,
                              colors[colorIndex].r,
                              colors[colorIndex].g,
                              colors[colorIndex].b,
                              255);
        domi_log("Color switch!");
    }
}

void script_fixed_update(void) {
    // Physics/ collision logic here
}

void script_cleanup(void) {
    domi_entity_destroy(player);
    domi_log("Player script cleanup");
}
