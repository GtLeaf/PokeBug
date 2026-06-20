# AGENTS.md — PokeBug 项目指南

> 本文件面向 AI 编程助手。若你是第一次接触本项目，请先阅读此文档，再修改代码。
>
> 最后更新：2026-06-19（新增探索模式、甲虫杯、本地 NPC 对战、存档 v8）

---

## 1. 项目概述

**PokeBug（口袋昆虫）** 是一款运行在 **M5Stack StickS3**（ESP32-S3）上的像素风口袋昆虫养成电子宠物。

- 玩家通过按键与 IMU 体感交互，饲养一只独角仙，经历 **卵 → 幼虫 → 蛹 → 青年期 → 成虫** 的完整生命周期。
- 养成阶段的喂食、静置、摇晃等行为会影响 6 项属性（SIZ / STR / END / SPI / SPD / MOT），最终通过 **ESP-NOW 1v1 直连对战** 验证养成成果。
- 项目当前处于 **MVP（v0.1）** 阶段，详细设计文档见 `doc/PokeBug-脑暴.md`。

### 1.1 技术栈

| 层级 | 技术 |
|---|---|
| 构建系统 | PlatformIO（`platformio.ini`） |
| 硬件平台 | Espressif ESP32-S3（`esp32-s3-devkitc-1`） |
| 开发框架 | Arduino（`framework = arduino`） |
| 硬件抽象 | M5Unified + M5GFX |
| 联机通信 | ESP-NOW（基于 WiFi STA） |
| 存档 | Arduino `Preferences`（NVS） |
| 语言标准 | C++（Arduino 框架默认 gnu++11） |

### 1.2 关键配置

- `platformio.ini`：环境 `m5stick-s3`，平台 `espressif32 @ 6.6.0`，板型 `esp32-s3-devkitc-1`，内存类型 `qio_opi`，分区表 `default_8MB.csv`。
  - 构建标志：`-DBOARD_HAS_PSRAM`、`-DARDUINO_USB_CDC_ON_BOOT=1`、`-DARDUINO_USB_MODE=1`。
  - 依赖：`m5stack/M5Unified@^0.2.3`、`m5stack/M5GFX@^0.2.3`。
  - 上传协议：`esptool`；串口波特率：`115200`。
- `default_8MB.csv`：8 MB Flash 分区表，包含 NVS、PHY、Factory、OTA0、OTA1、SPIFFS。
- `.vscode/extensions.json`：推荐使用 `platformio.platformio-ide`。
- `README.md`：当前仅含标题的 UTF-16 占位文件。
- `include/README`、`lib/README`、`test/README`：PlatformIO 生成的说明占位文件。

---

## 2. 代码组织

源码集中在 `src/`，按职责分层：

