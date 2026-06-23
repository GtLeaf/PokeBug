# PokeBug 属性公式

本文记录当前属性成长与战斗公式，作为后续平衡性调参基线。

模拟脚本：`spareAsset/scripts/simulate_battle_balance.py`

战斗公式入口：`BattleCalc::BattleStats` 与 `BattleCalc::computeAttack(attacker, defender)`。

## 属性定位

- `SIZ`: 体型。主要提高 HP，少量提高伤害。
- `STR`: 力量。主要提高直接伤害，当前做了小幅削峰。
- `END`: 耐久。提高 HP，并提供稳定减伤。
- `SPD`: 速度。提高先手值，并影响闪避。
- `SPI`: 精神。提高暴击，降低被暴击，提高 MOT 稳定性，并少量提高闪避。
- `MOT`: 状态/斗志。影响先手和伤害，回合结束后按 SPI 衰减。

## 成长公式

食物基础向量在 `src/game/FoodType.h`:

```cpp
FoodStats { siz, str, end, spd, spi }
```

阶段吸收系数在 `Bug::feed()`:

```cpp
LARVA:    SIZ 1.0, STR 1.0, END 0.0, SPD 0.2, SPI 0.1
JUVENILE: SIZ 0.5, STR 0.5, END 0.5, SPD 1.0, SPI 0.5
ADULT:    SIZ 0.1, STR 0.1, END 0.1, SPD 0.1, SPI 0.1
```

单次食物成长:

```cpp
gain = foodStat * stageAbsorb * temperamentMult * envMult * geneGrowthMult * penalty
```

基因成长倍率:

```cpp
growthMult = 0.8 + dominantGene * 0.1
```

属性上限:

```cpp
cap = 6 + (dominantGene + recessiveGene) / 2
```

`10` 只是中位参考值，不是硬上限。玩家五维属性上限由对应基因控制，理论范围约 `6-21`。

SPI 额外上限:

```cpp
spiCap += spiCapBonusTenths / 10.0
```

## 腐木 END 成长

成虫只有真正处于腐木休息状态时，才应累计 END 成长。

当前目标公式:

```cpp
WOOD_REST_END_GAIN_MS = 4min
gain = 0.2 * endGrowthMult() * temperamentEndMult
```

气质倍率:

```cpp
RESILIENT: 1.10
BRUTE:     0.90
其他:      1.00
```

这让 END 成长受基因和气质影响，避免腐木把 END 无差别补满。

## 战斗公式

### HP

```cpp
hp = 20 + SIZ * 3 + END * 2
```

### 先手

```cpp
initiative = SPD * 6 + MOT * 0.5 + random(-8, 8)
```

轻微随机节奏避免 `SPD +1` 永久锁先手。

### 闪避

```cpp
if defenderSPD > attackerSPD:
    dodge += (defenderSPD - attackerSPD) * 3.0
dodge += defenderSPI / 8.0
dodge = clamp(dodge, 0, 22)
```

闪避成功时本次伤害为 `0`。

### 伤害

```cpp
strPower = 1.5 + STR * 0.80
sizeBonus = 1.0 + 0.45 * SIZ / (SIZ + 8.0)
motMult = 0.75 + MOT / 200.0
raw = strPower * sizeBonus * motMult
```

`1.5 + STR * 0.80` 保留低 STR 的基础攻击力，同时降低高 STR 的单点边际收益。SIZ 仍有伤害收益，但被压到温和曲线。

### 暴击

```cpp
critRate = 5 + attackerSPI * 2.2 + attackerSPD * 0.5 - defenderSPI * 1.5
critRate = clamp(critRate, 5, 55)
critMult = 1.35
```

SPI 同时提高暴击与抗暴。

### END 防御

```cpp
mitigation = 1.0 - min(0.14, defenderEND * 0.0045)
reduction = defenderEND * 0.18
damage = round(raw * critMult * mitigation - reduction)
damage = max(1, damage)
```

END 同时提供百分比减伤和固定减伤，但都有上限或低斜率，避免耐久流拖成过长战斗。

### MOT 衰减

```cpp
loss = round(9 - SPI / 3.0)
loss = clamp(loss, 2, 9)
```

SPI 高的甲虫更能维持状态，但不会 MOT 永动。

## 调参目标

随机基础属性 `6-16` 时，单属性 `+1` 的边际胜率目标大致为:

```text
STR: 最高，但不应碾压
SPD: 明显偏强
SIZ: 中等
END: 中等，略偏持久战
SPI: 中等偏低，但提供暴击、抗暴、MOT 稳定
```

当前 STR 参数为：

```cpp
strPower = 1.5 + STR * 0.80
```

如果后续模拟出现 `STR +1` 长期超过 `80%`，优先下调 `0.80`；如果低 STR 体验过弱，再小幅上调 `1.5`。
