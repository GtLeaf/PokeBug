#include "ButtonDispatcher.h"
#include "../hardware/Hal.h"

ButtonDispatcher& ButtonDispatcher::ins() {
    static ButtonDispatcher instance;
    return instance;
}

int ButtonDispatcher::subscribe(Handler h, int priority) {
    int hdl = nextHandle++;
    handlers.push_back({hdl, priority, h});
    std::sort(handlers.begin(), handlers.end(), [](const Entry& a, const Entry& b) {
        return a.priority < b.priority;
    });
    return hdl;
}

void ButtonDispatcher::unsubscribe(int handle) {
    for (auto it = handlers.begin(); it != handlers.end(); ++it) {
        if (it->handle == handle) {
            handlers.erase(it);
            return;
        }
    }
}

void ButtonDispatcher::clear() {
    handlers.clear();
}

void ButtonDispatcher::poll() {
    uint32_t now = Hal::ins().millis();
    states[0].raw = Hal::ins().btnA_raw();
    states[1].raw = Hal::ins().btnB_raw();

    processBtn(states[0], now, 0);
    processBtn(states[1], now, 1);
}

void ButtonDispatcher::processBtn(BtnState& s, uint32_t now, uint8_t btnId) {
    // 消抖处理
    if (s.raw != s.debounced) {
        if (s.debounceTime == 0) {
            s.debounceTime = now;
        } else if (now - s.debounceTime >= DEBOUNCE_MS) {
            s.debounced = s.raw;
            s.debounceTime = 0;
        }
    } else {
        s.debounceTime = 0;
    }

    // 边沿检测
    bool edgePressed = (s.debounced && !prevDebounced[btnId]);
    bool edgeReleased = (!s.debounced && prevDebounced[btnId]);

    if (edgePressed) {
        s.pressTime = now;
        s.longTriggered = false;
    }

    if (s.debounced) {
        s.holdDuration = now - s.pressTime;
    }

    // 长按检测
    if (s.debounced && !s.longTriggered && s.holdDuration >= LONG_PRESS_MS) {
        s.longTriggered = true;
        ButtonEvent ev{btnId, BtnAction::LONG_PRESS};
        bool consumed = dispatch(ev);
        prevDebounced[btnId] = s.debounced;
        if (consumed) return;
    }

    // 短按检测（释放时，且未触发过长按）
    if (edgeReleased && !s.longTriggered) {
        ButtonEvent ev{btnId, BtnAction::PRESSED};
        dispatch(ev);
    }

    prevDebounced[btnId] = s.debounced;
}

bool ButtonDispatcher::dispatch(const ButtonEvent& ev) {
    for (const auto& entry : handlers) {
        if (entry.handler(ev)) {
            return true;  // 已消费，停止分发
        }
    }
    return false;
}
