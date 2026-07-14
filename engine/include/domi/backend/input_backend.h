#ifndef DOMI_BACKEND_INPUT_BACKEND_H
#define DOMI_BACKEND_INPUT_BACKEND_H

namespace domi {

// Abstract input backend.
//
// Tracks keyboard, mouse and gamepad state. Platform event handling is done
// via handleEvent() with the native event pointer (e.g. SDL_Event*).
class IInputBackend {
public:
    virtual ~IInputBackend() {}

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Called once per frame before queries.
    virtual void update() = 0;

    // Feed a native input event. The pointer type is implementation-specific.
    virtual void handleEvent(const void* nativeEvent) = 0;

    virtual bool isKeyDown(int key) const = 0;
    virtual bool isKeyPressed(int key) const = 0;
    virtual bool isKeyReleased(int key) const = 0;

    virtual bool isMouseButtonDown(int button) const = 0;
    virtual bool isMouseButtonPressed(int button) const = 0;

    virtual float getMouseX() const = 0;
    virtual float getMouseY() const = 0;
    virtual float getMouseDeltaX() const = 0;
    virtual float getMouseDeltaY() const = 0;

    virtual float getAxis(const char* name) const = 0;
};

} // namespace domi

#endif // DOMI_BACKEND_INPUT_BACKEND_H
