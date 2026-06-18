# PokeBug 养成与战斗系统 v1.2

## 1. 设计原则

- **食物有固定属性向量**：不随阶段变化，每种食物的营养成分是固定的。
- **阶段差异通过吸收系数体现**：不同生命阶段对同一种食物的吸收能力不同，避免每个阶段维护一张食物表。
- **SPD 影响战斗频率**：速度不仅是先攻权，还通过累计机制带来额外攻击。
- **卵期行为决定气质**：参考宝可梦性格，卵期交互模式决定孵化后的属性倾向。
- **计算公式**：`实际属性增益 = 食物属性 × 阶段吸收系数 × 气质系数`（逐元素相乘）。

## 2. 生命周期阶段

| 阶段 | 时长 | 可喂食 | 可对站 | 核心属性窗口 |
|---|---|---|---|---|
| 卵 | 5 min | ❌ | ❌ | — |
| 幼虫 | 30 min | ✅ | ❌ | **SIZ / STR** |
| 蛹 | **30 min** | ❌ | ❌ | **SPI**（安静放置，每 10 min +1） |
| 青年期 | **60 min** | ✅ | **✅** | **SPD** |
| 成虫 | ∞ | ✅ | ✅ | **END**（微弱） |

> 游戏速度（`gameSpeed`）可在设置中调节（0.5x / 1x / 2x / 4x / 8x），无单独测试值。深睡 10 分钟唤醒会一次性推进 10 分钟虚拟时间。

## 3. 食物属性表

每种食物有固定属性向量，表示一次喂养的基础营养值。

| 食物 | 类型 | 等级 | SIZ | STR | END | SPD | SPI | 特殊效果 |
|---|---|---|---|---|---|---|---|---|
| **Drop** | 水滴 | 1 | 0.30 | 0.05 | 0.00 | 0.00 | 0.00 | 无风险，可连续喂 |
| **Cube** | 方块/坚果 | 1 | 0.05 | 0.30 | 0.00 | 0.00 | 0.00 | 力量型底子 |
| **Slice** | 切片/香蕉 | 2 | 0.15 | 0.15 | 0.00 | 0.15 | 0.00 | 均衡偏速度 |
| **Citrus** | 柑橘 | 2 | 0.10 | 0.08 | 0.00 | 0.05 | 0.15 | 幼虫期累计 3 次 → 蛹期 SPI 上限 +0.3 |
| **Jelly** | 果冻/蜂蜜 | 3 | 0.20 | 0.25 | 0.00 | 0.20 | 0.00 | 连续喂间隔需 **> 5 min**，否则效果 ×0.5 |
| **Berry** | 浆果 | 3 | 0.15 | 0.15 | 0.15 | 0.10 | 0.10 | **唯一加 END** 的食物 |

## 4. 阶段吸收系数

将食物属性乘以对应系数，得到该阶段实际吸收值。

| 阶段 | SIZ | STR | END | SPD | SPI | 说明 |
|---|---|---|---|---|---|---|
| **卵** | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 不可喂食 |
| **幼虫** | 1.0 | 1.0 | 0.0 | 0.2 | 0.1 | SIZ/STR 完全吸收；SPD 启蒙（微弱）；END/SPI 不吸收 |
| **蛹** | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 不可喂食，安静放置 SPI 成长 |
| **青年期** | 0.5 | 0.5 | 0.5 | 1.0 | 0.5 | SPD 完全吸收；其他属性中等吸收 |
| **成虫** | 0.1 | 0.1 | 0.1 | 0.1 | 0.1 | 属性成长极微，以 MOT 恢复为主 |

### 计算示例

**幼虫期喂 1 次 Drop**：
- SIZ：0.30 × 1.0 = **0.30**
- STR：0.05 × 1.0 = **0.05**
- SPD：0.00 × 0.2 = 0.00
- SPI：0.00 × 0.1 = 0.00

**青年期喂 1 次 Jelly**（间隔合规）：
- SIZ：0.20 × 0.5 = **0.10**
- STR：0.25 × 0.5 = **0.125**
- SPD：0.20 × 1.0 = **0.20**
- SPI：0.00 × 0.5 = 0.00

**成虫期喂 1 次 Berry**（属性增益）：
- END：0.15 × 0.1 = **0.015**
- SPD：0.10 × 0.1 = **0.01**
- （其余属性同理，极微）

## 5. 成虫期食物效果（MOT 恢复为主）

成虫期属性吸收系数仅 0.1，食物主要作用是恢复 MOT。

