#pragma once
#include <cstdint>

// 按钮动作类型
enum class BtnAction {
    DOWN,         // 按下（消抖后触发）
    UP,           // 松开（消抖后触发）
    PRESSED,      // 短按（释放时触发，且未触发长按）
    LONG_PRESS,   // 长按（达到阈值时触发一次）
};

// 按钮事件
struct ButtonEvent {
    uint8_t btn;          // 0=A, 1=B
    BtnAction action;
};

// 按钮事件生成器：只负责读取硬件、消抖，并产出标准按钮事件。
// 事件路由由 GameEngine 统一处理，避免多个监听者之间的优先级和生命周期复杂度。
class ButtonDispatcher {
public:
    static constexpr uint8_t MAX_EVENTS_PER_POLL = 6;

    static ButtonDispatcher& ins();

    // 每帧调用：读取按钮并写入本次产生的事件，返回事件数量。
    uint8_t poll(ButtonEvent* events, uint8_t maxEvents);

private:
    ButtonDispatcher() = default;

    struct BtnState {
        bool raw = false;
        bool debounced = false;
        uint32_t debounceTime = 0;
        uint32_t pressTime = 0;
        uint32_t holdDuration = 0;
        bool longTriggered = false;
    };
    BtnState states[2];
    bool prevDebounced[2] = {false, false};

    static constexpr uint32_t DEBOUNCE_MS = 20;
    static constexpr uint32_t LONG_PRESS_MS = 500;  // PokeBug 长按阈值 500ms

    void processBtn(BtnState& s, uint32_t now, uint8_t btnId,
                    ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount);
    static void emit(ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount,
                     uint8_t btnId, BtnAction action);
};
