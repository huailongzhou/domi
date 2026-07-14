#ifndef DOMI_RENDER_COMMAND_H
#define DOMI_RENDER_COMMAND_H

#include "domi/math.h"
#include <vector>

namespace domi {

// Lightweight 2D render command used by RenderQueue.
// Keeping draw calls queued until end-of-frame reduces the number of
// wasm-to-native boundary crossings when the renderer is driven from scripts.
struct RenderCommand {
    enum Type {
        SetFillColor,
        SetStrokeColor,
        SetLineWidth,
        FillRect,
        StrokeRect,
        ClearRect,
        DrawLine,
        FillCircle,
        FillPath,
        StrokePath,
        DrawMaterial,
    } type;

    union Params {
        struct { float r, g, b, a; } color;
        struct { float x, y, w, h; } rect;
        struct { float x1, y1, x2, y2; } line;
        struct { float x, y, radius; int segments; } circle;
        struct { float lineWidth; } lineWidth;
        struct { int pathIndex; } path;
        struct { float x, y; void* handle; } material;
    } params;
};

// A stored path for FillPath / StrokePath commands.
// Stored separately so RenderCommand stays small and POD-friendly.
struct StoredPath {
    std::vector<Vec2> points;
    bool closed;
};

} // namespace domi

#endif // DOMI_RENDER_COMMAND_H
