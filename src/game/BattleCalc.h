#pragma once
#include <cstdint>

// 对战公式计算
class BattleCalc {
public:
    // 总 HP
    static int computeHp(uint8_t siz, uint8_t end) {
        return 20 + siz * 3 + end * 2;
    }

    // 本回合伤害，outCrit 输出是否暴击
    static int computeDamage(uint8_t str, uint8_t siz, uint8_t end, uint8_t spi,
                             uint8_t mot, bool& outCrit) {
        float baseDmg = str * (1.0f + siz / 20.0f);
        float motMult = 0.5f + mot / 100.0f;
        outCrit = (random(100) < (int)spi * 5);
        float critMult = outCrit ? 1.8f : 1.0f;
        int dmg = (int)(baseDmg * motMult * critMult) - (int)(end * 0.5f);
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
