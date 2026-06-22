#pragma once
#include <Arduino.h>
#include "Bug.h"
#include "../assets/NpcData.h"

// ============================================================
// NPC 生成器
// 按档位权重随机选择 NPC，属性基于玩家遭遇瞬间快照乘以文档倍率。
// 返回的 NpcCombatant 可直接用于 BattleScene 的本地 NPC 对战。
// ============================================================

struct NpcCombatant {
    uint8_t siz = 1, str = 1, end = 1, spd = 1, spi = 1;
    uint8_t mot = 50;
    uint8_t palette = 0;
    uint8_t index = 0;           // NpcData::ENTRIES 索引
    NpcData::Tier tier = NpcData::Tier::ROOKIE;
};

class NpcGenerator {
public:
    // 按探索档位权重生成一个 NPC
    static NpcCombatant generateForExplore(const Bug& player) {
        return generate(player, NpcData::EXPLORER_TIER_WEIGHTS);
    }

    // 按杯赛档位权重生成一个 NPC
    static NpcCombatant generateForCup(const Bug& player) {
        return generate(player, NpcData::CUP_TIER_WEIGHTS);
    }

private:
    static constexpr uint8_t NPC_STAT_MAX = 24; // 战斗安全上限，不再把基因潜力硬截到 10

    static NpcCombatant generate(const Bug& player, const uint8_t weights[4]) {
        // 1. 按权重选档位
        int total = weights[0] + weights[1] + weights[2] + weights[3];
        int roll = random(total);
        NpcData::Tier tier = NpcData::Tier::ROOKIE;
        int acc = 0;
        for (int i = 0; i < 4; i++) {
            acc += weights[i];
            if (roll < acc) {
                tier = (NpcData::Tier)i;
                break;
            }
        }

        // 2. 在该档位内随机选一个 NPC
        uint8_t indices[3];
        uint8_t count = 0;
        for (uint8_t i = 0; i < NpcData::COUNT; i++) {
            NpcData::Tier t = (NpcData::Tier)pgm_read_byte(&NpcData::ENTRIES[i].tier);
            if (t == tier) indices[count++] = i;
        }
        uint8_t idx = indices[random(count)];

        // 3. 生成属性：基于玩家遭遇瞬间快照，按档位倍率随机
        float multMin = 0.5f, multMax = 0.8f;
        switch (tier) {
            case NpcData::Tier::ROOKIE:  multMin = 0.5f; multMax = 0.8f; break;
            case NpcData::Tier::NORMAL:  multMin = 0.8f; multMax = 1.2f; break;
            case NpcData::Tier::VETERAN: multMin = 1.2f; multMax = 1.6f; break;
            case NpcData::Tier::LEGEND:  multMin = 1.8f; multMax = 2.5f; break;
            default: break;
        }

        auto scale = [&](float playerVal) -> uint8_t {
            float f = playerVal * randomFloat(multMin, multMax);
            int v = (int)roundf(f);
            if (v < 1) v = 1;
            if (v > NPC_STAT_MAX) v = NPC_STAT_MAX;
            return (uint8_t)v;
        };

        NpcCombatant npc;
        npc.siz = scale(player.getSiz());
        npc.str = scale(player.getStr());
        npc.end = scale(player.getEnd());
        npc.spd = scale(player.getSpd());
        npc.spi = scale(player.getSpi());
        npc.mot = (uint8_t)(50 + random(-15, 16));
        npc.index = idx;
        npc.tier = tier;

        // 调色板：传说级优先特殊光效（金/白化/虹彩），其他随机
        if (tier == NpcData::Tier::LEGEND) {
            npc.palette = random(3) + 1; // 1=金,2=白化,3=虹彩（与玩家 paletteId 0-3 对应）
        } else {
            npc.palette = random(4);
        }
        return npc;
    }

    static float randomFloat(float min, float max) {
        return min + (max - min) * (random(10001) / 10000.0f);
    }
};
