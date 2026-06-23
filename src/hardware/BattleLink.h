#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// ============================================================
// PokeBug ESP-NOW 1v1 对战链路（房间制）
// 流程：广播发现房间 / 搜索房间 → 加入请求 → 加入确认 → 同步 → 回合 → 结算
// ============================================================

enum BattleMsgType : uint8_t {
    MSG_DISCOVER      = 0,   // 旧发现，已废弃
    MSG_DISCOVER_ACK  = 1,   // 旧发现确认，已废弃
    MSG_BATTLE_START  = 4,
    MSG_BATTLE_SYNC   = 5,
    MSG_BATTLE_ROUND  = 6,
    MSG_BATTLE_RESULT = 7,
    MSG_ROOM_ADVERT   = 8,   // 房主广播房间
    MSG_JOIN_REQ      = 9,   // 加入者请求加入
    MSG_JOIN_ACK      = 10,  // 房主确认/拒绝加入
    MSG_BATTLE_READY  = 11,  // 从机汇报本回合 MOT（含加油）
    MSG_ACK           = 0x80,
};

static constexpr uint8_t BATTLE_PROTOCOL_VERSION = 2;

// 房间广播包 8 bytes
struct __attribute__((packed)) room_advert_t {
    uint8_t type;
    uint8_t mac[6];
    uint8_t room_id;
};

// 加入请求包 8 bytes
struct __attribute__((packed)) join_req_t {
    uint8_t type;
    uint8_t mac[6];
    uint8_t room_id;
};

// 加入确认包 14 bytes
struct __attribute__((packed)) join_ack_t {
    uint8_t type;
    uint8_t host_mac[6];
    uint8_t client_mac[6];
    uint8_t accepted;  // 1=接受，0=拒绝
};

// 从机准备包 4 bytes（上报本回合 MOT，含加油）
struct __attribute__((packed)) battle_ready_t {
    uint8_t type;           // MSG_BATTLE_READY
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t round_num;
    uint8_t my_mot;         // 从机本回合 MOT
};

// 属性同步包 10 bytes
struct __attribute__((packed)) battle_sync_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t siz;
    uint8_t str;
    uint8_t end;
    uint8_t spi;
    uint8_t spd;
    uint8_t motivation;
    uint8_t hunger;
    uint8_t palette_id;
};

// 回合结果包 10 bytes（主机 authoritative，包含双方状态）
struct __attribute__((packed)) battle_round_t {
    uint8_t type;           // MSG_BATTLE_ROUND
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t round_num;
    uint8_t host_dmg;       // 主机对从机造成的伤害
    uint8_t client_dmg;     // 从机对主机造成的伤害
    uint8_t host_hp;        // 主机本回合后 HP
    uint8_t client_hp;      // 从机本回合后 HP
    uint8_t host_mot;       // 主机本回合后 MOT
    uint8_t client_mot;     // 从机本回合后 MOT
    uint8_t crits;          // bit0=主机暴击, bit1=从机暴击, bit2=主机攻击被闪避, bit3=从机攻击被闪避, bit4=主机先手
};

// 结算包 3 bytes
struct __attribute__((packed)) battle_result_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t win;  // 发送方视角：1=胜，0=负
};

// 确认包 2 bytes
struct __attribute__((packed)) ack_msg_t {
    uint8_t type;
    uint8_t acked_type;
};

class BattleLink {
public:
    static constexpr uint8_t MAX_ROOM_LIST = 4;
    struct RoomEntry {
        uint8_t mac[6];
        uint8_t room_id;
        uint32_t lastSeenMs;
    };

    static BattleLink& ins();

    // 初始化/反初始化 ESP-NOW + WiFi STA
    bool begin();
    void end();

    // ========== 房间阶段 ==========
    // 创建房间：开始广播 MSG_ROOM_ADVERT
    void startRoomHost(uint8_t roomId);
    // 搜索房间：开始监听 MSG_ROOM_ADVERT
    void startRoomSearch();
    // 停止房间广播/搜索
    void stopRoom();

    // 房主：取出一个待处理的加入请求
    bool takeJoinReq(join_req_t& out);
    // 房主：发送加入确认
    bool sendJoinAck(bool accept, const uint8_t* clientMac);

    // 加入者：获取当前扫描到的房间列表（数量可能为 0）
    uint8_t getRoomListCount() const;
    const RoomEntry* getRoomListEntry(uint8_t idx) const;