| 食物 | MOT 恢复 | 额外效果 |
|---|---|---|
| Drop | +10 | 无 |
| Cube | +8 | 无 |
| Slice | +20 | 甲虫**优先走向**（快碳吸引力） |
| Citrus | +15 | 吃下后 **2 分钟内** 对战暴击率 +3% |
| Jelly | +30 | 吃下后 **5 分钟内** MOT 自然衰减 -20% |
| Berry | +25 | 无 |

## 6. 特殊机制

### 6.1 间隔惩罚（Jelly）

Jelly 在 **幼虫期** 和 **青年期** 连续喂养时，若两次间隔 **< 5 分钟**，第二次及以后效果 ×0.5。视觉提示：食物盘上的 Jelly 闪烁变暗。

### 6.2 青年期对战

- 青年期甲虫**可正常进入对战**（配对、同步、战斗流程与成虫相同）。
- 无额外限制或削弱：青年期 SPD 已成型，但 SIZ/STR 可能未完全展开，HP 和伤害天然低于成虫，这是自然差异而非人为限制。
- 对战胜利奖励（SPI +0.5，树汁 +2）与成虫相同。

### 6.3 蛹期安静成长

蛹期不可喂食，但安静放置每 10 分钟 SPI +1（上限由基因 MNT 决定）。以下幼虫期食物影响蛹期效率：

| 幼虫期食物 | 蛹期影响 |
|---|---|
| Citrus（累计 3 次） | SPI 上限 **额外 +0.3**（唯一突破基因天花板的途径） |
| Berry（任意次数） | 安静效率 **+10%**（每 9 分钟 +1 SPI，而非 10 分钟） |

## 7. 玩家决策流

### 流派 A：大块头坦克（Drop + Berry）

- 幼虫期：6 次 Drop → SIZ 拉满 → HP 厚
- 幼虫期：穿插 3 次 Berry → END +0.45
- 青年期：3 次 Berry → SPD +0.30，END +0.225
- 结果：SIZ≈9, END≈6, SPD≈3 → **HP 53，先攻权低，但两刀砍不死**

### 流派 B：速攻暴击（Slice + Citrus + Jelly）

- 幼虫期：3 Slice + 3 Citrus → SIZ 适中，SPI 底子好，蛹期上限 +0.3
- 蛹期：安静放置 → SPI 高
- 青年期：3 Jelly（间隔合规）→ SPD +0.60，STR +0.375
- 结果：STR≈5, SPI≈5, SPD≈9 → **先攻 + 25% 暴击率，一刀流**

### 流派 C：均衡战士（Cube + Drop + Berry）

- 幼虫期：3 Cube + 3 Drop → STR + SIZ 均衡
- 青年期：3 Berry → SPD +0.30, END +0.225
- 结果：无明显短板，无先攻权但稳定

## 8. 存档与代码对接

| 文件 | 改动 |
|---|---|
| `src/game/FoodType.h`（新建） | `enum class FoodType : uint8_t { DROP, CUBE, SLICE, CITRUS, JELLY, BERRY }` |
| `src/game/Bug.h` | 新增 `spd` 属性；`feed(FoodType)` 替代 `feed()`；`Stage` 增加 `JUVENILE`；`foodCounts[6]` 替代 `sapCount` |
| `src/game/Bug.cpp` | 阶段推进逻辑（含青年期）；`feed()` 查食物属性表 + 阶段系数 |
| `src/scenes/TerrariumScene` | 食物盘按 `FoodType` 绘制不同颜色/形状；青年期绘制（缩放 0.7x） |
| `src/scenes/MenuScene` | 食物子菜单展示 6 种 + 数量 |
| `src/game/BattleCalc.h` | 新增 `firstStrike()` 先攻判定 |
| `src/scenes/BattleScene` | 先攻动画、伤害数字飘出 |
| `src/core/SaveManager` | 存档升级到 **v5**（加 `spd` + `JUVENILE` 阶段 + `foodCounts[6]`） |

## 9. 版本历史

| 版本 | 日期 | 变更 |
|---|---|---|
| v1.0 | 2026-06-17 | 初版：6 种食物 + 5 阶段 + 属性向量 + 阶段系数模型 |

## 10. SPD 多段攻击机制（Speed Combo Gauge）

SPD 不仅是先攻权，还通过**行动值累计**带来额外攻击机会。

### 10.1 机制