```
src/
├── main.cpp                          # 程序入口；区分 Deep Sleep 定时器唤醒与正常启动路径
├── core/                             # 引擎与核心基础设施
│   ├── GameEngine.{h,cpp}            # 单例主引擎：初始化、主循环、场景调度、idle/深睡/自动存档
│   ├── Scene.h                       # 场景基类（onEnter/onExit/update/render/onButton）
│   ├── ButtonDispatcher.{h,cpp}      # 按钮事件分发器（观察者模式，消抖 + 短按/长按）
│   └── SaveManager.{h,cpp}           # NVS 存档读写（Bug 数据 + 用户设置）
├── game/                             # 游戏逻辑
│   ├── Bug.{h,cpp}                   # 独角仙实体：生命周期、属性、基因、背包、存档序列化
│   ├── BattleCalc.h                  # 对战公式：HP、伤害、暴击、先攻、MOT 衰减
│   └── NpcGenerator.h                # NPC 生成器（探索 & 甲虫杯共用）
├── hardware/                         # 硬件抽象与通信
│   ├── Hal.{h,cpp}                   # M5Unified 封装：显示、按键、IMU、电池、背光
│   ├── PixelRenderer.{h,cpp}         # 像素精灵/UI 渲染辅助（基于 M5GFX/LGFX_Sprite）
│   └── BattleLink.{h,cpp}            # ESP-NOW 1v1 对战链路：房间、配对、回合同步
├── scenes/                           # 场景实现
│   ├── TerrariumScene.{h,cpp}        # 培养缸主场景
│   ├── MenuScene.{h,cpp}             # 图标菜单（含 Box 子菜单）
│   ├── LobbyScene.{h,cpp}            # 对战大厅（创建/搜索房间）
│   ├── InfoScene.{h,cpp}             # 属性信息页（2 页：状态 / 战绩）
│   ├── SettingsScene.{h,cpp}         # 设置界面（亮度/字体/速度/idle/重置）
│   ├── BattleScene.{h,cpp}           # 对战场景（ESP-NOW / 本地 NPC 双模式）
│   ├── ExploreScene.{h,cpp}          # 探索模式：倾斜移动、随机事件、NPC 遭遇、放生
│   └── CupScene.{h,cpp}              # 甲虫杯：通知、对阵表、淘汰赛、结算
└── assets/
    ├── MainSceneAssets.h             # 主场景图片资源声明
    ├── MainSceneAssets.cpp           # 主场景图片 PROGMEM 数据
    ├── FoodAssets.{h,cpp}            # 食物 RLE 精灵
    ├── BowlAssets.{h,cpp}            # 食物盘 RLE 精灵
    ├── WoodAssets.{h,cpp}            # 腐木 RLE 精灵
    ├── HerculesAdultSprites.{h,cpp}  # 成虫动画 RLE 精灵
    ├── ActionAssets.{h,cpp}          # 手指戳动作 RLE 精灵
    └── NpcData.h                     # 12 个 NPC 的 PROGMEM 数据（档位、名字、台词）
```

### 2.1 架构要点

- **单例模式**：`GameEngine::ins()`、`Hal::ins()`、`SaveManager::ins()`、`BattleLink::ins()`、`ButtonDispatcher::ins()` 是项目主要单例。
- **场景状态机**：`GameEngine::switchScene()` 拥有当前 `Scene*` 指针，每帧调用 `update()` / `render()`，并通过 `onButton()` 处理输入。场景通过设置 `nextScene` 请求切换，`GameEngine::update()` 检测到后执行切换。
- **按钮分发**：`ButtonDispatcher` 支持按优先级订阅，事件一旦被消费（handler 返回 `true`）即停止后续分发。长按阈值 **500 ms**，消抖 **20 ms**。
- **虚拟时间**：`GameEngine` 维护 `gameNow`（ms），每帧按 `gameSpeed` 推进；Deep Sleep 唤醒时会一次性推进 10 分钟虚拟时间。
- **渲染流程**：所有场景绘制到 `Hal::ins().canvas()`（240×135 RGB565 离屏 Sprite），最后 `Hal::ins().flush()` 推送到屏幕。
- **省电与深睡**：
  - 主场景无操作达到 idle 时间后降低背光，再超过 1 分钟进入 **Deep Sleep**。
  - Deep Sleep 使用 10 分钟（600 s）定时器唤醒；唤醒路径在 `main.cpp` 中仅做最小化状态更新，避免点亮屏幕。
- 对战时 `BattleLink` 开启 WiFi STA；对战结束后 `BattleLink::end()` 关闭 WiFi/BT 以降低功耗。
- **本地 NPC 对战**：`BattleScene` 通过 `GameEngine::setPendingNpcBattle()` 接收对手数据，跳过 ESP-NOW 流程，直接本地计算回合并结算。用于探索模式遭遇战与甲虫杯淘汰赛。
- **Fight 子菜单**：主菜单 `Fight` 进入子菜单，包含 `PvP`（ESP-NOW 对战大厅）和 `Cup`（甲虫杯）。Cup 仅在成虫、存活且饥饿度 ≥ 30 时可选，不满足时置灰，按 A 弹出提示框说明原因。
- **UI 文案**：`ExploreScene` 与 `CupScene` 内所有用户可见字符串统一收归到 `UiStrings.h`，当前为英文。

