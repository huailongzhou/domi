#ifndef DOMI_WINDOW_H
#define DOMI_WINDOW_H

#include <string>

namespace domi {

class IWindowBackend;

// Application window facade.
//
// Delegates all platform-specific window operations to an IWindowBackend
// implementation. This keeps the engine core free of SDL or other native APIs.
class Window {
public:
    Window();
    ~Window();

    // Initialize with a window backend. The backend must outlive the Window.
    bool init(IWindowBackend* backend);
    void destroy();

    IWindowBackend* getBackend() const { return backend_; }

    bool claimGPU();
    void releaseGPU();
    bool isGPUClaimed() const;

    int getWidth() const;
    int getHeight() const;

    void swapBuffers();

private:
    IWindowBackend* backend_;
};

} // namespace domi

#endif // DOMI_WINDOW_H
