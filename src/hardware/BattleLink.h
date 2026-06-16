#pragma once
#include <Arduino.h>
#include <esp_now.h>

// ============================================================
// PokeBug ESP-NOW 1v1 对战链路
// 基于设计文档协议：广播发现 → 配对 → 同步 → 回合 → 结算
// ============================================================

enum BattleMsgType : uint8_t {
    MSG_DISCOVER      = 0,
    MSG_DISCOVER_ACK  = 1,
    MSG_BATTLE_START  = 4,
    MSG_BATTLE_SYNC   = 5,
    MSG_BATTLE_ROUND  = 6,
    MSG_BATTLE_RESULT = 7,
    MSG_ACK           = 0x80,
};

// 发现包 7 bytes
struct __attribute__((packed)) discover_msg_t {
    uint8_t type;
    uint8_t mac[6];
};

// 属性同步包 8 bytes
struct __attribute__((packed)) battle_sync_t {
    uint8_t type;
    uint8_t siz;
    uint8_t str;
    uint8_t end;
    uint8_t spi;
    uint8_t motivation;
    uint8_t hunger;
    uint8_t palette_id;
};

// 回合结果包 6 bytes
struct __attribute__((packed)) battle_round_t {
    uint8_t type;
    uint8_t round_num;
    uint8_t my_dmg;
    uint8_t my_hp;
    uint8_t my_mot;
    uint8_t my_crit;
};

// 结算包 2 bytes
struct __attribute__((packed)) battle_result_t {
    uint8_t type;
    uint8_t win;  // 发送方视角：1=胜，0=负
};

// 确认包 2 bytes
struct __attribute__((packed)) ack_msg_t {
    uint8_t type;
    uint8_t acked_type;
};

class BattleLink {
public:
    static BattleLink& ins();

    // 初始化/反初始化 ESP-NOW + WiFi STA
    bool begin();
    void end();

    // 发现阶段
    void startDiscovery();
    void stopDiscovery();
    bool isDiscovering() const { return discoveryActive; }
    bool isPeerConnected() const { return peerConnected; }

    // 连接状态
    enum class ConnectState {
        IDLE,
        DISCOVERING,
        CONNECTED,
        FAILED,
    };
    ConnectState getConnectState() const {
        if (peerConnected) return ConnectState::CONNECTED;
        if (discoveryActive) return ConnectState::DISCOVERING;
        if (connectFailed) return ConnectState::FAILED;
        return ConnectState::IDLE;
    }

    // 发送对战数据
    bool sendSync(const battle_sync_t& sync);
    bool sendRound(const battle_round_t& round);
    bool sendResult(bool win);

    // 取出接收到的数据（非阻塞，取一次后清空）
    bool takeReceivedSync(battle_sync_t& out);
    bool takeReceivedRound(battle_round_t& out);
    bool takeReceivedResult(bool& outWin);

    // 当前发送是否完成/成功
    bool isSending() const { return sendState == SendState::SENDING; }
    bool isSendIdle() const { return sendState == SendState::IDLE; }
    bool takeLastSendSuccess();

    // 轮询：处理发现广播、ACK 超时、重试
    void update();

private:
    BattleLink() = default;

    static const uint8_t BROADCAST_MAC[6];
    static constexpr uint32_t DISCOVER_TIMEOUT_MS = 10000;
    static constexpr uint32_t DISCOVER_INTERVAL_MS = 500;
    static constexpr uint32_t ACK_TIMEOUT_MS = 1500;
    static constexpr uint8_t  MAX_RETRIES = 3;

    bool initialized = false;
    bool sendBusy = false;

    // 发现/连接
    bool discoveryActive = false;
    bool peerConnected = false;
    bool connectFailed = false;
    uint32_t discoveryStartMs = 0;
    uint32_t lastDiscoverBroadcastMs = 0;
    uint8_t peerMac[6] = {0};

    // 发送状态机
    enum class SendState { IDLE, SENDING, SUCCESS, FAIL };
    SendState sendState = SendState::IDLE;

    struct SendContext {
        uint8_t targetMac[6] = {0};
        uint8_t buf[32] = {0};
        uint8_t len = 0;
        uint8_t retryCount = 0;
        uint32_t startMs = 0;
        bool ackReceived = false;
    };
    SendContext sendCtx;

    // 接收缓冲
    bool pendingSync = false;
    battle_sync_t pendingSyncData;
    bool pendingRound = false;
    battle_round_t pendingRoundData;
    bool pendingResult = false;
    bool pendingResultWin = false;

    // 待发送 ACK
    bool pendingAck = false;
    uint8_t pendingAckMac[6] = {0};
    uint8_t pendingAckType = 0;

    bool ensureUnicastPeer(const uint8_t mac[6]);
    bool ensureBroadcastPeer();
    bool sendInternal(const uint8_t mac[6], const uint8_t* data, uint8_t len);

    static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
    static void onSentStatic(const uint8_t* mac, esp_now_send_status_t status);
    void onRecv(const uint8_t* mac, const uint8_t* data, int len);
    void onSent(const uint8_t* mac, esp_now_send_status_t status);

    void handleDiscover(const uint8_t* mac, const uint8_t* data, uint8_t len);
    void handleDiscoverAck(const uint8_t* mac, const uint8_t* data, uint8_t len);
    void handleSync(const uint8_t* mac, const battle_sync_t& sync);
    void handleRound(const uint8_t* mac, const battle_round_t& round);
    void handleResult(const uint8_t* mac, const battle_result_t& result);
    void handleAck(const uint8_t* mac, const ack_msg_t& ack);

    void queueAck(const uint8_t mac[6], uint8_t ackedType);
};
