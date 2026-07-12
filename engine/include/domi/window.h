#ifndef DOMI_WINDOW_H
#define DOMI_WINDOW_H

#include <SDL3/SDL.h>
#include <string>

namespace domi {

class Window {
public:
    Window();
    ~Window();

    bool create(const std::string& title, int width, int height);
    void destroy();

    SDL_Window* getSDLWindow() const { return window_; }
    SDL_Renderer* getSDLRenderer() const { return renderer_; }
    SDL_GPUDevice* getGPUDevice() const { return gpuDevice_; }

    bool claimGPU();
    void releaseGPU();
    bool isGPUClaimed() const { return gpuClaimed_; }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    void swapBuffers();

private:
    SDL_Window* window_;
    SDL_Renderer* renderer_;     // 2D renderer
    SDL_GPUDevice* gpuDevice_;   // 3D GPU device
    bool gpuClaimed_;
    int width_, height_;
};

} // namespace domi

#endif
