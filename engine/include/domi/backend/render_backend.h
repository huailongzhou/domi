#ifndef DOMI_BACKEND_RENDER_BACKEND_H
#define DOMI_BACKEND_RENDER_BACKEND_H

#include "domi/math.h"
#include "domi/types.h"
#include <vector>

namespace domi {

class RenderTexture;
struct Material;

// Supported formats for off-screen render targets.
// The backend maps each value to its native pixel format.
enum class RenderTextureFormat {
    RGBA8888
};

// Abstract 2D rendering backend.
//
// This interface encapsulates all platform-specific rendering operations so that
// the engine code (Canvas2D, RenderQueue, RenderPass, etc.) never calls SDL or
// any other graphics API directly. On embedded targets this can be replaced by
// a hardware-accelerated implementation without changing the rest of the engine.
class IRenderBackend {
public:
    virtual ~IRenderBackend() {}

    // -----------------------------------------------------------------
    // Target / viewport
    // -----------------------------------------------------------------

    // Set the current render target. Passing NULL selects the default screen
    // target. The backend is responsible for flushing pending work before
    // switching.
    virtual void setTarget(RenderTexture* target) = 0;

    // Clear the current target to the given color, ignoring blend mode.
    virtual void clear(const Color& c) = 0;

    // Set/reset a clip rectangle in target coordinates.
    virtual void setClipRect(float x, float y, float w, float h) = 0;
    virtual void resetClipRect() = 0;

    // Present the default target to the screen.
    virtual void present() = 0;

    // Drawable size of the default target.
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    // -----------------------------------------------------------------
    // Render state
    // -----------------------------------------------------------------

    virtual void setFillColor(const Color& c) = 0;
    virtual void setStrokeColor(const Color& c) = 0;
    virtual void setLineWidth(float w) = 0;

    // -----------------------------------------------------------------
    // Simple shapes
    // -----------------------------------------------------------------

    virtual void fillRect(float x, float y, float w, float h) = 0;
    virtual void strokeRect(float x, float y, float w, float h) = 0;
    virtual void drawLine(float x1, float y1, float x2, float y2) = 0;
    virtual void fillCircle(float x, float y, float radius, int segments) = 0;

    // -----------------------------------------------------------------
    // Paths (assumed convex for fill)
    // -----------------------------------------------------------------

    virtual void fillPath(const std::vector<Vec2>& points, bool closed,
                          const Color& color) = 0;
    virtual void strokePath(const std::vector<Vec2>& points, bool closed,
                            const Color& color, float lineWidth) = 0;

    // -----------------------------------------------------------------
    // Textures and materials
    // -----------------------------------------------------------------

    // Draw a previously rendered target texture at (x, y) at its native size.
    // Rotation is in radians around (centerX, centerY) relative to (x, y).
    // Scale factors are applied to the texture size.
    virtual void drawTexture(float x, float y, RenderTexture* texture,
                             BlendMode mode, float angle = 0.0f,
                             float centerX = 0.0f, float centerY = 0.0f,
                             float scaleX = 1.0f, float scaleY = 1.0f) = 0;

    // -----------------------------------------------------------------
    // Generated materials
    // -----------------------------------------------------------------

    // Upload a generated material to a native texture. The backend caches the
    // result keyed by material content so the same pixels are only uploaded once.
    // Returns an opaque handle to the cached texture.
    virtual void* uploadMaterial(const Material& material) = 0;

    // Draw a previously uploaded material at (x, y) using its cached handle.
    // Rotation is in radians around (centerX, centerY) relative to (x, y).
    // Scale factors are applied to the material size.
    virtual void drawMaterial(float x, float y, void* handle,
                              float angle = 0.0f,
                              float centerX = 0.0f, float centerY = 0.0f,
                              float scaleX = 1.0f, float scaleY = 1.0f) = 0;

    // Destroy a cached material texture and remove it from the cache.
    virtual void destroyMaterial(void* handle) = 0;

    // -----------------------------------------------------------------
    // Render target management
    // -----------------------------------------------------------------

    // Create/destroy a native texture suitable for use as a render target.
    // The returned handle is opaque to the engine and is stored in RenderTexture.
    virtual void* createRenderTarget(int width, int height,
                                     RenderTextureFormat format) = 0;
    virtual void destroyRenderTarget(void* handle) = 0;

    // -----------------------------------------------------------------
    // 3D software rasterizer support
    // -----------------------------------------------------------------

    // Lock the full-screen 3D target for CPU writing. Returns true on success.
    virtual bool lock3DTarget(void** pixels, int* pitch) = 0;
    virtual void unlock3DTarget() = 0;

    // Composite the 3D target onto the current target.
    virtual void present3DTarget() = 0;
};

} // namespace domi

#endif // DOMI_BACKEND_RENDER_BACKEND_H
