#include "domi/input.h"
#include "domi/backend/input_backend.h"
#include <cstdio>

namespace domi {

InputSystem::InputSystem(IInputBackend* backend)
    : backend_(backend) {}

InputSystem::~InputSystem() {
    shutdown();
}

bool InputSystem::init() {
    if (!backend_) return false;
    return backend_->init();
}

void InputSystem::shutdown() {
    if (backend_) backend_->shutdown();
}

void InputSystem::update() {
    if (backend_) backend_->update();
}

void InputSystem::handleEvent(const SDL_Event& e) {
    if (backend_) backend_->handleEvent(&e);
}

bool InputSystem::isKeyDown(int key) const {
    return backend_ ? backend_->isKeyDown(key) : false;
}

bool InputSystem::isKeyPressed(int key) const {
    return backend_ ? backend_->isKeyPressed(key) : false;
}

bool InputSystem::isKeyReleased(int key) const {
    return backend_ ? backend_->isKeyReleased(key) : false;
}

bool InputSystem::isMouseButtonDown(int button) const {
    return backend_ ? backend_->isMouseButtonDown(button) : false;
}

bool InputSystem::isMouseButtonPressed(int button) const {
    return backend_ ? backend_->isMouseButtonPressed(button) : false;
}

float InputSystem::getMouseX() const {
    return backend_ ? backend_->getMouseX() : 0;
}

float InputSystem::getMouseY() const {
    return backend_ ? backend_->getMouseY() : 0;
}

float InputSystem::getMouseDeltaX() const {
    return backend_ ? backend_->getMouseDeltaX() : 0;
}

float InputSystem::getMouseDeltaY() const {
    return backend_ ? backend_->getMouseDeltaY() : 0;
}

float InputSystem::getAxis(const char* name) const {
    return backend_ ? backend_->getAxis(name) : 0;
}

} // namespace domi