### 2.2 探索模式

- **入口**：主菜单 `🌲 Explore`（成虫且饥饿度 ≥ 30）。
- **移动**：IMU 左右倾斜控制甲虫移动，到边缘自动回头。
- **事件**：每 5–10 秒触发一次随机事件（树汁、食物源、腐木、NPC、稀有事件、无事）。
- **NPC 对战**：按探索档位权重（新手 40% / 普通 35% / 老手 20% / 传说 5%）生成对手；传说级不可逃跑。
- **放生**：探索中长按 A 呼出放生确认，确认后基于当前基因小幅变异产生新卵（generation +1）。
- **消耗与奖励**：进入时扣 20 饥饿度；事件与 NPC 战胜利奖励树汁/SPI/腐木。

### 2.3 甲虫杯

- **入口**：`Fight` 子菜单 → `Beetle Cup`；仅成虫、存活、饥饿度 ≥ 30 且处于报名开放期时可选，不满足时置灰，按 A 弹出提示说明原因。
- **周期**：基于 `gameNow` 虚拟时间，每 **7 游戏天** 一届新赛季，每届只有 **1 游戏天** 的报名窗口。报名窗口关闭后如果玩家未参赛，本赛季自动结束且没有任何奖励。
- **流程**：通知 → 8 强对阵表 → 三轮本地 NPC 对战（Quarter → Semi → Final）→ 名次结算。
- **NPC 分布**：新手 10% / 普通 30% / 老手 40% / 传说 20%，整体强于探索。
- **奖励**：冠军 +6 树汁 +1 腐木，亚军 +4，四强 +2，八强 +1；未参赛无奖励。同时更新本虫杯赛记录与成就。

### 2.4 培养缸状态栏（右侧 40×135）

状态栏聚焦**甲虫状态 + 虚拟环境**，不再展示背包（Sap/Wood）。

| 区块 | 内容 | 说明 |
|---|---|---|
| 阶段图标 | E / L / P / A | 当前生命周期阶段 |
| 饥饿度 | 32×6 水平进度条 | 绿/黄/红三段，无数值 |
| 心情（MOT） | **7×6 像素爱心**，从底部像水位一样填充 | **红=填充**（高心情）、灰=空（低心情），按 MOT 百分比填充行数 |
| 温度 | 黄色小点 + 数值 | **模拟值**，基于 `gameNow` 昼夜波动（18-32°C） |
| 湿度 | 蓝色小点 + 数值 | **模拟值**，与温度反向波动（30-85%） |
| 虚拟时间 | HH:MM | 与 `gameSpeed` 同步，比现实流速快 |

> 环境温湿度为**模拟微缩生态**，当前无真实传感器（M5StickS3 不自带温湿度）。后续外接 SHT30 或接入天气系统时，只需替换 `TerrariumScene::drawStatusBar()` 中的模拟计算层，UI 布局无需改动。

---

## 3. 构建、烧录与调试

### 3.1 环境要求