- 每回合结束时，SPD 高的一方将 SPD 差值累加到 `spdGauge`：
  - `spdGauge += max(0, mySPD - enemySPD)`
- 当 `spdGauge >= SPD_GAUGE_THRESHOLD`（默认 **10**）：
  - 下回合获得一次**额外攻击**（Extra Attack）
  - `spdGauge = 0`（清0）
  - 额外攻击伤害 = 正常伤害 × **0.5**（防止高速碾压）
  - 额外攻击不会再次触发 gauge（一回合最多 1 次额外攻击）
- 若双方 SPD 相同，gauge 不增长

### 10.2 示例

| 我方 SPD | 敌方 SPD | 每回合 gauge | 触发周期 | 效果 |
|---|---|---|---|---|
| 8 | 5 | +3 | 第4回合 | 每 4 回合额外攻击 1 次（50% 伤害） |
| 10 | 5 | +5 | 第2回合 | 每 2 回合额外攻击 1 次（50% 伤害） |
| 10 | 1 | +9 | 第2回合 | 每 2 回合额外攻击 1 次（50% 伤害） |
| 5 | 5 | 0 | — | 无额外攻击 |

### 10.3 回合流程（含额外攻击）

```
1. 先攻判定（SPD 高者先攻）
2. 先攻方基础攻击
3. 若先攻方有 extraAttack 标记 → 追加攻击（50% 伤害）
4. 若被击方 HP > 0 → 被击方反击（基础攻击）
5. 若被击方有 extraAttack 标记且先攻方 HP > 0 → 追加攻击（50% 伤害）
6. 回合结束，更新 gauge
```

> 先攻方若在基础攻击中 KO 敌方，敌方无反击；若先攻方有 extraAttack 但敌方已 KO，额外攻击不触发。

### 10.4 代码对接

| 文件 | 改动 |
|---|---|
| `src/game/BattleCalc.h` | 新增 `updateSpdGauge()`、`hasExtraAttack()`；调整 `computeDamage()` 支持额外攻击倍率 |
| `src/scenes/BattleScene.h` | 新增 `int mySpdGauge`、`int enemySpdGauge`；`bool myExtraAttack`、`bool enemyExtraAttack` |
| `src/scenes/BattleScene.cpp` | 回合开始时检查 gauge 并标记 extraAttack；CLASH 阶段执行额外攻击并显示 "COMBO!" |

---

## 11. 卵期气质系统（Temperament）

参考宝可梦性格，卵期交互行为决定孵化后的**气质**，气质终身影响属性成长倍率。

### 11.1 6 种气质

| 气质 | 提升属性 | 降低属性 | 卵期行为条件 | 孵化表现 |
|---|---|---|---|---|
| **迅捷** | SPD × 1.10 | SIZ × 0.90 | 倾斜累计 ≥ **30%** 卵期总时长（虚拟时间） | 卵重心偏移，倾向一侧 |
| **韧甲** | END × 1.10 | SPI × 0.90 | 短按 B 戳 ≥ **4 次** | 卵壳有微压痕但异常坚固 |
| **巨体** | SIZ × 1.10 | SPD × 0.90 | 短按 A 喷水 ≥ **4 次** | 卵偏椭圆，水分充足 |
| **蛮力** | STR × 1.10 | END × 0.90 | 摇晃 ≥ **4 次** + 至少 **1 次剧烈摇晃**（加速度 > 2.5g） | 卵有大力撞击痕迹，活力外露 |
| **均衡** | 无 | 无 | 戳、喷水、摇晃都 **≥ 1 且 ≤ 3** | 卵正常大小，无特殊表现 |
| **灵心** | SPI × 1.10 | STR × 0.90 | **兜底** | 卵发光柔和，安静内敛 |

> **气质在幼虫孵化瞬间确定，终身不可变。**

### 11.2 判定优先级

三层判定，**从高层到低层依次检查**，一旦满足立即返回：

| 层级 | 判定类型 | 规则 | 说明 |
|---|---|---|---|
| **L1** | 环境类：迅捷 | 倾斜累计 ≥ 30% 卵期总时长（虚拟时间） | 倾斜最"难达成"，需要持续保持姿势，优先级最高 |
| **L2** | 操作类：韧甲/巨体/蛮力 | 戳、喷水、摇晃三者中取**次数最高者**。若 ≥ 4 次 → 对应气质；若相同则**最近触发者**胜出 | 三者同级，次数说话 |
| **L3** | 均衡 | 戳、喷水、摇晃都 ≥ 1 且 ≤ 3 | "什么都试了一下，但都不深入" |
| **L4** | 灵心 | **兜底** | 什么都没做（操作类 = 0）或只满足 L1-L3 的边界 |

