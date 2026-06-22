#pragma once
#include <Arduino.h>
#include <cmath>
#include <cstdint>

// 对战公式计算
class BattleCalc {
public:
    struct BattleStats {
        uint8_t siz = 0;
        uint8_t str = 0;
        uint8_t end = 0;
        uint8_t spd = 0;
        uint8_t spi = 0;
        uint8_t mot = 0;
    };

    struct AttackResult {
        uint8_t damage;
        bool crit;
        bool dodged;
    };

    // 总 HP
    static int computeHp(uint8_t siz, uint8_t end) {
        return 20 + siz * 3 + end * 2;
    }

    static int computeHp(const BattleStats& stats) {
        return computeHp(stats.siz, stats.end);
    }

    // 先手值：SPD 高者更容易先手；MOT 与少量节奏随机避免 +1 SPD 永久锁先手。
    static int computeInitiative(uint8_t spd, uint8_t mot) {
        return spd * 6 + mot + (int)random(-8, 9);
    }

    static int computeInitiative(const BattleStats& stats) {
        return computeInitiative(stats.spd, stats.mot);
    }

    static AttackResult computeAttack(const BattleStats& attacker, const BattleStats& defender) {
        return computeAttackRaw(attacker.str, attacker.siz,
                                attacker.spi, attacker.spd,
                                defender.end, defender.spi, defender.spd,
                                attacker.mot);
    }

    // 每回合 MOT 自然衰减
    static int computeMotLoss(uint8_t spi) {
        int loss = (int)roundf(9.0f - spi / 3.0f);
        if (loss < 2) loss = 2;
        if (loss > 9) loss = 9;
        return loss;
    }

private:
    // 本次攻击结果：包含伤害、暴击、闪避。闪避时 damage=0。
    static AttackResult computeAttackRaw(uint8_t attackerStr,
                                         uint8_t attackerSiz,
                                         uint8_t attackerSpi,
                                         uint8_t attackerSpd,
                                         uint8_t defenderEnd,
                                         uint8_t defenderSpi,
                                         uint8_t defenderSpd,
                                         uint8_t mot) {
        AttackResult result{1, false, false};

        float dodgeRate = 0.0f;
        int spdDiff = (int)defenderSpd - (int)attackerSpd;
        if (spdDiff > 0) dodgeRate += spdDiff * 3.0f;
        dodgeRate += defenderSpi / 8.0f;
        dodgeRate = clampFloat(dodgeRate, 0.0f, 22.0f);
        if (rollPercent(dodgeRate)) {
            result.damage = 0;
            result.dodged = true;
            return result;
        }

        static constexpr float STR_BASE_POWER = 1.5f;
        static constexpr float STR_DAMAGE_SLOPE = 0.80f; // 小幅削弱 STR 单点边际收益
        float sizeBonus = 1.0f + 0.45f * attackerSiz / (attackerSiz + 8.0f);
        float motMult = 0.75f + mot / 200.0f;
        float strPower = STR_BASE_POWER + attackerStr * STR_DAMAGE_SLOPE;
        float raw = strPower * sizeBonus * motMult;

        float critRate = 5.0f + attackerSpi * 2.2f + attackerSpd * 0.5f - defenderSpi * 1.5f;
        critRate = clampFloat(critRate, 5.0f, 55.0f);
        result.crit = rollPercent(critRate);
        float critMult = result.crit ? 1.35f : 1.0f;

        float mitigation = 1.0f - minFloat(0.14f, defenderEnd * 0.0045f);
        float reduction = defenderEnd * 0.18f;
        int damage = (int)roundf(raw * critMult * mitigation - reduction);
        if (damage < 1) damage = 1;
        if (damage > 255) damage = 255;
        result.damage = (uint8_t)damage;
        return result;
    }
    static float minFloat(float a, float b) {
        return a < b ? a : b;
    }

    static float clampFloat(float value, float minValue, float maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    static bool rollPercent(float rate) {
        return random(10000) < (long)(rate * 100.0f);
    }
};