- 安装 [PlatformIO Core](https://platformio.org/install/cli) 或 VS Code + PlatformIO IDE 扩展。
- 将 M5StickS3 通过 USB 连接到电脑。

### 3.2 常用命令

```bash
# 编译
pio run

# 烧录到设备
pio run --target upload

# 打开串口监视器（波特率 115200）
pio device monitor --baud 115200

# 编译 + 烧录 + 监视器（常用组合）
pio run --target upload && pio device monitor --baud 115200

# 运行 PlatformIO 单元测试（当前 test/ 下无实际测试文件）
pio test

# 清理构建产物
pio run --target clean
```

### 3.3 运行与调试

- 串口输出使用 `Serial.println()` / `Serial.printf()`，可用于追踪状态机、对战通信、存档读写。
- `BattleScene` 内置 `maybeLogStateStall()`，每 2 秒打印一次对战状态，方便定位 ESP-NOW 卡死。
- 在 VS Code 中可直接使用 PlatformIO 扩展的「Build / Upload / Monitor」按钮。

---

## 4. 代码风格指南

### 4.1 语言与注释

- **代码注释以中文为主**，保持与设计文档一致。新增功能请在头文件和关键实现处用中文写明设计意图。
- 类、函数、变量命名采用英文驼峰/帕斯卡风格；常量使用 `SCREAMING_SNAKE_CASE`。

### 4.2 格式约定

- 缩进：**4 个空格**（不要 Tab）。
- 头文件保护：使用 `#pragma once`。
- 单例实现：构造函数私有化，提供 `static T& ins()`。
- 整数类型：嵌入式场景优先使用 `<cstdint>` 中的定宽类型（`uint8_t`、`uint32_t` 等）。
- 时间单位：代码中以 **毫秒（ms）** 为主；存档中为节省空间会除以 1000 以秒为单位存储。

### 4.3 设计约定

- **不要直接调用 `M5.xxx`**：显示/按键/IMU 统一通过 `Hal` 访问，便于移植和测试。
- **不要直接操作 NVS**：使用 `SaveManager`；`Bug` 自身提供 `save()` / `load()` 序列化，但 NVS 开启/关闭由 `SaveManager` 负责。
- **场景切换**：通过 `Scene::nextScene` 设置目标，由 `GameEngine::update()` 检测到后调用 `switchScene()`，不要在 `render()` 中切换场景。
- **按钮事件**：`onButton()` 返回 `true` 表示已消费；长按事件触发后不会再触发短按。
- **避免在 ISR 中调用 `Serial.print`**：当前回调均在主循环上下文处理，符合规范。

---

## 5. 核心机制说明

### 5.1 生命周期与阶段时长

当前阶段时长由 `gameSpeed` 控制，无单独测试值。深睡 10 分钟唤醒会一次性推进 10 分钟虚拟时间。正式上线时阶段时长如下：

| 阶段 | 设计值 |
|---|---|
| 卵期 | 5 min |
| 幼虫期 | 30 min |
| 蛹期 | **30 min** |
| 青年期 | **60 min**（可喂食、可对站） |
| 成虫期 | ∞（无自然上限，饥饿死亡） |

定义位置：`src/game/Bug.h` 中的 `*_DURATION_MS` 常量。

### 5.2 属性与基因

- 每只独角仙有 4 组基因（VIG / ATK / MNT / APP），每组 1 字节 `[显性4bit | 隐性4bit]`。
> 基因在 `Bug::initNew()` 时均匀随机生成；显性值影响成长倍率（0.8 ~ 2.3），显+隐平均值影响属性上限（**SIZ/STR/END/SPI/SPD 均为 6 ~ 21**）。
- **VIG** → SIZ + END；**ATK** → STR；**MNT** → SPI；**APP** → SPD + 外观调色板（`getPaletteId()`）。**颜色越深，SPD 成长倍率越高、上限越高**。
- MVP 中基因数值不直接展示，仅通过孵化提示语（`Bug::getHatchHint()`）和外观调色板（`Bug::getPaletteId()`，0-3）让玩家感知差异。
- 属性：**SIZ / STR / END / SPI / SPD 为 1.0 ~ 21.0 的浮点（基因上限 6~21）**；MOT 为 0-100 的整数；饥饿度 hunger 为 0-100。
- **基因 APP 同时影响调色板（外观）与 SPD 上限/倍率**。
- **卵期气质（Temperament）**：参考宝可梦性格，卵期交互行为（倾斜 ≥ 30% 总时长 / 戳 ≥ 4 次 / 喷水 ≥ 4 次 / 摇晃 ≥ 4 次 + 剧烈 ≥ 1 次）决定 6 种气质之一（迅捷/韧甲/巨体/蛮力/均衡/灵心），其中 5 种提升一项属性 ×1.10、降低一项属性 ×0.90，**均衡**无增益无减益，**灵心**兜底。判定三层：L1 倾斜（最高优先级）→ L2 操作类次数最高者（平局最近触发）→ L3 均衡（操作类 1~3）→ L4 灵心兜底。倾斜按虚拟时间累计，受 `gameSpeed` 影响。气质终身影响所有阶段成长。详见 `doc/PokeBug-FoodSystem.md` §11。
- **环境物品加成**：食物盘有食物时所有属性吸收 × 1.05，特定食物再 × 1.03（对应属性）；空盘 MOT 衰减 +20%。腐木 5 种风格各有 3% 属性倾向，成虫休息时对应属性恢复 +0.02/min。食物盘 + 腐木是长期投资，影响整个阶段。详见 `doc/PokeBug-FoodSystem.md` §13。

### 5.3 交互映射

| 交互 | 短按/动作 | 长按 |
|---|---|---|
| A | 喂食 / 确认 / 对战加油 / 菜单中切换 | 打开菜单（主场景）/ 返回主界面（子场景） |
| B | 戳甲虫 / 切换选项 | 从子菜单返回 / 培养缸下进入 Deep Sleep |
| 左倾斜 | 甲虫朝左爬行（仅成虫） | — |
| 右倾斜 | 甲虫朝右爬行（仅成虫） | — |

- 死亡后：在培养缸长按 **A + B 3 秒** 重置存档，世代数 `generation` +1。
- 摇晃：加速度模长 > 2.0g 且 500 ms 冷却；不同生命阶段效果不同（卵期延长孵化、幼虫/蛹降低 SPI、成虫训练 STR/MOT）。
- 倾斜：仅成虫有效。设备左侧向下（`ax > +0.5g`）→ 甲虫朝左爬行；右侧向下（`ax < -0.5g`）→ 朝右爬行。方向变化需防抖 200 ms。EAT/TURN 状态不响应。放下设备（回死区）后甲虫继续走到目标再停。
- 戳甲虫（短按 B）：成虫后退 5 像素并切换戒备姿态（RESET 帧）；卵左右晃动；幼虫缩成一团；蛹无反应。冷却期间再次按 B 会在甲虫上方闪烁提示。

### 5.4 对战

- 通过 `LobbyScene` 进入对战大厅：创建房间或搜索附近房间，选择后加入。
- 房主（创建者）自动成为 **主机 authoritative**，统一计算双方伤害、HP、MOT；加入者为从机，按主机下发的状态更新画面。
- 同步属性后自动战斗直到一方 HP 归零 → 结算。
- **先攻判定**：每回合开始时按 SPD 对比，高 SPD 方先出手；相同则随机。先攻方 KO 敌方则敌方无反击。
- **SPD 多段攻击**：每回合 SPD 高的一方将 SPD 差值累加到 `spdGauge`；当 gauge >= 10 时，下回合获得一次额外攻击（50% 伤害），gauge 清0。详见 `doc/PokeBug-FoodSystem.md` §10。
- 每回合冲锋阶段（CHARGE）可按 A 加油，MOT +15；从机加油后的 MOT 会在 CLASH 阶段通过 `MSG_BATTLE_READY` 上报主机。
- 伤害公式见 `src/game/BattleCalc.h`；通信协议与包结构见 `src/hardware/BattleLink.h`。

### 5.5 存档

- `SaveManager` 使用 NVS 命名空间 `pokebug`，当前 Bug 存档版本 **`SAVE_VERSION = 8`**（定义于 `SaveManager.h` 与 `Bug.cpp`）。
- `Bug::save()` / `Bug::load()` 负责二进制序列化；`SaveManager` 负责 NVS 开启/关闭与版本校验。
- v8 在 v7 基础上新增探索/杯赛字段：`releaseCountTotal`、`cupParticipated`、`cupBest`、`cupWins`、`cupLegendKills`、`achievementFlags`、`cupStreak`。
- 全局杯赛数据独立保存：`cup_season`（届数）、`cup_game_time`（上一届开始时的虚拟游戏时间，秒）、`cup_state`（周期状态：0=IDLE,1=REGISTER_OPEN,2=REGISTER_EXPIRED,3=IN_PROGRESS），旧 key `last_cup_time` 仍可在首次升级时读取兼容，换虫/死亡/放生均不影响。
- 当前实现兼容读取 v5 / v6 / v7 旧存档；版本不匹配且无法兼容时会拒绝加载并创建新存档。
- 设置项独立保存：字体缩放、亮度、游戏速度、idle 时间档位、主场景背景、腐木风格、食盘风格、食物风格。

### 5.6 省电

- `GameEngine` 在培养缸主场景下无操作达到 idle 时间后降低背光，再超过 1 分钟进入 **Deep Sleep**。
- idle 时间档位（`GameEngine::idleTimeoutIndex`）：0=30s、1=1m、2=2m、3=5m、4=Never。
- 游戏速度（`gameSpeed`）可选 0.5x / 1x / 2x / 4x / 8x。
- Deep Sleep 使用 10 分钟（600 s）定时器唤醒；唤醒路径在 `main.cpp` 中更新一次虚拟时间后再次进入 Deep Sleep。
- 对战时会开启 WiFi STA；对战结束后 `BattleLink::end()` 关闭 WiFi/BT 以降低功耗。

---

## 6. 测试策略

- 当前 `test/` 目录仅含 PlatformIO 占位 README，项目未实现单元测试。
- 推荐在 `test/` 下按 PlatformIO 单元测试框架编写针对以下模块的测试：
  - `Bug` 的生命周期推进、属性计算、存档序列化与版本兼容性。
  - `BattleCalc` 的伤害/HP/MOT 衰减边界条件。
  - `ButtonDispatcher` 的消抖、短按、长按、优先级分发。
- 集成测试以真机验证为主：串口日志 + 实际两台设备对战验证 ESP-NOW 流程。

---

## 7. 部署与发布

- 目标设备：M5StickS3 或兼容的 ESP32-S3 开发板（需支持 ST7789V 240×135 屏幕与 IMU）。
- 烧录协议：`esptool`（`upload_protocol = esptool`）。
- 首次烧录后，NVS 中没有存档，程序会自动创建新独角仙。
- 发布前务必：
  1. 检查 `SaveManager::SAVE_VERSION` 与 `Bug::save()` / `load()` 是否一致；
  2. 验证存档升级路径（v5/v6/v7 可迁移，更早版本会创建新存档）；
  3. 确认 `platformio.ini` 中的分区表、依赖版本与硬件匹配；
  4. 验证探索模式、甲虫杯、本地 NPC 对战的真机流程与存档持久化。

---

## 8. 安全与注意事项

- **不要频繁写入 NVS**：`SaveManager` 已有并发写入保护；自动保存周期为 30 秒，深睡前会强制保存一次。
- **ESP-NOW 无加密**：MVP 中 `encrypt = false`，请勿在对战中传输敏感信息。
- **Deep Sleep 唤醒路径**：`main.cpp` 中 Deep Sleep 唤醒后不会初始化 `GameEngine` 和显示，仅做最小化状态更新，避免 OLED/背光意外点亮。
- **避免在 ISR 中调用 `Serial.print`**：当前回调均在主循环上下文处理，符合规范。

---

## 9. 参考文档

- `platformio.ini`：构建配置。
- `doc/PokeBug-脑暴.md`：完整游戏设计文档（中文），包含数值设计、UI 布局、迭代路线。
- `doc/PokeBug-FoodSystem.md`：食物系统与属性成长规格（6 种食物 × 5 阶段 × 阶段吸收系数模型）。
- `doc/PokeBug-ExploreAndCup.md`：探索模式与甲虫杯设计文档（NPC 4 档体系、事件表、淘汰赛、放生机制）。
- `default_8MB.csv`：Flash 分区表。
- PlatformIO 文档：https://docs.platformio.org/
- M5Unified / M5GFX 文档：https://docs.m5stack.com/

---

*最后更新：2026-06-19*