    // 加入者：向指定房间发送加入请求
    bool sendJoinReq(uint8_t roomId, const uint8_t* hostMac);
    // 加入者：取出加入确认
    bool takeJoinAck(join_ack_t& out);

    // ========== 对战连接 ==========
    // 设置对战角色与对端 MAC（由 LobbyScene 在进入 BattleScene 前调用）
    void setBattlePeer(const uint8_t* mac, bool asHost);
    bool isBattlePeerSet() const { return battlePeerSet; }
    bool isHost() const { return battleAsHost; }

    // 发送对战数据
    bool sendSync(const battle_sync_t& sync);
    bool sendReady(const battle_ready_t& ready);
    bool sendRound(const battle_round_t& round);
    bool sendResult(bool win);

    // 取出接收到的数据（非阻塞，取一次后清空）
    bool takeReceivedSync(battle_sync_t& out);
    bool takeReceivedReady(battle_ready_t& out);
    bool takeReceivedRound(battle_round_t& out);
    bool takeReceivedResult(bool& outWin);

    // 当前发送是否完成/成功
    bool isSending() const { return sendState == SendState::SENDING; }
    bool isSendIdle() const { return sendState == SendState::IDLE; }
    bool takeLastSendSuccess();

    // 轮询：处理广播、ACK 超时、重试、房间列表过期清理
    void update();

    // MAC 地址
    void getMyMac(uint8_t mac[6]) const { WiFi.macAddress(mac); }
    void getPeerMac(uint8_t mac[6]) const { memcpy(mac, battlePeerMac, 6); }

private:
    BattleLink() = default;

    static const uint8_t BROADCAST_MAC[6];
    static constexpr uint32_t ROOM_ADVERT_INTERVAL_MS = 500;
    static constexpr uint32_t ROOM_ENTRY_TIMEOUT_MS = 3000;
    static constexpr uint32_t ACK_TIMEOUT_MS = 300;   // 直连场景 ACK 通常很快，缩短超时以便快速重试
    static constexpr uint8_t  MAX_RETRIES = 5;

    bool initialized = false;
    bool sendBusy = false;
    bool currentSendTracked = false;

    // 房间阶段状态
    enum class RoomState { IDLE, HOSTING, SEARCHING };
    RoomState roomState = RoomState::IDLE;
    uint8_t hostedRoomId = 0;
    uint32_t lastRoomAdvertMs = 0;
    RoomEntry roomList[MAX_ROOM_LIST];

    // 对战连接状态
    bool battlePeerSet = false;
    bool battleAsHost = false;
    uint8_t battlePeerMac[6] = {0};

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
    bool pendingJoinReq = false;
    join_req_t pendingJoinReqData;
    bool pendingJoinAck = false;
    join_ack_t pendingJoinAckData;
    bool pendingReady = false;
    battle_ready_t pendingReadyData;

    // 待发送 ACK
    bool pendingAck = false;
    uint8_t pendingAckMac[6] = {0};
    uint8_t pendingAckType = 0;

    bool ensureUnicastPeer(const uint8_t mac[6]);
    bool ensureBroadcastPeer();
    bool sendInternal(const uint8_t mac[6], const uint8_t* data, uint8_t len);
    bool sendLobbyPacket(const uint8_t mac[6], const uint8_t* data, uint8_t len);
    bool sendAckPacket(const uint8_t mac[6], uint8_t ackedType);
    bool isBattlePeerMac(const uint8_t mac[6]) const;

    void addOrUpdateRoom(const uint8_t* mac, uint8_t roomId);
    void expireRooms();

    static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
    static void onSentStatic(const uint8_t* mac, esp_now_send_status_t status);
    void onRecv(const uint8_t* mac, const uint8_t* data, int len);
    void onSent(const uint8_t* mac, esp_now_send_status_t status);

    void handleRoomAdvert(const uint8_t* mac, const room_advert_t& advert);
    void handleJoinReq(const uint8_t* mac, const join_req_t& req);
    void handleJoinAck(const uint8_t* mac, const join_ack_t& ack);
    void handleReady(const uint8_t* mac, const battle_ready_t& ready);
    void handleSync(const uint8_t* mac, const battle_sync_t& sync);
    void handleRound(const uint8_t* mac, const battle_round_t& round);
    void handleResult(const uint8_t* mac, const battle_result_t& result);
    void handleAck(const uint8_t* mac, const ack_msg_t& ack);

    void queueAck(const uint8_t mac[6], uint8_t ackedType);
};