> **倾斜按虚拟时间累计**：受 `gameSpeed` 影响，1x 速度下卵期 5 min 虚拟时间，需要倾斜 ≥ 1.5 min；2x 速度下实际 2.5 min 内倾斜 ≥ 1.5 min 虚拟时间。

### 11.3 典型场景判定

| 场景 | 卵期行为 | 判定结果 | 原因 |
|---|---|---|---|
| 倾斜 2 分钟（1x 速度）+ 戳 5 次 | 倾斜 ≥ 30% | **迅捷** | L1 优先级最高，覆盖 L2 |
| 戳 5 次 + 喷水 2 次 + 摇晃 0 | 戳最高（5次）≥ 4 | **韧甲** | L2 操作类，次数最高 |
| 戳 3 次 + 喷水 3 次 + 摇晃 3 次 | 都 ≥ 1 且 ≤ 3 | **均衡** | L3 满足 |
| 戳 2 次 + 喷水 0 次 + 摇晃 0 | 操作类不全 ≥ 1 | **灵心** | 未满足均衡，L4 兜底 |
| 喷水 4 次 + 戳 4 次，最后戳了一次 | 戳和喷水同次数（4次），最近戳 | **韧甲** | L2 平局：最近触发胜出 |
| 倾斜 1 分钟 + 戳 5 次（1x 速度） | 倾斜 < 30%（1min < 1.5min），不满足 L1 | **韧甲** | L1 不满足，走 L2 |
| 什么都不做 | 操作类 = 0，倾斜 = 0 | **灵心** | L4 兜底 |

### 11.4 卵期记录字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `eggStartTime` | `uint64_t` | 卵期开始时间（`gameNow`） |
| `eggShakeCount` | `uint8_t` | 有效摇晃次数（> 2.0g，500ms 冷却） |
| `eggViolentShakeCount` | `uint8_t` | 剧烈摇晃次数（> 2.5g，500ms 冷却） |
| `eggPokeCount` | `uint8_t` | 戳次数（短按 B，30s 冷却） |
| `eggWaterCount` | `uint8_t` | 喷水次数（短按 A，30s 冷却） |
| `eggLeftTiltMs` | `uint32_t` | 左倾累计时长（ax > +0.5g） |
| `eggRightTiltMs` | `uint32_t` | 右倾累计时长（ax < -0.5g） |
| `eggLastAction` | `uint8_t` | 最近触发的操作类：0=无, 1=戳, 2=喷水, 3=摇晃 |
| `eggTiltMs` | `uint32_t` | 倾斜总累计（`eggLeftTiltMs + eggRightTiltMs`） |

> 倾斜按 `TerrariumScene::updateTilt()` 相同的防抖机制（200ms 稳定）累计，只记录实际处于左/右倾状态的虚拟时间。

### 11.5 判定伪代码

```cpp
enum class Temperament : uint8_t {
    RESILIENT,  // 韧甲
    GIANT,      // 巨体
    BRUTE,      // 蛮力
    SWIFT,      // 迅捷
    BALANCED,   // 均衡
    SPIRIT,     // 灵心
};

enum class EggAction : uint8_t { NONE, POKE, WATER, SHAKE };

Temperament determineTemperament() {
    uint64_t totalDuration = gameNow - eggStartTime;
    uint32_t tiltPct = (eggTiltMs * 100) / totalDuration;

    // L1: 迅捷（倾斜占比最高优先级）
    if (tiltPct >= 30) return Temperament::SWIFT;

    // L2: 操作类，收集满足各自条件的候选
    struct Candidate { Temperament t; int count; };
    Candidate candidates[3];
    int n = 0;
    if (eggPokeCount >= 4)    candidates[n++] = {Temperament::RESILIENT, eggPokeCount};
    if (eggWaterCount >= 4)   candidates[n++] = {Temperament::GIANT, eggWaterCount};
    if (eggShakeCount >= 4 && eggViolentShakeCount >= 1)
        candidates[n++] = {Temperament::BRUTE, eggShakeCount};

    if (n > 0) {
        // 找次数最高者
        int maxCount = candidates[0].count;
        for (int i = 1; i < n; i++) if (candidates[i].count > maxCount) maxCount = candidates[i].count;
        // 平局：最近触发者胜出
        if (eggLastAction == (uint8_t)EggAction::POKE && eggPokeCount == maxCount)
            return Temperament::RESILIENT;
        if (eggLastAction == (uint8_t)EggAction::WATER && eggWaterCount == maxCount)
            return Temperament::GIANT;
        if (eggLastAction == (uint8_t)EggAction::SHAKE && eggShakeCount == maxCount && eggViolentShakeCount >= 1)
            return Temperament::BRUTE;
        // 回退固定优先级：戳 > 喷水 > 摇晃
        for (int i = 0; i < n; i++) if (candidates[i].count == maxCount) return candidates[i].t;
    }

    // L3: 均衡
    bool allInRange = (eggPokeCount >= 1 && eggPokeCount <= 3) &&
                      (eggWaterCount >= 1 && eggWaterCount <= 3) &&
                      (eggShakeCount >= 1 && eggShakeCount <= 3);
    if (allInRange) return Temperament::BALANCED;

    // L4: 灵心兜底
    return Temperament::SPIRIT;
}
```

