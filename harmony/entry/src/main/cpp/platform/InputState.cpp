// browser/harmony/entry/src/main/cpp/platform/InputState.cpp
#include "InputState.h"

namespace prism {

void InputState::pushPointer(float x, float y, PointerAction action, int button) {
    std::lock_guard<std::mutex> lock(mtx_);
    pointer_.push_back(PointerEvent{x, y, action, button});
}

void InputState::pushKey(int code, bool pressed) {
    std::lock_guard<std::mutex> lock(mtx_);
    keys_.push_back(KeyEventData{code, pressed});
}

void InputState::pushScroll(float dy) {
    std::lock_guard<std::mutex> lock(mtx_);
    scroll_ += dy;
}

std::vector<PointerEvent> InputState::takePointer() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<PointerEvent> out;
    out.swap(pointer_);
    return out;
}

std::vector<KeyEventData> InputState::takeKeys() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<KeyEventData> out;
    out.swap(keys_);
    return out;
}

float InputState::takeScroll() {
    std::lock_guard<std::mutex> lock(mtx_);
    float v = scroll_;
    scroll_ = 0.f;
    return v;
}

} // namespace prism
