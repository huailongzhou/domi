#include "domi/input.h"
#include <cstdio>

namespace domi {

InputSystem::InputSystem()
    : mouseX_(0), mouseY_(0), mouseDeltaX_(0), mouseDeltaY_(0), scrollX_(0), scrollY_(0) {
    memset(keysCurr_, 0, sizeof(keysCurr_));
    memset(keysPrev_, 0, sizeof(keysPrev_));
    memset(mouseCurr_, 0, sizeof(mouseCurr_));
    memset(mousePrev_, 0, sizeof(mousePrev_));
}

InputSystem::~InputSystem() {}

void InputSystem::update() {
    memcpy(keysPrev_, keysCurr_, sizeof(keysCurr_));
    memcpy(mousePrev_, mouseCurr_, sizeof(mouseCurr_));
    mouseDeltaX_ = 0;
    mouseDeltaY_ = 0;
    scrollX_ = 0;
    scrollY_ = 0;
}

void InputSystem::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
        if (e.key.scancode < MAX_KEYS) keysCurr_[e.key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        if (e.key.scancode < MAX_KEYS) keysCurr_[e.key.scancode] = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button < MAX_MOUSE) mouseCurr_[e.button.button] = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button < MAX_MOUSE) mouseCurr_[e.button.button] = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mouseDeltaX_ = e.motion.xrel;
        mouseDeltaY_ = e.motion.yrel;
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        scrollX_ = e.wheel.x;
        scrollY_ = e.wheel.y;
        break;
    }
}

bool InputSystem::isKeyDown(int key) const {
    return key >= 0 && key < MAX_KEYS && keysCurr_[key];
}

bool InputSystem::isKeyPressed(int key) const {
    return key >= 0 && key < MAX_KEYS && keysCurr_[key] && !keysPrev_[key];
}

bool InputSystem::isKeyReleased(int key) const {
    return key >= 0 && key < MAX_KEYS && !keysCurr_[key] && keysPrev_[key];
}

bool InputSystem::isMouseButtonDown(int button) const {
    return button >= 0 && button < MAX_MOUSE && mouseCurr_[button];
}

bool InputSystem::isMouseButtonPressed(int button) const {
    return button >= 0 && button < MAX_MOUSE && mouseCurr_[button] && !mousePrev_[button];
}

float InputSystem::getAxis(const char* name) const {
    if (strcmp(name, "Horizontal") == 0) {
        float v = 0;
        if (isKeyDown(SDL_SCANCODE_D) || isKeyDown(SDL_SCANCODE_RIGHT)) v += 1;
        if (isKeyDown(SDL_SCANCODE_A) || isKeyDown(SDL_SCANCODE_LEFT)) v -= 1;
        return v;
    }
    if (strcmp(name, "Vertical") == 0) {
        float v = 0;
        if (isKeyDown(SDL_SCANCODE_W) || isKeyDown(SDL_SCANCODE_UP)) v -= 1;
        if (isKeyDown(SDL_SCANCODE_S) || isKeyDown(SDL_SCANCODE_DOWN)) v += 1;
        return v;
    }
    return 0;
}

} // namespace domi