### 11.6 代码对接

| 文件 | 改动 |
|---|---|
| `src/game/Bug.h` | 新增 `Temperament` 枚举；`temperament` 字段；卵期记录字段（`eggPokeCount`、`eggWaterCount`、`eggViolentShakeCount`、`eggLastAction`、`eggTiltMs`）；`determineTemperament()` |
| `src/game/Bug.cpp` | 孵化时调用 `determineTemperament()`；`feed()` 中应用气质系数；`processIMU()` 卵期记录摇晃/倾斜/暴力摇晃；`onButton()` 卵期记录戳/喷水 |
| `src/scenes/TerrariumScene.cpp` | 卵期按 A 触发喷水（`eggWaterCount++`）而非 `placeSapInTray()`；按 B 触发戳（`eggPokeCount++`）；`updateTilt()` 累计倾斜时间 |
| `src/scenes/InfoScene` | 显示气质名称（如"气质：均衡"） |
| `src/core/SaveManager` | 存档 v5 增加 `temperament` + 全部卵期记录字段 |

---

## 13. 环境物品加成系统

培养缸中的物品不仅是装饰，它们会微妙地影响属性成长。

### 13.1 食物盘环境加成

| 食物盘等级 | 基础加成 | 解锁条件 | 说明 |
|---|---|---|---|
| **Lv.1 基础盘** | 无 | 初始默认 | 纯容器，无属性加成 |
| **Lv.2 培育盘** | 有食物时所有属性 × 1.03 | 对战胜利 2 次解锁 | 培育环境优化 |
| **Lv.3 精育盘** | 有食物时所有属性 × 1.06 | 对战胜利 5 次解锁 | 高级培育环境 |

> **空盘惩罚**：无论等级，空盘时 MOT 自然衰减 +20%（甲虫焦虑）。

| 盘中食物 | 额外加成 | 说明 |
|---|---|---|
| **Cube** | STR 额外 × 1.03 | 力量环境 |
| **Slice** | SPD 额外 × 1.03 | 速度环境 |
| **Drop** | SIZ 额外 × 1.03 | 体型环境 |
| **Citrus** | SPI 额外 × 1.03 | 精神环境 |
| **Jelly** | STR 额外 × 1.03 | 爆发环境 |
| **Berry** | END 额外 × 1.03 | 耐力环境 |

> 食物盘加成仅在该阶段甲虫实际进食时生效（幼虫/青年期/成虫）。卵期和蛹期不生效。盘中食物是“储备”，甲虫在饥饿时会自行靠近食物盘进食。

### 13.2 腐木环境加成

| 风格 | 属性倾向 | 加成 | 说明 |
|---|---|---|---|
| **Twig**（原木） | STR | 休息/idle 时 STR 恢复 +0.02/min | 硬质环境，力量沉淀 |
| **Stack**（堆叠） | SIZ | 休息/idle 时 SIZ 恢复 +0.02/min | 堆积感，体型舒展 |
| **Mossy**（苔藓） | SPI | 休息/idle 时 SPI 恢复 +0.02/min | 湿润安静，精神沉淀 |
| **Pale**（苍白） | SPD | 休息/idle 时 SPD 恢复 +0.02/min | 光滑表面，敏捷训练 |
| **Hollow**（空心） | END | 休息/idle 时 END 恢复 +0.02/min | 庇护所，耐力恢复 |

