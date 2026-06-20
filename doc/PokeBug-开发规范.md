# PokeBug 开发规范

> 本文档面向 PokeBug 项目开发者与 AI 编程助手，用于统一代码风格、协作约定与关键实现原则。
> 
> 最后更新：2026-06-19

---

## 1. 总则

- 本规范与 `AGENTS.md` 互补；若两者冲突，以本文档为准。
- 规范适用于 `src/` 目录下所有 C++ 源代码、头文件及项目文档。
- 所有改动应遵循「最小侵入」原则：只改必要文件，不破坏现有逻辑与存档兼容性。

---

## 2. UI 文案规范（强制）

### 2.1 统一英文

- **所有用户可见的 UI 文案必须使用英文**。
- 串口日志、调试信息、注释可以使用中文或英文，视上下文而定。
- 请勿在界面中混用中文与英文；历史遗留中文文案应逐步迁移到英文。

### 2.2 文案集中在 `UiStrings.h`

- **所有 UI 文案必须统一放在 `src/core/UiStrings.h` 中**，以 `static constexpr const char*` 形式声明。
- 每个文案常量需附带中文注释说明用途。
- 按场景/模块分组，例如：
  - 通用（`BACK`, `PLACE`）
  - 菜单（`MENU_INFO`, `MENU_FEED`）
  - 设置（`SET_BRIGHTNESS`）
  - 对战（`BATTLE_HP`, `BATTLE_MOT`）
  - 探索（`EXPLORE_FIGHT`, `EXPLORE_FLED`）
  - 甲虫杯（`CUP_BRACKET`, `CUP_CHAMPION`）

```cpp
// Menu 场景（主菜单）
static constexpr const char* MENU_INFO        = "Info";        // 信息
static constexpr const char* MENU_FEED        = "Food";        // 食物
```

### 2.3 禁止硬编码

- **任何场景代码中不允许出现字面量 UI 文案**。
- 需要格式化数值时，文案字符串可保留 `printf` 占位符（如 `"Sap +%d"`），由调用方使用 `snprintf` 填充。

```cpp
// ✅ 正确
snprintf(buf, sizeof(buf), UiStrings::EXPLORE_SAP_PLUS, eventValue);
PixelRenderer::drawPixelText(x, y, buf, color, fs);

// ❌ 错误
PixelRenderer::drawPixelText(x, y, "发现树汁", color, fs);
```

### 2.4 命名约定

- 常量使用 `SCREAMING_SNAKE_CASE`。
- 前缀按模块划分：
  - `MENU_`：主菜单与子菜单项
  - `SET_`：设置界面
  - `BATTLE_`：对战场景
  - `LOBBY_`：对战大厅
  - `EXPLORE_`：探索模式
  - `CUP_`：甲虫杯
  - `INFO_`：信息页
  - `RESET_` / `IDLE_` 等：通用或特定功能

### 2.5 多语言预留

- 当前为英文版本；若未来需要多语言，只需替换 `UiStrings.h` 中的字符串值，调用方无需改动。
- 因此文案常量名必须语义清晰，不能依赖具体语言文本。

---

## 3. 代码组织

### 3.1 目录职责

```
src/
├── main.cpp              # 入口；区分 Deep Sleep 唤醒路径
├── core/                 # 引擎、场景基类、存档、按钮分发
├── game/                 # 游戏逻辑：Bug、对战公式、NPC 生成
├── hardware/             # 硬件抽象：Hal、渲染器、BattleLink
├── scenes/               # 场景实现
└── assets/               # 图片、RLE 精灵、NPC 数据
```

### 3.2 单例

- 主要单例：`GameEngine`, `Hal`, `SaveManager`, `BattleLink`, `ButtonDispatcher`。
- 单例实现：构造函数私有化，提供 `static T& ins()`。

### 3.3 场景切换

- 通过 `Scene::nextScene` 请求切换，由 `GameEngine::update()` 检测后调用 `switchScene()`。
- **不要在 `render()` 中切换场景**。

---

## 4. 命名与风格

### 4.1 命名

- 类/结构体：`PascalCase`
- 函数/变量：`camelCase`
- 常量：`SCREAMING_SNAKE_CASE`
- 私有成员：可加下划线后缀或直接用 `m` 前缀，项目内保持统一即可

### 4.2 格式

- 缩进：**4 个空格**，不使用 Tab。
- 头文件保护：使用 `#pragma once`。
- 整数类型：优先使用 `<cstdint>` 定宽类型（`uint8_t`, `uint32_t` 等）。
- 时间单位：代码中以毫秒（ms）为主；存档中可除以 1000 以秒存储。

### 4.3 注释

- **代码注释以中文为主**，保持与设计文档一致。
- 关键算法、状态机分支、边界处理必须注释设计意图。
- 新增功能请在头文件和关键实现处写明设计意图。

---

## 5. 硬件与渲染

### 5.1 禁止直接调用 M5.xxx

- 显示、按键、IMU 统一通过 `Hal` 访问。
- NVS 统一通过 `SaveManager` 访问。

### 5.2 渲染流程

- 所有场景绘制到 `Hal::ins().canvas()`（240×135 RGB565 离屏 Sprite）。
- 每帧最后调用 `Hal::ins().flush()` 推送到屏幕。
- 文本绘制统一走 `PixelRenderer::drawPixelText()`，已禁用自动换行，超长文本需调用方自行截断或分行。

---

## 6. 存档与版本

### 6.1 存档版本

- `SaveManager::SAVE_VERSION` 与 `Bug::save()` / `load()` 必须保持一致。
- 新增字段时递增版本号，并保留旧版本兼容迁移路径。

### 6.2 写入保护

- 自动保存周期 30 秒；深睡前强制保存一次。
- 避免频繁写入 NVS。

---

## 7. 按钮与输入

### 7.1 按钮分发

- 通过 `ButtonDispatcher` 订阅，支持优先级；返回 `true` 表示消费事件。
- 长按阈值 500 ms，消抖 20 ms；长按触发后不再触发短按。

### 7.2 交互映射

- 短按 A：确认 / 喂食 / 对战加油
- 长按 A：打开菜单 / 返回主界面
- 短按 B：切换 / 戳甲虫
- 长按 B：返回上一级 / 从子菜单返回

---

## 8. 网络与通信

### 8.1 ESP-NOW

- MVP 中 `encrypt = false`，请勿传输敏感信息。
- 对战结束后调用 `BattleLink::end()` 关闭 WiFi/BT 以降低功耗。

### 8.2 本地 NPC 对战

- 通过 `GameEngine::setPendingNpcBattle()` 接收对手数据，跳过 ESP-NOW。
- 本地 NPC 战不依赖 `BattleLink::isHost()`，应在应用回合结果时显式按 host 视角处理。

---

## 9. 文档维护

- 修改 UI 文案、菜单结构、对战流程、存档格式等玩家可见行为时，必须同步更新：
  - `AGENTS.md`
  - 本文档
  - 相关 `doc/PokeBug-*.md` 设计文档
- 新增模块请在 `AGENTS.md` 的代码组织图中补充。

---

## 10. 检查清单

提交前请确认：

- [ ] 新增 UI 文案已加入 `UiStrings.h` 且为英文
- [ ] 场景代码中无硬编码 UI 文案
- [ ] 编译通过（`pio run`）
- [ ] 存档版本一致（如修改了 Bug 序列化）
- [ ] 相关文档已更新

---

*最后更新：2026-06-19*
