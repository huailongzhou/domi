#ifndef DOMI_BACKEND_RENDER_BACKEND_H
#define DOMI_BACKEND_RENDER_BACKEND_H

#include "domi/core/math.h"
#include "domi/core/types.h"
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
    // Also accepts handles from createMutableTexture().
    virtual void destroyMaterial(void* handle) = 0;

    // -----------------------------------------------------------------
    // Mutable textures (e.g. font glyph atlases)
    // -----------------------------------------------------------------

    // Create an empty RGBA8888 texture that can be updated in place via
    // updateTextureRegion(). Unlike uploadMaterial() the result is NOT
    // deduplicated by content, so callers always get their own texture.
    // Destroy with destroyMaterial(). Handles left alive are cleaned up when
    // the backend itself is destroyed.
    virtual void* createMutableTexture(int width, int height) = 0;

    // Update a sub-region of a mutable texture with tightly packed
    // RGBA8888 pixels (R, G, B, A byte order, pitch == w * 4).
    virtual void updateTextureRegion(void* handle, int x, int y, int w, int h,
                                     const void* rgbaPixels) = 0;

    // Draw a sub-region of a material texture at (x, y) at the region's
    // native size, tinted by a multiplicative color modulation.
    // Rotation is in radians around (centerX, centerY) relative to (x, y).
    virtual void drawMaterialRegion(float x, float y, void* handle,
                                    int srcX, int srcY, int srcW, int srcH,
                                    const Color& tint,
                                    float angle = 0.0f,
                                    float centerX = 0.0f, float centerY = 0.0f,
                                    float scaleX = 1.0f, float scaleY = 1.0f) = 0;

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

    // Begin/end a 3D drawing block. The backend manages its own full-screen
    // 3D target, depth buffer, and compositing. These calls bracket all 3D
    // fill operations for a single frame.
    virtual void begin3D() = 0;
    virtual void end3D() = 0;

    // Fill a single triangle into the 3D target with z-buffering.
    // Coordinates are already in screen/target space.
    virtual void fillTriangle3D(const Vec2& a, const Vec2& b, const Vec2& c,
                                float za, float zb, float zc,
                                const Color& color) = 0;

    // Fill an axis-aligned rectangle (origin..origin+size) transformed by an
    // affine transform, with a single depth value. The SDL backend triangulates
    // it; a future 2D hardware backend can draw it directly as an affine rect.
    virtual void fillAffineRect3D(const Vec2& origin, const Vec2& size,
                                  const Affine2D& transform, float z,
                                  const Color& color) = 0;

    // Fill a general screen-space quadrilateral using a single depth value.
    // This is the primitive used by the voxel renderer: it avoids the
    // parallelogram approximation of fillAffineRect3D so perspective-distorted
    // voxel faces line up exactly with their neighbors.
    virtual void fillQuad3D(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                            const Vec2& p3, float z,
                            const Color& color) = 0;
};

} // namespace domi

#endif // DOMI_BACKEND_RENDER_BACKEND_H