> 腐木加成仅对成虫有效。幼虫/蛹期无腐木概念。休息时每分钟属性微弱恢复，体现“有腐木 = 有安全感”。

> 实际属性增益 = 食物属性 × 阶段吸收系数 × 气质系数 × 环境系数

```
实际属性增益 = 食物属性 × 阶段吸收系数 × 气质系数 × 环境系数
```

| 环境系数 | 计算 |
|---|---|
| 食物盘系数 | Lv.1 = 1.0（无加成）；Lv.2 = 1.03（所有属性）；Lv.3 = 1.06（所有属性）；空盘 = 1.0（仅 MOT 衰减加速）；特定食物 = 再 × 1.03（对应属性） |
| 腐木系数 | 有腐木 = 1.03（对应属性，仅成虫） |

**示例：蛮力气质幼虫，Lv.3 食物盘中有 Cube，无腐木**
- 实际 STR = 0.30 × 1.0（幼虫系数） × 1.10（蛮力） × 1.06（Lv.3 盘） × 1.03（Cube 倾向） = **0.36**
- 实际 SIZ = 0.30 × 1.0 × 1.10 × 1.06 = 0.35（无 Cube 额外加成）

**示例：均衡气质成虫，Lv.2 食物盘中有 Berry，腐木为 Hollow**
- 实际 END = 0.15 × 0.1（成虫系数） × 1.0（均衡） × 1.03（Lv.2 盘） × 1.03（Berry 倾向） × 1.03（Hollow 倾向） = **0.017**
- 同时休息时每 1 分钟 END +0.02

### 13.4 环境加成与玩家决策

| 环境配置 | 效果 | 适用流派 |
|---|---|---|
| Cube + Twig (Lv.3) | STR × 1.06 × 1.03 = 1.09，休息 STR +0.02/min | 力量流 |
| Cube + Twig (Lv.2) | STR × 1.03 × 1.03 = 1.06，休息 STR +0.02/min | 力量流 |
| Berry + Hollow (Lv.3) | END × 1.06 × 1.03 × 1.03 = 1.12，休息 END +0.02/min | 坦克流 |
| Berry + Hollow (Lv.2) | END × 1.03 × 1.03 × 1.03 = 1.09，休息 END +0.02/min | 坦克流 |
| Slice + Pale (Lv.3) | SPD × 1.06 × 1.03 = 1.09，休息 SPD +0.02/min | 速攻流 |
| Citrus + Mossy (Lv.3) | SPI × 1.06 × 1.03 = 1.09，休息 SPI +0.02/min | 暴击流 |
| 空盘 + 无腐木 | MOT 衰减 +20%，无属性加成 | 惩罚状态 |

> 食物盘和腐木是“长期投资”——放一次影响整个阶段。玩家在幼虫期选择 Lv.3 Cube 盘 + Twig 腐木，就是在为“力量流”奠基。

### 13.5 代码对接

| 文件 | 改动 |
|---|---|
| `src/game/Bug.h` | 新增 `foodTrayLevel`（食物盘等级 1-3）、`foodTrayEnvBoost`（食物盘环境系数）、`woodEnvBoost`（腐木环境系数）、`getEnvMultiplier(attr)` |
| `src/game/Bug.cpp` | `feed()` 中乘环境系数（根据 `foodTrayLevel` 计算）；`update()` 中食物盘为空时 MOT 衰减加速；`setFoodTray(level, foodType)`/`setWood(woodStyle)` 更新环境系数 |
| `src/scenes/TerrariumScene` | 食物盘放置/清空时调用 `bug.setFoodTray(level, foodType)`；腐木切换时调用 `bug.setWood(woodStyle)` |
| `src/scenes/MenuScene` | 物品子菜单显示食物盘等级与环境加成提示（如“Lv.3 Cube 盘：STR +9%”） |

---

## 14. 版本历史

| 版本 | 日期 | 变更 |
|---|---|---|
| v1.0 | 2026-06-17 | 初版：6 种食物 + 5 阶段 + 属性向量 + 阶段系数模型 |
| v1.1 | 2026-06-17 | 新增：SPD 多段攻击机制 + 卵期气质系统（6 种气质） |
| v1.2 | 2026-06-17 | 气质系统最终版：倾斜 30% 优先级最高；操作类（戳/喷水/摇晃）次数最高者胜出，平局最近触发；均衡=操作类 1~3；灵心兜底；倾斜按虚拟时间累计 |
| v1.3 | 2026-06-18 | 新增：环境物品加成系统（食物盘 + 腐木） |