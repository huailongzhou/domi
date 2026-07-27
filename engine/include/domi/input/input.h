#ifndef DOMI_INPUT_H
#define DOMI_INPUT_H

#include <SDL3/SDL.h>

namespace domi {

class IInputBackend;

class InputSystem {
public:
    explicit InputSystem(IInputBackend* backend);
    ~InputSystem();

    bool init();
    void shutdown();
    void update();
    void handleEvent(const SDL_Event& e);

    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const;
    bool isKeyReleased(int key) const;

    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    float getMouseX() const;
    float getMouseY() const;
    float getMouseDeltaX() const;
    float getMouseDeltaY() const;

    // Mouse wheel scroll for the current frame.
    float getScrollX() const;
    float getScrollY() const;

    float getAxis(const char* name) const;

private:
    IInputBackend* backend_;
};

} // namespace domi

#endif
