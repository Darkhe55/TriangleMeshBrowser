// browser/harmony/entry/src/main/cpp/platform/InputState.h
// 线程安全的输入事件收集: XComponent 回调线程写入, 渲染线程消费。
#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace prism {

enum class PointerAction { Down, Up, Move, Cancel };

struct PointerEvent {
    float x = 0.f;
    float y = 0.f;
    PointerAction action = PointerAction::Move;
    int button = 0;
};

struct KeyEventData {
    int  code = 0;
    bool pressed = false;
};

class InputState {
public:
    void pushPointer(float x, float y, PointerAction action, int button);
    void pushKey(int code, bool pressed);
    void pushScroll(float dy);

    std::vector<PointerEvent> takePointer();
    std::vector<KeyEventData> takeKeys();
    float takeScroll();

private:
    std::mutex mtx_;
    std::vector<PointerEvent> pointer_;
    std::vector<KeyEventData> keys_;
    float scroll_ = 0.f;
};

} // namespace prism
