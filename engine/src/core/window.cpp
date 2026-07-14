#include "domi/window.h"
#include "domi/backend/window_backend.h"

namespace domi {

Window::Window() : backend_(NULL) {}

Window::~Window() {
    destroy();
}

bool Window::init(IWindowBackend* backend) {
    backend_ = backend;
    return backend_ != NULL;
}

void Window::destroy() {
    backend_ = NULL;
}

bool Window::claimGPU() {
    return backend_ ? backend_->claimGPU() : false;
}

void Window::releaseGPU() {
    if (backend_) backend_->releaseGPU();
}

bool Window::isGPUClaimed() const {
    return backend_ ? backend_->isGPUClaimed() : false;
}

int Window::getWidth() const {
    return backend_ ? backend_->getWidth() : 0;
}

int Window::getHeight() const {
    return backend_ ? backend_->getHeight() : 0;
}

void Window::swapBuffers() {
    // Presentation is handled by the render backend.
}

} // namespace domi
