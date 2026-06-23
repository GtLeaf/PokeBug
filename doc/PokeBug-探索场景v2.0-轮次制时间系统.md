# 🗺️ PokeBug 探索场景 v2.0 — 轮次制 + 时间系统 + 地点差异化

> 基于用户 7 条新设计规则，对原 `ExploreScene` 进行全面重构设计。  
> 核心改动：45秒时间制 → **3轮事件制**，引入 **Morning/Afternoon/Evening** 三时段，**每日2次限制**。

---

## 一、设计规则总览（7 条）

| 编号 | 规则 | 设计影响 |
|---|---|---|
| 1 | 每次探险持续 **3 轮** | 取消 45s 计时，改为事件轮次制 |
| 2 | 遇对战可接受/离开，**离开后不可继续探索** | NPC 战斗的 "离开" = 探索强制结束 |
| 3 | 战斗失败后 **MOT 降为 0**，MOT < 50 不可再次探索 | 失败惩罚加重，MOT 成为探索准入门槛 |
| 4 | 时间分为 **Morning / Afternoon / Evening**，概率不同 | 每个地点 × 3 时段 = 12 张概率表 |
| 5 | 非成虫（卵/幼虫/蛹）遇战斗给提示后结束 | 探索状态保险机制 |
| 6 | 探索结束后虚拟时间自动推进 **3 小时** | 时间资源化，可能跨入下一时段或深夜 |
| 7 | **每天只能探索 2 次** | 探索次数成为每日稀缺资源 |

---

## 二、时间系统（Day Cycle）

### 2.1 三时段定义

| 时段 | 英文 | 氛围 | 默认背景色调 |
|---|---|---|---|
| **上午** | Morning | 清新、晨雾、安全 | 明亮偏蓝绿 |
| **下午** | Afternoon | 温暖、活跃、适中 | 明亮偏黄绿 |
| **晚上** | Evening | 昏暗、危险、神秘 | 暗蓝紫，月光/萤火虫 |

### 2.2 时间推进规则

```
探索结束 → 虚拟时钟 +3 小时
05:00 → 08:00
10:30 → 13:30
18:30 → 21:30
21:30 → 00:30
```

- 每次探索固定推进 **3 小时**，无论探索是否完整走完 3 轮。
- 探索过程本身暂停普通养成时间计算；结算时只推进世界时钟，不额外补算这 3 小时的饥饿/成长。
- 如果 +3 小时后进入新的探索日，才会重置每日探索次数。
- 探索是推进时间的主要手段之一（另一个是自然待机流逝）。

### 2.3 每日探索限制

| 项目 | 规则 |
|---|---|
| **每日次数上限** | 2 次 |
| **计数器重置** | 从 Evening 进入 Next Day Morning 时清零 |
| **UI 提示** | 当次数用尽时，Menu 中 Explore 项显示为灰色禁用态，长按显示 `"Daily limit reached."` |
| **保存** | `exploreCountToday` 和 `currentDay` 写入存档 |

---

## 三、探索准入与退出规则

### 3.1 准入条件（必须同时满足）

| 条件 | 规则 | 不满足时的提示 |
|---|---|---|
| 阶段 | **成虫期**（Stage::ADULT） | `"Only adult beetles can explore."` |
| 存活 | 未死亡 | `"Your beetle has passed away."` |
| 饥饿 | Hunger ≥ 30 | `"Hunger too low. Feed first."` |
| 斗志 | **MOT ≥ 50** | `"MOT too low. Cheer up first."` |
| 次数 | 当日探索次数 < 2 | `"Daily limit reached."` |

### 3.2 探索状态机（3 轮制）

