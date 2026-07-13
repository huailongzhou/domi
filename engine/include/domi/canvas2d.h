#ifndef DOMI_CANVAS2D_H
#define DOMI_CANVAS2D_H

#include "domi/material.h"
#include "domi/render_queue.h"
#include "domi/math.h"
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace domi {

class RenderTexture;

enum class RenderMode {
    PAINTER,
    ZBUFFER
};

// A minimal Canvas2D-style drawing API built on top of SDL_Renderer.
// Supports basic shapes, paths and convex polygon filling.
// Also includes a software z-buffer rasterizer for 3D triangles.
class Canvas2D {
public:
    explicit Canvas2D(SDL_Renderer* renderer);
    ~Canvas2D();

    void setRenderMode(RenderMode mode) { renderMode_ = mode; }
    RenderMode getRenderMode() const { return renderMode_; }

    void setFillColor(const Color& c);
    void setStrokeColor(const Color& c);
    void setLineWidth(float w);

    const Color& getFillColor() const { return fillColor_; }
    const Color& getStrokeColor() const { return strokeColor_; }
    float getLineWidth() const { return lineWidth_; }

    // Queue / immediate-mode switch. Batching is enabled by default so that
    // wasm-driven scripts can submit many draw calls with one native flush.
    void setBatching(bool enabled);
    bool isBatching() const { return batching_; }
    void flush();

    SDL_Renderer* getRenderer() const { return renderer_; }

    // Simple shapes
    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void clearRect(float x, float y, float w, float h);
    void drawLine(float x1, float y1, float x2, float y2);
    void fillCircle(float x, float y, float radius, int segments = 32);

    // Path API (fill() assumes the path is convex)
    void beginPath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void fill();
    void stroke();

    // Draw a generated material at (x, y). Handles all PixelFormat variants.
    void drawMaterial(float x, float y, const Material& material);

    // Draw a previously rendered target texture at (x, y) at its native size.
    void drawTexture(float x, float y, RenderTexture* texture);

    // Set the SDL render target. Passing NULL resets to the default target.
    void setRenderTarget(RenderTexture* target);

    // Set/reset a clip rectangle in screen/target space.
    void setClipRect(float x, float y, float w, float h);
    void resetClipRect();

    // 3D software rasterizer with z-buffer.
    // Call begin3D() before, end3D() after drawing 3D triangles.
    void begin3D();
    void end3D();
    void fillTriangle3D(const Vec2& a, const Vec2& b, const Vec2& c,
                        float za, float zb, float zc, const Color& color);
    void fillCube3D(float cx, float cy, float size, float rotX, float rotY,
                    float rotZ = 0.0f, const Color& color = Color(1, 1, 1, 1));
    void fillBox3D(float cx, float cy, float sx, float sy, float sz,
                   float rotX, float rotY, float rotZ = 0.0f,
                   const Color& color = Color(1, 1, 1, 1));

    // Draw an arbitrary indexed triangle mesh through the software rasterizer.
    // vertices:   array of local-space positions
    // indices:    triangle list (3 indices per triangle)
    // triangleCount: number of triangles
    void drawMesh3D(float cx, float cy, float scale,
                    float rotX, float rotY, float rotZ,
                    const Vec3* vertices, int vertexCount,
                    const int* indices, int triangleCount,
                    const Color& color);

private:
    SDL_Renderer* renderer_;
    SDL_Texture* target3D_;
    int width_;
    int height_;
    std::vector<float> depthBuffer_;
    void* lockedPixels_;
    int lockedPitch_;
    bool in3D_;
    RenderMode renderMode_;

    Color fillColor_;
    Color strokeColor_;
    float lineWidth_;
    std::vector<Vec2> path_;
    bool pathClosed_;

    RenderQueue queue_;
    bool batching_;
    RenderTexture* currentTarget_;
};

} // namespace domi

#endif
