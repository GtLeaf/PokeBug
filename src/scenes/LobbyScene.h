#pragma once
#include "../core/Scene.h"
#include "../hardware/BattleLink.h"

// 对战大厅：创建房间 / 搜索房间
class LobbyScene : public Scene {
public:
    LobbyScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    enum class State {
        MODE_SELECT,     // 选择创建/搜索
        HOST_WAITING,    // 创建房间后等待加入
        SEARCH_SCANNING, // 正在搜索房间
        SEARCH_LIST,     // 显示房间列表
        JOINING,         // 已发送加入请求，等待确认
        MESSAGE,         // 显示提示信息（如拒绝、超时）
    };
    State state = State::MODE_SELECT;

    enum class Mode {
        NONE,
        CREATE,
        SEARCH,
        BACK,
    };
    Mode selectedMode = Mode::NONE;

    uint8_t roomId = 0;              // 本机创建的房间号
    uint32_t stateStartMs = 0;       // 当前状态开始时间
    uint32_t lastScanLogMs = 0;      // 搜索日志节流
    uint8_t selectedRoomIdx = 0;     // 列表中选中的房间索引
    uint8_t roomCount = 0;           // 上次扫描到的房间数
    const char* messageText = nullptr; // MESSAGE 状态要显示的文本

    void enterModeSelect(Mode defaultMode = Mode::CREATE);
    void enterHostWaiting();
    void enterSearchScanning();
    void enterSearchList();
    void enterJoining(uint8_t idx);
    void enterMessage(const char* text);

    void drawModeSelect();
    void drawHostWaiting();
    void drawSearchScanning();
    void drawSearchList();
    void drawJoining();
    void drawMessage();

    void drawSettingsStyleList(const char* title, const char* const* items, uint8_t count, uint8_t cursor);
    void drawListItem(int y, const char* text, bool selected, bool last);

    static Mode nextMode(Mode m);

    static constexpr uint32_t HOST_TIMEOUT_MS = 30000;
    static constexpr uint32_t SEARCH_SCAN_MS = 1500;
    static constexpr uint32_t JOIN_TIMEOUT_MS = 5000;
    static constexpr uint32_t MESSAGE_MS = 1500;
};
