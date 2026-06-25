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
    MSG_BATTLE_READY  = 11,  // 从机汇报节奏输入后的 MOT
    MSG_GIFT_ITEM     = 12,  // 礼物道具
    MSG_VISIT_RECALL  = 13,  // 访客主动召回甲虫
    MSG_VISIT_PING    = 14,  // Visit 心跳，用于检测房主是否仍可达
    MSG_VISIT_STATUS  = 15,  // 房主权威 Visit 状态
    MSG_VISIT_INTENT  = 16,  // 访客侧互动意图
    MSG_VISIT_EAT_RESULT = 17, // 房主判定访客进食结果
    MSG_ACK           = 0x80,
};

static constexpr uint8_t BATTLE_PROTOCOL_VERSION = 6;

enum RoomPurpose : uint8_t {
    ROOM_PURPOSE_BATTLE = 0,
    ROOM_PURPOSE_GIFT   = 1,
    ROOM_PURPOSE_VISIT  = 2,
};

// 房间广播包 9 bytes
struct __attribute__((packed)) room_advert_t {
    uint8_t type;
    uint8_t mac[6];
    uint8_t room_id;
    uint8_t purpose;
};

// 加入请求包 9 bytes
struct __attribute__((packed)) join_req_t {
    uint8_t type;
    uint8_t mac[6];
    uint8_t room_id;
    uint8_t purpose;
};

// 加入确认包 14 bytes
struct __attribute__((packed)) join_ack_t {
    uint8_t type;
    uint8_t host_mac[6];
    uint8_t client_mac[6];
    uint8_t accepted;  // 1=接受，0=拒绝
};

// 从机节奏更新包 4 bytes（玩家按 A 后上报本机当前 MOT；不再驱动 ATB 时机）
struct __attribute__((packed)) battle_ready_t {
    uint8_t type;           // MSG_BATTLE_READY
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t round_num;      // 产生输入时的主机回合号，过期则丢弃
    uint8_t my_mot;         // 从机当前 MOT（含节奏加成）
};

// 属性同步包 13 bytes
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
    uint8_t visit_speed_x10;
    uint8_t str_cap;
    uint8_t temperament;
};

// 回合结果包 12 bytes（主机 authoritative，包含双方状态与 ATB 快照）
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
    uint8_t host_gauge;     // 主机权威 ATB 快照，0-100
    uint8_t client_gauge;   // 从机权威 ATB 快照，0-100
};

// 结算包 3 bytes
struct __attribute__((packed)) battle_result_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t win;  // 发送方视角：1=胜，0=负
};

// 礼物包 7 bytes：item_id 使用 ItemCatalog 编码，当前 UI 只允许 Food。
struct __attribute__((packed)) gift_item_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint16_t transfer_id;   // 同一 peer 下用于重试去重
    uint16_t item_id;
    uint8_t amount;
};

struct __attribute__((packed)) visit_recall_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
};

struct __attribute__((packed)) visit_ping_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t hunger;         // 访客本机当前 HUN，用于房主 UI 同步
    uint8_t motivation;     // 访客本机当前 MOT，用于房主 UI 同步
};

struct __attribute__((packed)) visit_status_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t flags;          // bit0=active
    uint16_t remaining_s;   // 主机权威剩余秒数
    uint16_t duration_s;    // 主机权威总秒数
    uint8_t speed_x10;      // 主机游戏速度 * 10
    uint8_t guest_eat_count;
    uint8_t guest_play_count;
};

enum VisitIntentCode : uint8_t {
    VISIT_INTENT_PLAY = 1,
    VISIT_INTENT_EAT  = 2,
};

struct __attribute__((packed)) visit_intent_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t intent;
};

