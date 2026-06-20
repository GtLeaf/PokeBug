# PokeBug — 成虫视觉开发规格 v1.0

> 基于 `doc/PokeBug-FoodSystem.md` §11.7 的详细实现文档
> 面向开发者：渲染系统、调色板、体型缩放的技术规格

---

## 1. 设计目标

玩家看到培养缸中的成虫时，应能**一眼读出三项信息**：

1. **气质流派**（色相）—— 这是什么类型的甲虫
2. **养成进度**（深浅）—— 对应属性养到什么程度了
3. **体型大小**（缩放）—— 仅巨体气质可见，SIZ 越高体型越大

---

## 2. 气质 → 色相

### 2.1 映射表

| 气质枚举 | 名称 | 色相 | 直觉 | 主色 RGB565 | 副色 RGB565 |
|---|---|---|---|---|---|
| `Temperament::BRUTE` | 蛮力 | **深红** | 血性/攻击 | `0xF800` | `0xA000` |
| `Temperament::SWIFT` | 迅捷 | **灰蓝** | 轻灵/速度 | `0x6B7D` | `0x3A5A` |
| `Temperament::GIANT` | 巨体 | **橙褐** | 厚重/大体型 | `0xFD20` | `0xA200` |
| `Temperament::RESILIENT` | 韧甲 | **金色** | 坚硬/防御 | `0xFE00` | `0xA600` |
| `Temperament::BALANCED` | 均衡 | 白/浅灰 | 中性 | `0xFFFF` | `0xBDF7` |
| `Temperament::SPIRIT` | 灵心 | 青绿 | 幽玄 | `0x07E0` | `0x03C0` |

### 2.2 色相与属性的对应关系

| 气质 | 提升属性 | 对应属性查询 |
|---|---|---|
| 蛮力 | STR ×1.10 | `bug.getStr()` / `bug.strCap()` |
| 迅捷 | SPD ×1.10 | `bug.getSpd()` / `bug.spdCap()` |
| 巨体 | SIZ ×1.10 | `bug.getSiz()` / `bug.sizCap()` |
| 韧甲 | END ×1.10 | `bug.getEnd()` / `bug.endCap()` |
| 均衡 | 无 | 五维平均 |
| 灵心 | SPI ×1.10 | `bug.getSpi()` / `bug.spiCap()` |

> 色相不随进度变化，只随气质固定。同一气质的甲虫，无论刚成虫还是大成，主色相始终相同。

---

## 3. 深浅 → 养成进度

### 3.1 计算 ratio

```cpp
// Bug.h 中新增方法
float getAdultDepth() const {
    switch (temperament) {
        case Temperament::BRUTE:      return str / strCap();
        case Temperament::SWIFT:      return spd / spdCap();
        case Temperament::GIANT:      return siz / sizCap();
        case Temperament::RESILIENT:  return end / endCap();
        case Temperament::SPIRIT:     return spi / spiCap();
        case Temperament::BALANCED:
            return (siz/sizCap() + str/strCap() + end/endCap() + spd/spdCap() + spi/spiCap()) / 5.0f;
    }
    return 0.0f;
}
```

### 3.2 四档深浅

| 档位 | ratio 范围 | 视觉处理 | 玩家感知 |
|---|---|---|---|
| **初阶** | 0.00 ~ 0.25 | 基础色相 + 大量混入灰/白色，饱和度极低 | 苍白，刚起步 |
| **成长** | 0.25 ~ 0.50 | 基础色相 + 少量混入灰色，饱和度中等 | 淡雅，成长中 |
| **成型** | 0.50 ~ 0.75 | 基础色相标准饱和度（标准色） | 正常，已成型 |
| **大成** | 0.75 ~ 1.00 | 基础色相 + 提高亮度 20~30%，形成光泽感 | **发光，接近极限** |

### 3.3 颜色混合算法

```cpp
// 基础色相 = 气质对应的 RGB565 主色
// 以 RGB565 转换为 RGB888 进行插值，再转回 RGB565

uint16_t mixColor(uint16_t base, uint16_t mix, float mixRatio) {
    // 提取 R/G/B 各 5/6/5 位
    uint8_t baseR = (base >> 11) & 0x1F;
    uint8_t baseG = (base >> 5) & 0x3F;
    uint8_t baseB = base & 0x1F;
    uint8_t mixR = (mix >> 11) & 0x1F;
    uint8_t mixG = (mix >> 5) & 0x3F;
    uint8_t mixB = mix & 0x1F;
    
    uint8_t r = (uint8_t)(baseR * (1 - mixRatio) + mixR * mixRatio);
    uint8_t g = (uint8_t)(baseG * (1 - mixRatio) + mixG * mixRatio);
    uint8_t b = (uint8_t)(baseB * (1 - mixRatio) + mixB * mixRatio);
    
    return (r << 11) | (g << 5) | b;
}

// 大成档位：提高亮度
uint16_t brighten(uint16_t color, float factor) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    r = min((uint8_t)(r * factor), (uint8_t)0x1F);
    g = min((uint8_t)(g * factor), (uint8_t)0x3F);
    b = min((uint8_t)(b * factor), (uint8_t)0x1F);
    return (r << 11) | (g << 5) | b;
}
```

