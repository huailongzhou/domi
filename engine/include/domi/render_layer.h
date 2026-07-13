#ifndef DOMI_RENDER_LAYER_H
#define DOMI_RENDER_LAYER_H

namespace domi {

// Abstract 2D render layers ordered from back to front.
// The numeric spacing leaves room for custom sub-layers.
enum class RenderLayer {
    Background = 0,   // sky, horizon, far scenery
    Ground     = 100, // terrain, grass, water
    Surface    = 200, // roads, paths, floor markings
    Object     = 300, // props, vehicles, buildings, characters
    Canopy     = 400, // foliage, roofs, anything above objects
    Effect     = 500, // particles, lights, screen-space effects
    Overlay    = 600, // full-screen overlays, vignettes
    UI         = 700  // menus, HUD, buttons
};

} // namespace domi

#endif // DOMI_RENDER_LAYER_H
