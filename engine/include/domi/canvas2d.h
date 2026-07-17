#ifndef DOMI_CANVAS2D_H
#define DOMI_CANVAS2D_H

#include "domi/material.h"
#include "domi/render_queue.h"
#include "domi/math.h"
#include "domi/types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace domi {

class IRenderBackend;
class RenderTexture;
class Font;

enum class RenderMode {
    PAINTER,
    ZBUFFER
};

// A minimal Canvas2D-style drawing API.
//
// This class is a thin wrapper around IRenderBackend. It adds a command queue
// for batched 2D operations and a software z-buffer rasterizer for 3D triangles.
// All platform-specific rendering lives in the backend implementation.
class Canvas2D {
public:
    explicit Canvas2D(IRenderBackend* backend);
    ~Canvas2D();

    void setRenderMode(RenderMode mode) { renderMode_ = mode; }
    RenderMode getRenderMode() const { return renderMode_; }

    void setFillColor(const Color& c);
    void setStrokeColor(const Color& c);
    void setLineWidth(float w);

    const Color& getFillColor() const { return state_.fillColor; }
    const Color& getStrokeColor() const { return state_.strokeColor; }
    float getLineWidth() const { return state_.lineWidth; }

    // State stack (fill/stroke/lineWidth/transform).
    void save();
    void restore();

    // 2D transform (matches HTML canvas semantics).
    void translate(float x, float y);
    void rotate(float radians);
    void scale(float x, float y);
    void transform(float a, float b, float c, float d, float e, float f);
    void setTransform(float a, float b, float c, float d, float e, float f);
    void resetTransform();
    const Affine2D& getTransform() const { return state_.transform; }

    // Queue / immediate-mode switch. Batching is enabled by default so that
    // wasm-driven scripts can submit many draw calls with one native flush.
    void setBatching(bool enabled);
    bool isBatching() const { return batching_; }
    void flush();

    IRenderBackend* getBackend() const { return backend_; }

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

    // Standard canvas-style curve primitives.
    void arc(float x, float y, float radius,
             float startAngle, float endAngle,
             bool counterClockwise = false);
    void arcTo(float x1, float y1, float x2, float y2, float radius);
    void bezierCurveTo(float cp1x, float cp1y,
                       float cp2x, float cp2y,
                       float x, float y);
    void quadraticCurveTo(float cpx, float cpy, float x, float y);
    void ellipse(float x, float y, float radiusX, float radiusY,
                 float rotation, float startAngle, float endAngle,
                 bool counterClockwise = false);

    // Draw a generated material at (x, y). Handles all PixelFormat variants.
    void drawMaterial(float x, float y, const Material& material);

    // Like drawMaterial, but uploads the material only on first use and
    // reuses the cached texture afterwards, avoiding a full pixel-hash per
    // draw. The cache key is the material's id when set, otherwise its
    // address. The material must be immutable while cached; to refresh the
    // cached texture after regenerating pixels, call uploadMaterial() again
    // with the same id key (it replaces the old entry).
    void drawMaterialCached(float x, float y, const Material& material);

    // Key-based material cache. Allows callers to avoid expensive CPU work
    // (e.g. FreeType rasterization) when the same material has already been
    // uploaded. The key is an opaque caller-provided string.
    //
    //   checkMaterial(key) -> returns backend handle if cached, else NULL.
    //   uploadMaterial(key, material) -> uploads and registers under key.
    //   drawMaterial(key, x, y) -> draws a previously registered material.
    void* checkMaterial(const char* key) const;
    void* uploadMaterial(const char* key, const Material& material);
    void drawMaterial(const char* key, float x, float y);

    // Draw text using a loaded FreeType font.
    void drawText(float x, float y, const char* text, Font* font, const Color& color);

    // Draw a previously rendered target texture at (x, y) at its native size.
    void drawTexture(float x, float y, RenderTexture* texture);
    void drawTexture(float x, float y, RenderTexture* texture, BlendMode mode);

    // Set the current render target. Passing NULL selects the default target.
    void setRenderTarget(RenderTexture* target);

    // Clear the current render target to the given color.
    void clear(const Color& c);

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
    void drawMesh3D(float cx, float cy, float scale,
                    float rotX, float rotY, float rotZ,
                    const Vec3* vertices, int vertexCount,
                    const int* indices, int triangleCount,
                    const Color& color);

private:
    struct State {
        Color fillColor;
        Color strokeColor;
        float lineWidth;
        Affine2D transform;
        State()
            : fillColor(1, 1, 1, 1), strokeColor(0, 0, 0, 1), lineWidth(1.0f) {}
    };

    IRenderBackend* backend_;
    RenderTexture* currentTarget_;
    int width_;
    int height_;

    bool in3D_;
    void* lockedPixels_;
    int lockedPitch_;
    std::vector<float> depthBuffer_;
    // Per-pixel stamp of the 3D pair that last wrote the pixel. The z-test
    // treats only pixels stamped by the current pair as valid depth —
    // everything else counts as "cleared", so the depth/pixel buffers never
    // need explicit clearing. end3D zeroes unstamped pixels inside the
    // presented region so stale colors don't show through.
    std::vector<int> pairStamp3D_;
    int pairId3D_;
    // Bounding box of the pixels touched this pair; presented by end3D.
    int present3DX0_, present3DY0_, present3DX1_, present3DY1_;

    RenderMode renderMode_;
    bool batching_;
    State state_;
    std::vector<State> stateStack_;
    std::vector<Vec2> path_;
    bool pathClosed_;
    RenderQueue queue_;

    // Caller-provided key -> backend material handle cache.
    // This lets text and other generators skip re-rasterization on repeats.
    std::unordered_map<std::string, void*> materialKeyCache_;

    Vec2 applyTransform(float x, float y) const {
        return state_.transform * Vec2(x, y);
    }
    void applyTransform(std::vector<Vec2>& points) const;
};

} // namespace domi

#endif // DOMI_CANVAS2D_H
