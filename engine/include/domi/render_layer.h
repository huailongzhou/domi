#ifndef DOMI_RENDER_LAYER_H
#define DOMI_RENDER_LAYER_H

namespace domi {

// Standard 2D render layers. Lower values are drawn first (farther back).
enum class RenderLayer {
    Background = 0,
    Ground     = 100,
    Road       = 200,
    RoadMarking= 250,
    Object     = 300, // trunks, vehicles, houses, rocks
    Canopy     = 350, // tree foliage rendered above objects
    Cloud      = 500,
    Effect     = 600,
    UI         = 700
};

} // namespace domi

#endif // DOMI_RENDER_LAYER_H