struct __attribute__((packed)) visit_eat_result_t {
    uint8_t type;
    uint8_t version;        // BATTLE_PROTOCOL_VERSION
    uint8_t success;
    uint8_t hunger_gain;
    uint8_t new_guest_hunger;
    uint8_t food_type;
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
        uint8_t purpose;
        uint32_t lastSeenMs;
    };

    static BattleLink& ins();

    // 初始化/反初始化 ESP-NOW + WiFi STA
    bool begin();
    void end();

    // ========== 房间阶段 ==========
    // 创建房间：开始广播 MSG_ROOM_ADVERT
    void startRoomHost(uint8_t roomId, uint8_t purpose = ROOM_PURPOSE_BATTLE);
    // 搜索房间：开始监听 MSG_ROOM_ADVERT
    void startRoomSearch(uint8_t purpose = ROOM_PURPOSE_BATTLE);
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
    void setVisitPowerSave(bool enabled);

    // 发送对战数据
    bool sendSync(const battle_sync_t& sync);
    bool sendReady(const battle_ready_t& ready);
    bool sendRound(const battle_round_t& round);
    bool sendResult(bool win);
    bool sendGiftItem(uint16_t itemId, uint8_t amount);
    bool sendVisitRecall();
    bool sendVisitPing(uint8_t hunger, uint8_t motivation);
    bool sendVisitStatus(uint32_t remainingMs, uint32_t durationMs, uint8_t speedX10, bool active,
                         uint8_t guestEatCount = 0, uint8_t guestPlayCount = 0);
    bool sendVisitIntent(uint8_t intent);
    bool sendVisitEatResult(bool success, uint8_t hungerGain, uint8_t newGuestHunger, uint8_t foodType);

    // 取出接收到的数据（非阻塞，取一次后清空）
    bool takeReceivedSync(battle_sync_t& out);
    bool takeReceivedReady(battle_ready_t& out);
    bool takeReceivedRound(battle_round_t& out);
    bool takeReceivedResult(bool& outWin);
    bool takeReceivedGift(gift_item_t& out);
    bool takeReceivedVisitRecall();
    bool takeReceivedVisitVitals(uint8_t& outHunger, uint8_t& outMotivation);
    bool takeReceivedVisitStatus(visit_status_t& out);
    bool takeReceivedVisitIntent(uint8_t& outIntent);
    bool takeReceivedVisitEatResult(visit_eat_result_t& out);

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
    static constexpr uint32_t VISIT_ACK_TIMEOUT_MS = 700;
    static constexpr uint8_t  MAX_RETRIES = 5;

    bool initialized = false;
    bool sendBusy = false;
    bool currentSendTracked = false;
    bool visitPowerSaveActive = false;

    // 房间阶段状态
    enum class RoomState { IDLE, HOSTING, SEARCHING };
    RoomState roomState = RoomState::IDLE;
    uint8_t hostedRoomId = 0;
    uint8_t roomPurpose = ROOM_PURPOSE_BATTLE;
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
    bool pendingGift = false;
    gift_item_t pendingGiftData;
    bool pendingVisitRecall = false;
    bool pendingVisitVitals = false;
    uint8_t pendingVisitHunger = 100;
    uint8_t pendingVisitMotivation = 50;
    bool pendingVisitStatus = false;
    visit_status_t pendingVisitStatusData;
    bool pendingVisitIntent = false;
    uint8_t pendingVisitIntentData = 0;
    bool pendingVisitEatResult = false;
    visit_eat_result_t pendingVisitEatResultData;
    bool lastGiftSeen = false;
    uint8_t lastGiftPeerMac[6] = {0};
    uint16_t lastGiftTransferId = 0;

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
    void handleGiftItem(const uint8_t* mac, const gift_item_t& gift);
    void handleVisitRecall(const uint8_t* mac, const visit_recall_t& recall);
    void handleVisitPing(const uint8_t* mac, const visit_ping_t& ping);
    void handleVisitStatus(const uint8_t* mac, const visit_status_t& status);
    void handleVisitIntent(const uint8_t* mac, const visit_intent_t& intent);
    void handleVisitEatResult(const uint8_t* mac, const visit_eat_result_t& result);
    void handleAck(const uint8_t* mac, const ack_msg_t& ack);

    void queueAck(const uint8_t mac[6], uint8_t ackedType);
};
