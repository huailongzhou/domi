#include "domi/window.h"
#include <cstdio>

namespace domi {

Window::Window() : window_(NULL), renderer_(NULL), gpuDevice_(NULL), gpuClaimed_(false), width_(0), height_(0) {}

Window::~Window() {
    destroy();
}

bool Window::create(const std::string& title, int width, int height) {
    width_ = width;
    height_ = height;

    window_ = SDL_CreateWindow(title.c_str(), width, height, 0);
    if (!window_) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        return false;
    }

    // Create a 2D renderer. SDL3 can use GPU/Metal/OpenGL internally.
    renderer_ = SDL_CreateRenderer(window_, NULL);
    if (!renderer_) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = NULL;
        return false;
    }

    // Enable vertical sync to avoid tearing/flickering on high-refresh displays.
    if (!SDL_SetRenderVSync(renderer_, 1)) {
        fprintf(stderr, "Failed to enable VSync: %s\n", SDL_GetError());
    }

    // Raise and focus the window so keyboard input is received immediately.
    SDL_RaiseWindow(window_);
    SDL_SetWindowKeyboardGrab(window_, true);

    // Also create a GPU device for 3D. Don't claim the window yet; the 2D
    // renderer owns the swapchain. 3D code will claim/release the window
    // via claimGPU/releaseGPU before/after GPU rendering.
    gpuDevice_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
    if (!gpuDevice_) {
        fprintf(stderr, "GPU device creation failed, 3D will be unavailable\n");
    }

    return true;
}

void Window::destroy() {
    if (gpuDevice_) {
        if (gpuClaimed_) {
            SDL_ReleaseWindowFromGPUDevice(gpuDevice_, window_);
            gpuClaimed_ = false;
        }
        SDL_DestroyGPUDevice(gpuDevice_);
        gpuDevice_ = NULL;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = NULL;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = NULL;
    }
}

bool Window::claimGPU() {
    if (!gpuDevice_ || gpuClaimed_) return gpuClaimed_;
    if (!SDL_ClaimWindowForGPUDevice(gpuDevice_, window_)) {
        fprintf(stderr, "Failed to claim window for GPU: %s\n", SDL_GetError());
        return false;
    }
    gpuClaimed_ = true;
    return true;
}

void Window::releaseGPU() {
    if (gpuDevice_ && gpuClaimed_) {
        SDL_ReleaseWindowFromGPUDevice(gpuDevice_, window_);
        gpuClaimed_ = false;
    }
}

void Window::swapBuffers() {
    if (renderer_) {
        SDL_RenderPresent(renderer_);
    }
    // GPU swap is handled by SDL_GPU swapchain
}

} // namespace domi