### 3.4 24 色调色板生成

运行时根据气质和 ratio 动态生成 2 色（主色+副色）：

```cpp
struct AdultPalette {
    uint16_t main;   // 甲壳主色
    uint16_t shadow; // 甲壳阴影/副色
};

AdultPalette getAdultPalette(Temperament t, float ratio) {
    uint16_t baseMain = BASE_COLORS[(uint8_t)t][0];   // 基础主色
    uint16_t baseShadow = BASE_COLORS[(uint8_t)t][1]; // 基础副色
    
    uint16_t gray = 0x8410;  // 中灰色，用于混入
    uint16_t white = 0xFFFF; // 白色
    
    AdultPalette pal;
    if (ratio < 0.25f) {
        // 初阶：大量混入灰+白
        pal.main = mixColor(mixColor(baseMain, gray, 0.5f), white, 0.3f);
        pal.shadow = mixColor(mixColor(baseShadow, gray, 0.5f), white, 0.3f);
    } else if (ratio < 0.50f) {
        // 成长：少量混入灰
        pal.main = mixColor(baseMain, gray, 0.3f);
        pal.shadow = mixColor(baseShadow, gray, 0.3f);
    } else if (ratio < 0.75f) {
        // 成型：标准色
        pal.main = baseMain;
        pal.shadow = baseShadow;
    } else {
        // 大成：提高亮度 25%
        pal.main = brighten(baseMain, 1.25f);
        pal.shadow = brighten(baseShadow, 1.15f);
    }
    return pal;
}
```

> 也可以预计算 24 色调色板（6 气质 × 4 深浅）存入 `PROGMEM`，避免运行时浮点运算。但动态生成开销极小，可接受。

---

## 4. 巨体型体缩放

### 4.1 缩放规则

仅当 `temperament == Temperament::GIANT` 时生效：

| SIZ 范围 | 缩放倍率 | 代码 |
|---|---|---|
| SIZ < 10 | 0.9x | `scale = 0.9f` |
| 10 ≤ SIZ < 18 | 1.1x | `scale = 1.1f` |
| SIZ ≥ 18 | 1.2x | `scale = 1.2f` |

### 4.2 非巨体

其他气质固定 `scale = 1.0f`，不缩放。

### 4.3 缩放实现

复用 `PixelRenderer::drawRgb565RleScaled()`（已支持 `float scale` 参数）：

```cpp
// TerrariumScene::drawAdult
float scale = 1.0f;
if (bug.getTemperament() == Temperament::GIANT) {
    float siz = bug.getSiz();
    if (siz < 10.0f) scale = 0.9f;
    else if (siz < 18.0f) scale = 1.1f;
    else scale = 1.2f;
}

// 绘制时使用 scale 参数
PixelRenderer::drawRgb565RleScaled(x - frameW * scale / 2, y - frameH * scale,
                                   frameW, frameH,
                                   data, offset, length,
                                   scale, flipSprite);
```

> 注意：缩放后 `x` 坐标需要调整，让精灵底部中心对齐原 `x` 位置。`drawRgb565RleScaled` 内部以左上角为锚点，所以 `x` 需要左移 `frameW * scale / 2`。

---

## 5. 精灵着色（颜色替换）

### 5.1 当前问题

`HerculesAdultSprites` 中的精灵数据是**原始 RGB565 颜色**，直接绘制。要实现气质色相，需要将精灵中的**原始主色/副色**替换为**目标主色/副色**。

### 5.2 原始颜色基准

需要先确定 `HerculesAdultSprites` 中成虫精灵的原始主色和副色。假设原始主色为 `RAW_MAIN`（如棕色），原始副色为 `RAW_SHADOW`（如深棕）。

### 5.3 替换算法

在 `drawRgb565Rle` 或 `drawRgb565RleScaled` 中增加颜色替换：

```cpp
// 绘制时逐像素替换
uint16_t replaceColor(uint16_t raw, uint16_t rawMain, uint16_t rawShadow,
                      uint16_t targetMain, uint16_t targetShadow) {
    if (raw == rawMain) return targetMain;
    if (raw == rawShadow) return targetShadow;
    // 中间色：根据与 rawMain/rawShadow 的距离进行插值
    // 简化：只替换 exact match，其他颜色保持原样
    return raw;
}
```

> 如果精灵中使用了多种中间色（如棕→深棕的渐变），需要更复杂的替换：将颜色映射到 rawMain→targetMain 的线性空间。

