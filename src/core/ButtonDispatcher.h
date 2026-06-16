#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>

// 按钮动作类型
enum class BtnAction {
    PRESSED,      // 短按（释放时触发）
    LONG_PRESS,   // 长按（达到阈值时触发一次）
};

// 按钮事件
struct ButtonEvent {
    uint8_t btn;          // 0=A, 1=B
    BtnAction action;
};

// 按钮事件分发器 — 观察者模式
// 监听者注册 handler，priority 越小优先级越高
// 一旦某个 handler 返回 true（已消费），停止后续分发
class ButtonDispatcher {
public:
    using Handler = std::function<bool(const ButtonEvent&)>;

    static ButtonDispatcher& ins();

    // 注册监听者，返回 handle 用于注销
    int subscribe(Handler h, int priority = 0);
    void unsubscribe(int handle);

    // 清除所有监听者
    void clear();

    // 每帧调用：读取按钮、生成事件、分发
    void poll();

private:
    ButtonDispatcher() = default;

    struct Entry {
        int handle;
        int priority;
        Handler handler;
    };
    std::vector<Entry> handlers;
    int nextHandle = 1;

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

    void processBtn(BtnState& s, uint32_t now, uint8_t btnId);
    bool dispatch(const ButtonEvent& ev);
};