```
onEnter
  ├── 扣除 Hunger -20
  ├── 初始化轮次 round = 1
  └── 状态 → EXPLORING

EXPLORING (每轮触发一个事件)
  ├── triggerEvent() → 弹出事件结果
  ├── 显示 "Round X/3" + 结果 + "A:Next  B:Return"
  ├── 若 A 按下：
  │     ├── round++
  │     ├── 若 round ≤ 3：回到 EXPLORING
  │     └── 若 round > 3：状态 → RESULT，显示 "Explore Complete"
  └── 若 B 按下：
        └── 状态 → RETURNING，提前结束

NPC_PROMPT（战斗遭遇）
  ├── 显示对手信息 + "A:Accept  B:Leave"
  ├── A 接受 → 进入 BattleScene
  │     └── 战斗结束后：
  │         ├── 若胜利：继续 EXPLORING（保留当前 round）
  │         └── 若失败：MOT = 0，状态 → RESULT，显示 "Defeated... MOT=0"
  └── B 离开 → 状态 → RESULT，显示 "Left..."，**不可继续探索**

RESULT（3 轮完成或提前结束）
  ├── 显示本次探索总收益汇总
  ├── 虚拟时间推进 3 小时
  ├── 每日探索次数 +1
  └── A 确认 → 返回培养缸
```

### 3.3 非成虫状态保险（规则 5）

虽然准入时限制成虫期，但如果在探索中发生异常退化（如基因突变导致回退），或未来开放子成虫等中间状态：

- 当 `triggerEvent()` 返回 `NPC` 类型时，先检查 `bug.getStage()`
- 若 **非成虫**（卵/幼虫/蛹）：
  - 不进入 `NPC_PROMPT`
  - 显示 `"Too young to fight. Explore ends."`
  - 状态 → `RESULT`，**探索强制结束**
  - 已获得的轮次收益保留，但剩余轮次作废

---

## 四、四地点 × 三时段 概率总表

### 4.1 Park 🌱 城市公园

| 时段 | SAP | FOOD | WOOD | NPC | RARE | NOTHING | 特点 |
|---|---|---|---|---|---|---|---|
| **Morning** | **40%** | 15% | 10% | 10% | 5% | 20% | 晨雾弥漫，树汁最丰 |
| **Afternoon** | 30% | 20% | 10% | 15% | 8% | 17% | 野餐高峰，食物源增加 |
| **Evening** | 25% | 15% | 10% | **20%** | 10% | 20% | 傍晚散步者，NPC 增多 |

**NPC 等级**：ROOKIE 70% / NORMAL 25% / VETERAN 5% / LEGEND 0%（全时段）

### 4.2 Back Hill 🪵 后山

| 时段 | SAP | FOOD | WOOD | NPC | RARE | NOTHING | 特点 |
|---|---|---|---|---|---|---|---|
| **Morning** | 20% | 10% | **30%** | 10% | 10% | 20% | 晨露晒木，腐木最丰 |
| **Afternoon** | 30% | 15% | 20% | 10% | 10% | 15% | 炎热干燥，树汁蒸发 |
| **Evening** | 15% | 10% | 20% | **25%** | 10% | 20% | 夜行动物出没，危险上升 |

**NPC 等级**：ROOKIE 40% / NORMAL 45% / VETERAN 14% / LEGEND 1%（Morning 0%，Evening 3%）

### 4.3 Riverside 💧 河岸

| 时段 | SAP | FOOD | WOOD | NPC | RARE | NOTHING | 特点 |
|---|---|---|---|---|---|---|---|
| **Morning** | 15% | 15% | 10% | 15% | **30%** | 15% | 晨雾菌类，稀有事件天堂 |
| **Afternoon** | 25% | 20% | 10% | 15% | 15% | 15% | 蒸发旺盛，食物源高 |
| **Evening** | 10% | 10% | 10% | 20% | **25%** | 25% | 萤火虫与幻影，但安静也多 |

**NPC 等级**：ROOKIE 20% / NORMAL 40% / VETERAN 30% / LEGEND 10%（Evening 15%）

### 4.4 Old Woods 🌲 老林

| 时段 | SAP | FOOD | WOOD | NPC | RARE | NOTHING | 特点 |
|---|---|---|---|---|---|---|---|
| **Morning** | 10% | 10% | 15% | 20% | **25%** | 20% | 鸟鸣唤醒，古老秘密显露 |
| **Afternoon** | 10% | 10% | 15% | **25%** | 20% | 20% | 午后安静，强者巡逻 |
| **Evening** | 5% | 5% | 10% | **35%** | 20% | **25%** | 深夜地狱，高风险高回报 |