### 5.4 简化方案（推荐）

如果原始精灵只有 2 种颜色（主色+副色），直接 `exact match` 替换即可。如果有多色，考虑：

1. **重新绘制精灵**：使用 2 色索引（0=主色，1=副色，2=透明），绘制时替换为实际 RGB565。但需修改 RLE 编码格式。
2. **运行时映射**：在 `drawRgb565Rle` 中增加一个 `std::function<uint16_t(uint16_t)>` 颜色映射回调。
3. **预生成多色版本**：为 6 种气质各生成一套精灵数据，但 Flash 占用 ×6。

> 推荐方案 2：修改 `drawRgb565Rle` 增加可选的颜色映射回调，开销最小。

---

## 6. 玩家视角速查表

| 看到 | 读出 |
|---|---|
| 深红，很浅 | 蛮力，STR 刚起步（ratio < 0.25） |
| 深红，发光 | 蛮力，STR 接近极限（ratio > 0.75） |
| 灰蓝，中等 | 迅捷，SPD 养到一半（ratio ~ 0.5） |
| 灰蓝，标准大小 | 迅捷，非巨体基因 |
| 橙褐，很大，发光 | 巨体，SIZ 接近极限且体型大 |
| 橙褐，很小，苍白 | 巨体，SIZ 很低（幼年或没养） |
| 金色，发光 | 韧甲，END 接近极限 |
| 白色，中等 | 均衡，全面发展中等 |
| 青绿，很浅 | 灵心，SPI 刚起步 |
| 青绿，发光 | 灵心，SPI 接近极限 |

---

## 7. 代码改动清单

### 7.1 `src/game/Bug.h`

| 新增 | 说明 |
|---|---|
| `float getAdultDepth() const` | 根据气质返回对应属性的 ratio |
| `float getAdultScale() const` | 仅巨体返回缩放倍率，其他返回 1.0f |

### 7.2 `src/hardware/PixelRenderer.h`

| 新增 | 说明 |
|---|---|
| `struct AdultPalette { uint16_t main, shadow; }` | 成虫调色板结构 |
| `static AdultPalette getAdultPalette(Temperament t, float ratio)` | 根据气质+ratio 生成调色板 |
| 修改 `drawRgb565Rle` | 增加可选颜色映射回调（如 `std::function<uint16_t(uint16_t)>`） |

### 7.3 `src/scenes/TerrariumScene.h`

| 修改 | 说明 |
|---|---|
| 删除 `static const uint16_t PALETTE[4][2]` | 旧调色板不再使用 |
| 无需新增 | 动态生成调色板，不存静态数组 |

### 7.4 `src/scenes/TerrariumScene.cpp`

| 修改 | 说明 |
|---|---|
| `drawAdult(int x, int y, uint8_t palette)` | 改为 `drawAdult(int x, int y)`，内部调用 `getAdultPalette()` 和 `getAdultScale()` |
| 删除 `PALETTE` 初始化 | 旧调色板数据删除 |
| 颜色替换逻辑 | 在 `drawRgb565Rle` 调用中传入颜色映射 |

### 7.5 数据文件

| 文件 | 说明 |
|---|---|
| `HerculesAdultSprites` | 需确认原始主色/副色 RGB565 值，作为替换基准 |

---

## 8. 待确认项

| # | 问题 | 影响 |
|---|---|---|
| 1 | `HerculesAdultSprites` 原始主色/副色确切值是什么？ | **已解析数据：成虫精灵中不存在 `#ff0000`（0xF800）**，实际使用棕/褐/橙/黑等 712 种颜色，属于多色渐变精灵。颜色替换需采用映射算法而非简单两色替换。 |
| 2 | 精灵是否有中间色（渐变）？ | **是**，共 712 种不同颜色，需运行时颜色映射。 |
| 3 | 大成档位的"发光"效果是否需要在绘制后叠加高光像素？ | **不需要**。 |
| 4 | 缩放后碰撞检测/边界检测是否需要调整？ | 最大放大已改为 **1.2x**；暂时不调整交互边界，后续纳入 TODO 验证。 |
| 5 | 缩放是否影响食物盘/腐木的交互距离判定？ | 巨体可能够不到食物盘 |

---

## 9. 版本历史

| 版本 | 日期 | 变更 |
|---|---|---|
| v1.0 | 2026-06-19 | 初版：气质色相 + 养成进度深浅 + 巨体体型缩放 + 颜色替换技术方案 |
| v1.1 | 2026-06-19 | 确认待实现项：原始精灵无 #ff0000，为多色渐变；取消发光；最大放大改为 1.2x；交互边界调整延后 |

---

> 本文档为 `doc/PokeBug-FoodSystem.md` §11.7 的详细实现版。设计决策以 FoodSystem.md 为准，本文档仅补充技术实现细节。
