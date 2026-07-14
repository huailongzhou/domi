#ifndef DOMI_BACKEND_WINDOW_BACKEND_H
#define DOMI_BACKEND_WINDOW_BACKEND_H

#include <string>

namespace domi {

// Abstract window / display backend.
//
// Responsible for creating the application window, managing the GPU claim
// state for 3D rendering, and reporting the drawable size.
class IWindowBackend {
public:
    virtual ~IWindowBackend() {}

    // Create/destroy the native window.
    virtual bool create(const std::string& title, int width, int height) = 0;
    virtual void destroy() = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    // Claim/release the window for native GPU rendering.
    // Returns true if the GPU is currently claimed.
    virtual bool claimGPU() = 0;
    virtual void releaseGPU() = 0;
    virtual bool isGPUClaimed() const = 0;

    // Opaque native GPU device handle, or NULL if GPU rendering is unavailable.
    // This is implementation-specific (e.g. SDL_GPUDevice*) and only used by
    // the 3D renderer while a matching backend is active.
    virtual void* getNativeGPUDevice() = 0;

    // Opaque native window handle (e.g. SDL_Window*). Used only by the 3D
    // renderer for swapchain acquisition.
    virtual void* getNativeWindow() = 0;
};

} // namespace domi

#endif // DOMI_BACKEND_WINDOW_BACKEND_H
