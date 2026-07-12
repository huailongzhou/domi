#ifndef DOMI_INPUT_H
#define DOMI_INPUT_H

#include <SDL3/SDL.h>
#include <cstdint>
#include <cstring>

namespace domi {

class InputSystem {
public:
    InputSystem();
    ~InputSystem();

    void update();
    void handleEvent(const SDL_Event& e);

    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const;
    bool isKeyReleased(int key) const;

    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    float getMouseX() const { return mouseX_; }
    float getMouseY() const { return mouseY_; }
    float getMouseDeltaX() const { return mouseDeltaX_; }
    float getMouseDeltaY() const { return mouseDeltaY_; }

    float getAxis(const char* name) const;

private:
    static const int MAX_KEYS = 512;
    static const int MAX_MOUSE = 8;

    bool keysCurr_[MAX_KEYS];
    bool keysPrev_[MAX_KEYS];
    bool mouseCurr_[MAX_MOUSE];
    bool mousePrev_[MAX_MOUSE];

    float mouseX_, mouseY_;
    float mouseDeltaX_, mouseDeltaY_;
    float scrollX_, scrollY_;
};

} // namespace domi

#endif