**NPC 等级**：ROOKIE 10% / NORMAL 30% / VETERAN 40% / LEGEND 20%（Evening 30%）

---

## 五、地点专属稀有事件（RARE 子类型）

### 5.1 Park 🌱（6 种）

| ID | 名称 | 奖励 | 时段加成 | 文案 |
|---|---|---|---|---|
| 0 | 野餐掉落 | +2-4 树汁 | Afternoon +1 | `"Picnic crumbs! Sap +%d"` |
| 1 | 洒水器 | +1 树汁 + 湿度 | Morning | `"Sprinkler shower! Refreshing"` |
| 2 | 晨露叶片 | +2 树汁 | Morning | `"Morning dew on leaf! +2"` |
| 3 | 蚯蚓翻土 | +1 树汁 | Morning | `"Earthworm digs up sap"` |
| 4 | 彩虹 | +3 树汁 + SPI +0.1 | Afternoon 雨后 | `"Rainbow! Lucky find +3"` |
| 5 | 蒲公英绒 | +1 树汁 | Morning | `"Dandelion fluff... +1"` |

### 5.2 Back Hill 🪵（6 种）

| ID | 名称 | 奖励 | 时段加成 | 文案 |
|---|---|---|---|---|
| 0 | 晒干的树皮 | +1 腐木 + 1 树汁 | Morning | `"Sun-baked bark! Wood+1"` |
| 1 | 蚂蚁搬家 | +2 树汁 | Afternoon | `"Ant trail leads to sap! +2"` |
| 2 | 松果滚落 | +1 树汁 | Evening | `"Pinecone rolls by... +1"` |
| 3 | 旧木桩裂缝 | +1 腐木 + 2 树汁 | Morning | `"Cracked stump! Wood+1"` |
| 4 | 蜥蜴晒背 | +2 树汁 + END +0.1 | Morning | `"Lizard sunbathing... +2"` |
| 5 | 山猫踪迹 | 无奖励，下次 NPC +10% | Evening | `"Mountain cat tracks..."` |

### 5.3 Riverside 💧（6 种）

| ID | 名称 | 奖励 | 时段加成 | 文案 |
|---|---|---|---|---|
| 0 | 苔藓地毯 | +2 树汁 + 1 腐木 | Morning | `"Moss carpet! +2 +wood"` |
| 1 | 萤火虫 | +3 树汁 + SPI +0.2 | **Evening** | `"Fireflies at dusk! +3"` |
| 2 | 牛蛙鸣叫 | +1 树汁 | Evening | `"Bullfrog croaks... +1"` |
| 3 | 雨后菌圈 | +3 树汁 + 1 腐木 + END +0.2 | **Morning** | `"Fairy ring! +3 +wood"` |
| 4 | 漂流木 | +1 腐木 + 2 树汁 | Morning | `"Driftwood ashore! +1"` |
| 5 | 水黾点水 | +2 树汁 | Afternoon | `"Water strider skims... +2"` |

### 5.4 Old Woods 🌲（6 种）

| ID | 名称 | 奖励 | 时段加成 | 文案 |
|---|---|---|---|---|
| 0 | 古树脂滴 | +4 树汁 + END +0.3 | Afternoon | `"Ancient resin drip! +4"` |
| 1 | 鹿角菇 | +2 树汁 + 1 腐木 | Morning | `"Deer antler mushroom! +2"` |
| 2 | 枯木甲虫 | +1 腐木 + 3 树汁 | Morning | `"Deadwood beetle troop! +3"` |
| 3 | 夜光苔藓 | +5 树汁 | **Evening** | `"Glow moss! +5"` |
| 4 | 朽木之心 | +1 腐木 + 2 树汁 + STR +0.2 | Morning | `"Heart of rot! +wood +2"` |
| 5 | **古树幻影** | +6 树汁 + 1 腐木 + SPI +0.3 | **Evening** | `"Phantom of the old tree! +6"` |

---

## 六、NPC 战斗规则（重构）

