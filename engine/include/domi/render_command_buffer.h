#ifndef DOMI_RENDER_COMMAND_BUFFER_H
#define DOMI_RENDER_COMMAND_BUFFER_H

#include "domi/math.h"
#include "domi/material.h"

namespace domi {

class Canvas2D;
class RenderTexture;

// A lightweight command buffer that records 2D draw calls against a
// Canvas2D-backed RenderQueue. It is used by each RenderPass to draw into a
// specific render target while minimizing wasm-to-native boundary crossings.
class CommandBuffer {
public:
    explicit CommandBuffer(Canvas2D* canvas);
    ~CommandBuffer();

    // Switch the rendering target. Flushes any pending queued commands for the
    // previous target before changing.
    void setTarget(RenderTexture* target);

    // State
    void setFillColor(const Color& c);
    void setStrokeColor(const Color& c);
    void setLineWidth(float w);

    // Simple shapes
    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void clearRect(float x, float y, float w, float h);
    void drawLine(float x1, float y1, float x2, float y2);
    void fillCircle(float x, float y, float radius, int segments = 32);

    // Path API
    void beginPath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void fill();
    void stroke();

    // Draw a generated material at (x, y).
    void drawMaterial(float x, float y, const Material& material);

    // Draw a render texture into the current target.
    void drawTexture(float x, float y, RenderTexture* texture);

    // Force submit any queued commands for the current target.
    void flush();

    // Set/reset a clip rectangle in the current target's coordinate space.
    void setClipRect(float x, float y, float w, float h);
    void resetClipRect();

    Canvas2D* getCanvas() const { return canvas_; }
    RenderTexture* getCurrentTarget() const { return currentTarget_; }

private:
    Canvas2D* canvas_;
    RenderTexture* currentTarget_;
};

} // namespace domi

#endif // DOMI_RENDER_COMMAND_BUFFER_H
