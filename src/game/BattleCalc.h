#pragma once
#include <cstdint>

// 对战公式计算
class BattleCalc {
public:
    // 总 HP
    static int computeHp(uint8_t siz, uint8_t end) {
        return 20 + siz * 3 + end * 2;
    }

    // 先手值：SPD 高者先手；用于每回合开局比较
    static int computeInitiative(uint8_t spd, uint8_t mot) {
        return spd * 10 + mot;
    }

    // 本回合伤害，outCrit 输出是否暴击
    static int computeDamage(uint8_t str, uint8_t siz, uint8_t end, uint8_t spi,
                             uint8_t spd, uint8_t mot, bool& outCrit) {
        float baseDmg = str * (1.0f + siz / 20.0f);
        float motMult = 0.5f + mot / 100.0f;
        // 暴击率由 SPI 与 SPD 共同决定
        int critRate = (int)spi * 3 + (int)spd * 1;
        if (critRate > 80) critRate = 80;
        outCrit = (random(100) < critRate);
        float critMult = outCrit ? 1.5f : 1.0f;
        int dmg = (int)(baseDmg * motMult * critMult) - (int)(end * 0.3f);
        if (dmg < 1) dmg = 1;
        return dmg;
    }

    // 每回合 MOT 自然衰减
    static int computeMotLoss(uint8_t spi) {
        int loss = 8 - spi / 2;
        if (loss < 1) loss = 1;
        return loss;
    }
};