### 6.1 遭遇流程

```
探索中触发 NPC 事件
  ├── 检查甲虫状态（非成虫 → 强制结束）
  ├── 生成对手（按地点 × 时段等级分布）
  └── 弹出 NPC_PROMPT 界面
      ├── 显示："[NORMAL]训练师小明"
      │           "Beetle:赫拉克勒斯"
      │           "让我看看你的实力！"
      ├── A: Accept → 进入 BattleScene
      └── B: Leave → 探索结束
```

### 6.2 战斗结果与探索连续性

| 结果 | 影响 | 探索继续？ |
|---|---|---|
| **胜利** | 按等级获得树汁 + 可能腐木 | ✅ 继续，当前轮次正常推进 |
| **失败** | **MOT 强制降为 0**，可能失去树汁 | ❌ 探索结束，返回培养缸 |
| **逃跑（NPC_PROMPT 按 B）** | 无收益无惩罚 | ❌ 探索结束 |

### 6.3 失败后的恢复

- MOT 降为 0 后，**不可再次探索**（准入条件 MOT ≥ 50）
- 恢复方式：
  - 戳甲虫（+5 MOT，30s CD）
  - 摇晃（+5 MOT，60s CD）
  - 自然时间恢复（缓慢）
  - 或等待探索日切换（时钟进入次日 05:00 后）重置

---

## 七、3 轮探索完整流程示例

### 示例 A：公园上午（新手友好）

```
【准入】Hunger 60, MOT 80, 成虫, 当日第1次 ✓

Round 1/3: 触发 SAP → "Sprinkler shower! Refreshing" → +1 树汁
            [A:Next  B:Return]
            玩家按 A

Round 2/3: 触发 SAP → "Morning dew on leaf! +2" → +2 树汁
            [A:Next  B:Return]
            玩家按 A

Round 3/3: 触发 FOOD_SOURCE → "Food source +1"
            探索结束

【结果】+3 树汁, 1 湿度
【时间推进】+3 小时，例如 07:00 → 10:00
【当日剩余】1 次探索
```

### 示例 B：老林晚上（高风险）

```
【准入】Hunger 70, MOT 90, 成虫, 当日第2次 ✓

Round 1/3: 触发 NPC → [VETERAN]甲虫猎人阿强
            A: Accept → 战斗
            胜利！+3 树汁, SPI +0.5
            继续 Round 2

Round 2/3: 触发 RARE → "Phantom of the old tree! +6" → +6 树汁 + 1 腐木 + SPI +0.3
            [A:Next  B:Return]
            玩家按 A

Round 3/3: 触发 NPC → [LEGEND]古树守护者
            A: Accept → 战斗
            失败... MOT 降为 0
            探索强制结束

【结果】+9 树汁 + 1 腐木 + SPI +0.8, 但 MOT = 0
【时间推进】+3 小时，例如 21:30 → 00:30
【当日剩余】0 次（已用2次）
【次日状态】MOT 0 → 不可探索，需先恢复 MOT
```

---

## 八、UI 与状态栏更新建议

### 8.1 新增状态栏元素

右侧状态栏（竖向 40px）建议新增两行：

```text
┌────┐
│ADLT│  阶段
├────┤
│FOOD│  饥饿
├────┤
│♥♥ │  心情
├────┤
│24° │  温度
├────┤
│68% │  湿度
├────┤
│ MOR│  ← 当前时段（Morning缩写）
├────┤
│ 1/2│  ← 当日探索次数
└────┘
```

- 时段缩写：MOR / AFT / EVE
- 探索次数：当前次数 / 2
- 当次数用尽时显示灰色低对比

### 8.2 探索菜单更新

```
EXPLORE
  ├─ Park        [MOR] 安全
  ├─ Back Hill   [MOR] 木材
  ├─ Riverside   [MOR] 稀有
  ├─ Old Woods   [MOR] 危险
  └─ Back
```

- 每个地点后可标注当前时段的推荐度（如安全/木材/稀有/危险）
- 若当日次数用尽，全子菜单灰色禁用

---

## 九、代码接入建议（关键变更点）

