#include "ButtonDispatcher.h"
#include "../hardware/Hal.h"

ButtonDispatcher& ButtonDispatcher::ins() {
    static ButtonDispatcher instance;
    return instance;
}

uint8_t ButtonDispatcher::poll(ButtonEvent* events, uint8_t maxEvents) {
    uint32_t now = Hal::ins().millis();
    states[0].raw = Hal::ins().btnA_raw();
    states[1].raw = Hal::ins().btnB_raw();

    uint8_t eventCount = 0;
    processBtn(states[0], now, 0, events, maxEvents, eventCount);
    processBtn(states[1], now, 1, events, maxEvents, eventCount);
    return eventCount;
}

void ButtonDispatcher::processBtn(BtnState& s, uint32_t now, uint8_t btnId,
                                  ButtonEvent* events, uint8_t maxEvents,
                                  uint8_t& eventCount) {
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

    bool edgePressed = (s.debounced && !prevDebounced[btnId]);
    bool edgeReleased = (!s.debounced && prevDebounced[btnId]);

    if (edgePressed) {
        s.pressTime = now;
        s.longTriggered = false;
        emit(events, maxEvents, eventCount, btnId, BtnAction::DOWN);
    }

    if (s.debounced) {
        s.holdDuration = now - s.pressTime;
    }

    if (s.debounced && !s.longTriggered && s.holdDuration >= LONG_PRESS_MS) {
        s.longTriggered = true;
        emit(events, maxEvents, eventCount, btnId, BtnAction::LONG_PRESS);
        prevDebounced[btnId] = s.debounced;
        return;
    }

    if (edgeReleased) {
        emit(events, maxEvents, eventCount, btnId, BtnAction::UP);
        if (!s.longTriggered) {
            emit(events, maxEvents, eventCount, btnId, BtnAction::PRESSED);
        }
    }

    prevDebounced[btnId] = s.debounced;
}

void ButtonDispatcher::emit(ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount,
                            uint8_t btnId, BtnAction action) {
    if (!events || eventCount >= maxEvents) return;
    events[eventCount++] = ButtonEvent{btnId, action};
}