### 9.1 新增状态变量（GameEngine / SaveManager）

```cpp
// GameEngine.h
uint8_t timeOfDay = 0;  // 0=Morning, 1=Afternoon, 2=Evening
uint8_t exploreCountToday = 0;  // 0-2
uint32_t currentDay = 0;  // 游戏内天数计数

enum TimeOfDay : uint8_t {
    TIME_MORNING = 0,
    TIME_AFTERNOON = 1,
    TIME_EVENING = 2,
};

void advanceTimeOfDay();  // 探索结束后调用
bool canExplore() const;  // 检查所有准入条件
```

### 9.2 ExploreScene 状态机重构

```cpp
enum class State {
    EXPLORING,        // 轮次进行中
    EVENT_POPUP,      // 普通事件结果展示
    NPC_PROMPT,       // 战斗选择
    ROUND_SUMMARY,    // 每轮结束后汇总（A:Next B:Return）
    FINAL_SUMMARY,    // 3轮完成或提前结束后的总汇总
    RETURNING,        // 返回培养缸
};

int currentRound = 1;      // 1-3
static constexpr int MAX_ROUNDS = 3;
```

### 9.3 triggerEvent() 按地点 × 时段分发

```cpp
void ExploreScene::triggerEvent(uint32_t now) {
    uint8_t location = GameEngine::ins().getExploreLocation();
    uint8_t tod = GameEngine::ins().getTimeOfDay();
    int roll = random(100);
    
    // 按地点 + 时段查表
    switch (location * 10 + tod) {
        case 0:  // Park + Morning
            if (roll < 40) eventType = SAP;
            else if (roll < 55) eventType = FOOD_SOURCE;
            // ... etc
            break;
        case 1:  // Park + Afternoon
            // ...
            break;
        // ... 共 12 种组合
    }
    
    // 非成虫保险检查
    if (eventType == NPC && bug.getStage() != Stage::ADULT) {
        showToast("Too young to fight.");
        state = State::FINAL_SUMMARY;
        return;
    }
}
```

### 9.4 新增文案（UiStrings.h）

```cpp
// 时间系统
static constexpr const char* TIME_MORNING = "MOR";
static constexpr const char* TIME_AFTERNOON = "AFT";
static constexpr const char* TIME_EVENING = "EVE";

// 探索次数
static constexpr const char* EXPLORE_COUNT_FMT = "%d/2";
static constexpr const char* EXPLORE_DAILY_LIMIT = "Daily limit reached.";
static constexpr const char* EXPLORE_MOT_LOW = "MOT too low.";

// 轮次提示
static constexpr const char* ROUND_FMT = "Round %d/3";
static constexpr const char* ROUND_NEXT = "A:Next";
static constexpr const char* ROUND_RETURN = "B:Return";
static constexpr const char* EXPLORE_COMPLETE = "Explore Complete";
static constexpr const char* EXPLORE_LEFT = "Left...";
static constexpr const char* TOO_YOUNG_TO_FIGHT = "Too young to fight.";
```

---

## 十、设计哲学总结

> **探索从"挂机刷资源"变成了"策略选择 + 轮次决策 + 时间管理"的游戏。**

| 维度 | v1.0（45秒时间制） | v2.0（3轮 + 时间制） |
|---|---|---|
| **节奏** | 持续45秒，被动等待 | 3轮主动决策，每轮按A/B |
| **风险** | 无惩罚，安全挂机 | 失败降 MOT，可能当天无法再次探索 |
| **策略** | 无（去哪都一样） | 地点 × 时段 = 12 种策略组合 |
| **时间感** | 无 | 时间成为资源，探索推进昼夜 |
| **每日循环** | 无限次 | 每日2次，稀缺性催生规划 |

**玩家决策树**：
```
今天还有几次？→ 2次：先刷 Park 上午攒资源？
                → 1次：去 Riverside 晚上赌稀有？
                → 0次：等明天，先恢复 MOT
```

---

> 如需直接生成 4 个地点 × 3 时段 = 12 张探索背景图提示词，或 24 个稀有事件的像素图标提示词，告诉我即可。
