# 眼睛主题开发技术指南

## 概述

本文档详细记录了眼睛主题的制作、转换和集成方法，便于后续添加新主题或修改现有主题。

**目标受众**: 开发人员、贡献者
**最后更新**: 2025-12-29
**项目**: ESP32-S3 双目 AI 机器人 (xiaozhi-esp32 分支)

## 目录

1. [主题数据结构](#主题数据结构)
2. [转换工具使用](#转换工具使用)
3. [手动转换流程](#手动转换流程)
4. [集成到项目](#集成到项目)
5. [MCP 协议集成](#mcp-协议集成)
6. [调试技巧](#调试技巧)
7. [常见问题](#常见问题)

---

## 主题数据结构

### 尺寸规范

所有眼睛主题必须符合以下尺寸规范：

| 组件 | 标准尺寸 | 格式 | 说明 |
|------|---------|------|------|
| **Sclera (眼白)** | 375 × 375 | RGB565 | 眼睛的主体部分 |
| **Iris (虹膜)** | 256 × 64 | RGB565 | 极坐标映射的瞳孔纹理 |
| **Upper Eyelid** | 375 × 32 | RGB565 (带透明度) | 上眼睑 |
| **Lower Eyelid** | 375 × 32 | RGB565 (带透明度) | 下眼睑 |

### 数据格式

#### RGB565 格式说明

每个像素使用 16 位表示：
- **红**: 5 bits (0-31)
- **绿**: 6 bits (0-63)
- **蓝**: 5 bits (0-31)

十六进制示例: `0xF800` = 纯红 (11111 000000 00000)

#### 数组存储格式

```cpp
// 375x375 的 RGB565 数组示例
const uint16_t theme_sclera[] = {
    0xFFFF, 0xFFFF, 0xF800,  // 第1行前3个像素
    0xFFFF, 0xFFFF, 0xFFFF,  // 继续第1行
    // ... 共 375*375 = 140625 个元素
};
```

### Iris (虹膜) 特殊格式

虹膜使用极坐标映射，尺寸为 256×64：

- **X 轴 (256)**: 角度 (0-360°)
- **Y 轴 (64)**: 半径 (从瞳孔中心到边缘)

渲染时通过极坐标转换将 2D 纹理映射到圆形虹膜区域。

---

## 转换工具使用

### 自动转换脚本

项目提供了两个 Python 转换脚本：

#### 1. convert_theme_with_iris.py (推荐)

**功能**: 完整的主题转换，支持 Sclera 和 Iris 数据提取和转换

**使用方法**:
```bash
python convert_theme_with_iris.py <输入文件> <主题名称> <命名空间>
```

**示例**:
```bash
# 转换猫眼主题
python convert_theme_with_iris.py main/display/graphics/cat_eye.h cat CatTheme

# 转换默认眼主题
python convert_theme_with_iris.py main/display/graphics/default_eye.h default DefaultTheme
```

**脚本特性**:
- ✅ 自动检测原始尺寸 (从 #define 宏读取)
- ✅ 支持 0x 和 0X 两种十六进制格式
- ✅ 自动检测 Iris 尺寸 (IRIS_MAP_WIDTH/HEIGHT)
- ✅ 最近邻插值缩放到 375×375
- ✅ 处理占位符 Iris (1×1 数据)
- ✅ 生成命名空间隔离的 .h 和 .cc 文件
- ✅ 添加 IRIS_MIN/IRIS_MAX 配置

**输出文件**:
- `main/display/<theme_name>_theme_data.h` - 头文件
- `main/display/<theme_name>_theme_data.cc` - 实现文件

#### 2. convert_one_theme.py (简化版)

**功能**: 仅转换 Sclera 数据，Iris 使用星空主题

**使用方法**:
```bash
python convert_one_theme.py <输入文件> <主题名称>
```

**适用场景**: 源文件没有独立的 Iris 数据，或计划复用现有 Iris

### 脚本工作原理

#### 尺寸检测

```python
# 从源文件中提取尺寸定义
width_match = re.search(r'#define\s+SCLERA_WIDTH\s+(\d+)', content)
height_match = re.search(r'#define\s+SCLERA_HEIGHT\s+(\d+)', content)

src_w = int(width_match.group(1))
src_h = int(height_match.group(1))
```

#### 数据提取

```python
# 支持两种十六进制格式
# 0xFFFF 或 0XFFFF
values = re.findall(r'0[xX][0-9A-Fa-f]+', values_str)
data = [int(v, 16) for v in values]
```

#### 最近邻插值算法

```python
def resize_sclera(sclera_data, src_w, src_h, dst_w=375, dst_h=375):
    """使用最近邻插值缩放图像"""
    x_ratio = src_w / dst_w
    y_ratio = src_h / dst_h

    result = []
    for y in range(dst_h):
        for x in range(dst_w):
            # 计算源坐标
            src_x = min(int(x * x_ratio), src_w - 1)
            src_y = min(int(y * y_ratio), src_h - 1)
            # 复制最近像素
            result.append(sclera_data[src_y * src_w + src_x])

    return result
```

**为什么使用最近邻插值?**
- 保留像素艺术风格
- 避免模糊和抗锯齿
- 保持清晰的边缘
- 适合低分辨率图像放大

---

## 手动转换流程

如果自动脚本无法使用，可以手动转换主题：

### 步骤 1: 准备源文件

获取原始眼睛图像数据，可以是：
- 现有的 .h 文件
- PNG/JPG 图像 (需要使用工具转换)
- 其他格式的数组数据

### 步骤 2: 分析原始尺寸

查看源文件中的尺寸定义：
```cpp
#define SCLERA_WIDTH 180
#define SCLERA_HEIGHT 180
#define IRIS_MAP_WIDTH 402
#define IRIS_MAP_HEIGHT 64
```

### 步骤 3: 转换 Sclera 到 375×375

使用图像处理工具 (Python/PIL):

```python
from PIL import Image
import numpy as np

# 加载或创建图像
# (假设已从 RGB565 转换为 RGB888)
img = Image.fromarray(rgb_array)

# 调整大小 (最近邻插值)
img_resized = img.resize((375, 375), Image.NEAREST)

# 转换回 RGB565
def rgb_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

# 生成 C 数组
array_data = []
for y in range(375):
    for x in range(375):
        pixel = img_resized.getpixel((x, y))
        rgb565 = rgb_to_rgb565(*pixel)
        array_data.append(f"0x{rgb565:04X}")
```

### 步骤 4: 转换 Iris 到 256×64 (如有)

如果源文件有独立的 Iris 数据：

```python
# 调整 Iris 大小
iris_resized = iris_img.resize((256, 64), Image.NEAREST)

# 生成极坐标映射数组
iris_array = []
for y in range(64):
    for x in range(256):
        pixel = iris_resized.getpixel((x, y))
        rgb565 = rgb_to_rgb565(*pixel)
        iris_array.append(f"0x{rgb565:04X}")
```

### 步骤 5: 生成命名空间文件

创建头文件 (`theme_name_theme_data.h`):

```cpp
#ifndef MAIN_DISPLAY_THEME_NAME_THEME_DATA_H_
#define MAIN_DISPLAY_THEME_NAME_THEME_DATA_H_

#include <stdint.h>

namespace ThemeName {

// Sclera 数据 (375x375 RGB565)
extern const uint16_t theme_name_sclera[];

// Iris 数据 (256x64 RGB565) - 可选
extern const uint16_t theme_name_iris[];

// 尺寸配置
static const int SCLERA_WIDTH = 375;
static const int SCLERA_HEIGHT = 375;

// Iris 缩放范围
static const int IRIS_MIN = 180;  // 最小瞳孔
static const int IRIS_MAX = 280;  // 最大瞳孔

}  // namespace ThemeName

#endif  // MAIN_DISPLAY_THEME_NAME_THEME_DATA_H_
```

创建实现文件 (`theme_name_theme_data.cc`):

```cpp
#include "theme_name_theme_data.h"

namespace ThemeName {

// Sclera 数据
const uint16_t theme_name_sclera[] = {
    0xFFFF, 0xFFFF, 0xF800,  // 第1行
    // ... 共 140625 个元素
};

// Iris 数据 - 可选
const uint16_t theme_name_iris[] = {
    0x0000, 0x0000, 0x0000,  // 第1行
    // ... 共 16384 个元素
};

}  // namespace ThemeName
```

### 步骤 6: 验证数据完整性

```bash
# 检查数组大小
python3 -c "
data = open('theme_name_theme_data.cc').read()
values = data.count('0x')
print(f'Sclera pixels: {values}')
print(f'Expected: {375*375} = 140625')
print(f'Match: {values == 140625}')
"
```

---

## 集成到项目

### 1. 更新 CMakeLists.txt

编辑 [main/CMakeLists.txt](../main/CMakeLists.txt):

```cmake
# 在 list(APPEND SRCS "display/eye_dragon_data.cc") 后添加
list(APPEND SRCS "display/theme_name_theme_data.cc")
```

### 2. 更新 eye_themes.h

编辑 [main/display/eye_themes.h](../main/display/eye_themes.h):

```cpp
typedef enum {
    // ... 现有主题
    EYE_THEME_THEME_NAME = 13,  // 新主题枚举
} EyeTheme;
```

### 3. 更新 eye_themes.cc

编辑 [main/display/eye_themes.cc](../main/display/eye_themes.cc):

```cpp
#include "theme_name_theme_data.h"  // 添加头文件

// 在 eye_themes[] 数组中添加
{
    .name = "theme_name",
    .sclera = ThemeName::theme_name_sclera,
    .iris = ThemeName::theme_name_iris,  // 或 iris_xingkong
    .width = 375,
    .height = 375,
    .iris_min = ThemeName::IRIS_MIN,
    .iris_max = ThemeName::IRIS_MAX
},
```

**Iris 策略选择**:

| 情况 | 使用的 Iris | 说明 |
|------|-------------|------|
| 源文件有完整 Iris | `ThemeName::theme_name_iris` | 使用主题自己的 Iris |
| 源文件 Iris 为 1×1 | `iris_xingkong` | 复用星空虹膜 (梦幻效果) |
| 源文件无 Iris | `iris_xingkong` | 复用星空虹膜 |

### 4. 编译验证

```bash
# 清理并重新编译
idf.py fullclean
idf.py build

# 检查固件大小
idf.py size
```

**预期固件大小** (13 个主题):
- Flash 使用: ~8 MB
- RAM 使用: ~500 KB (运行时)

---

## MCP 协议集成

### 添加主题切换工具

编辑 [main/mcp_server.cc](../main/mcp_server.cc):

#### 1. 更新工具描述

在 `self.eye.set_theme` 工具的描述中添加新主题：

```cpp
AddTool("self.eye.set_theme",
    "Change the eye theme style...\n"
    "Available themes:\n"
    "  - 'theme_name': Theme description here\n"
    // ... 其他主题
```

#### 2. 添加主题处理逻辑

在 switch 语句中添加：

```cpp
} else if (theme == "theme_name") {
    theme_id = EYE_THEME_THEME_NAME;
```

#### 3. 更新可用主题列表

在错误消息中添加新主题：

```cpp
"Available themes: xingkong, shuimu, keji, dragon, cat, default, "
"doe, goat, nauga, newt, noSclera, owl, terminator, theme_name"
```

### 测试主题切换

#### 方法 1: 使用 JSON-RPC

```bash
# 通过 WebSocket 或 MQTT 发送
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.eye.set_theme",
    "arguments": {
      "theme": "theme_name"
    }
  },
  "id": 1
}
```

#### 方法 2: 语音指令

配置语音识别，用户可以说：
- "换成 theme_name 眼睛"
- "切换到 theme_name 风格"

---

## 调试技巧

### 检查主题数据

#### 1. 验证尺寸

```cpp
// 在 eye_display.cc 中添加调试代码
ESP_LOGI(TAG, "Theme sclera size: %d x %d",
    current_theme->width,
    current_theme->height);
```

#### 2. 可视化 RGB565 数据

使用 Python 脚本将 RGB565 数组转换为 PNG：

```python
def rgb565_to_rgb(value):
    r = ((value >> 11) & 0x1F) << 3
    g = ((value >> 5) & 0x3F) << 2
    b = (value & 0x1F) << 3
    return (r, g, b)

def save_rgb565_as_png(data_array, width, height, filename):
    from PIL import Image
    img = Image.new('RGB', (width, height))
    pixels = img.load()

    for y in range(height):
        for x in range(width):
            idx = y * width + x
            rgb = rgb565_to_rgb(data_array[idx])
            pixels[x, y] = rgb

    img.save(filename)
```

### 常见编译错误

#### 错误 1: 多重定义

```
multiple definition of "sclera"
```

**原因**: 两个不同的主题文件使用了相同的全局变量名

**解决**: 使用命名空间隔离：
```cpp
namespace ThemeName {
    const uint16_t sclera[] = { ... };  // ✅ 正确
}
```

#### 错误 2: 格式说明符不匹配

```
format '%d' expects argument of type 'int', but argument has type 'uint32_t'
```

**解决**: 使用 PRI 宏：
```cpp
#include <inttypes.h>

ESP_LOGI(TAG, "Value: %" PRIu32, uint32_value);
ESP_LOGI(TAG, "Value: %" PRId32, int32_value);
```

#### 错误 3: Flash 空间不足

```
Partitions tables occupies 26.0MB of flash which does not fit in configured flash size 16MB
```

**解决**: 使用 16MB 无 OTA 分区表：
1. 创建 `partitions/v1/16m_no_ota.csv`
2. 更新 `sdkconfig`:
   ```
   CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v1/16m_no_ota.csv"
   ```
3. 重新编译

### 性能优化

#### 优化内存使用

如果固件过大，考虑：

1. **移除不常用主题**: 保留核心主题，其他按需加载
2. **压缩 Iris 数据**: 使用 RLE 或 LZ4 压缩
3. **共享 Iris**: 多个主题复用同一 Iris 数据

#### 优化渲染性能

在 [eye_display.cc](../main/display/eye_display.cc) 中：

```cpp
// 调整批处理行数
#define LINES_PER_BATCH 10  // 增加到 20 可提高性能

// 使用 DMA 传输
#define USE_DMA_TRANSFER    // 启用 DMA (如果硬件支持)
```

---

## 常见问题

### Q1: 为什么主题尺寸必须是 375×375?

**A**: 这是硬件和软件架构的设计决定：

1. **硬件限制**: GC9A01 LCD 为 240×240，375×375 提供足够的缩放空间
2. **渲染效率**: 统一尺寸简化渲染逻辑
3. **质量保证**: 较大源尺寸确保缩放后质量
4. **兼容性**: 所有主题使用相同尺寸，便于切换

### Q2: 如何判断主题应该使用自己的 Iris 还是星空 Iris?

**A**: 检查源文件的 Iris 数据：

| Iris 数据 | 决策 |
|----------|------|
| 完整的 256×64 或更大 | 使用自己的 Iris |
| 1×1 占位符 | 使用星空 Iris |
| 不存在 | 使用星空 Iris |

**检查方法**:
```bash
# 查看源文件
grep -A 5 "iris" source_file.h

# 如果看到:
# #define IRIS_MAP_WIDTH 1
# #define IRIS_MAP_HEIGHT 1
# 则为占位符，使用星空 Iris
```

### Q3: 为什么使用最近邻插值而不是双线性插值?

**A**: 最近邻插值的优势：

1. **像素风格**: 保留原始像素艺术风格
2. **清晰边缘**: 避免模糊，保持细节锐利
3. **性能**: 算法简单，转换速度快
4. **一致性**: 与现有渲染管线匹配

对于眼睛这类像素艺术，最近邻插值效果更好。

### Q4: 如何添加自定义 Iris 动画效果?

**A**: 扩展 Iris 数据结构：

```cpp
// 在 eye_display.cc 中
struct IrisAnimation {
    const uint16_t* frames[10];  // 动画帧
    int frame_count;
    int frame_duration_ms;
};

// 使用多帧 Iris 替代单帧
void drawEyeWithAnimation(int e, const IrisAnimation* anim) {
    static int current_frame = 0;
    // 渲染当前帧
    drawIris(anim->frames[current_frame]);
    // 更新帧
    if (millis() % anim->frame_duration_ms == 0) {
        current_frame = (current_frame + 1) % anim->frame_count;
    }
}
```

### Q5: 主题切换时如何避免闪烁?

**A**: 实现双缓冲：

```cpp
// 在 eye_display.cc 中
uint16_t display_buffer[2][240 * 240];  // 双缓冲
int current_buffer = 0;

void switchThemeSmoothly(EyeTheme new_theme) {
    // 在后台缓冲区渲染新主题
    renderToBuffer(display_buffer[1-current_buffer], new_theme);

    // 原子切换显示缓冲区
    xSemaphoreTake(lcd_mutex, portMAX_DELAY);
    swapDisplayBuffer();
    xSemaphoreGive(lcd_mutex);
}
```

### Q6: 如何减少固件大小?

**A**: 多种优化策略：

1. **移除未使用主题**:
   ```cmake
   # 在 CMakeLists.txt 中注释掉不需要的主题
   # list(APPEND SRCS "display/unused_theme.cc")
   ```

2. **优化 RGB565 数据**:
   ```cpp
   // 使用 Flash 存储而不是 RAM
   const __attribute__((section(".rodata"))) uint16_t theme_sclera[] = { ... };
   ```

3. **启用链接时优化**:
   ```
   # 在 sdkconfig 中
   CONFIG_COMPILER_OPTIMIZATION_SIZE=y
   ```

4. **压缩 Iris 数据**:
   - 使用 RLE (Run-Length Encoding)
   - 或 LZ4 快速压缩

### Q7: 如何支持更高分辨率的显示?

**A**: 多层适配方案：

```cpp
// 在 eye_themes.h 中
#define DISPLAY_RESOLUTION_LOW    240   // 当前
#define DISPLAY_RESOLUTION_HIGH   480   // 未来

#if DISPLAY_RESOLUTION == DISPLAY_RESOLUTION_HIGH
    #define SCLERA_SIZE 750
    #define IRIS_WIDTH  512
    #define IRIS_HEIGHT 128
#else
    #define SCLERA_SIZE 375
    #define IRIS_WIDTH  256
    #define IRIS_HEIGHT 64
#endif
```

---

## 完整示例

### 示例: 添加"火焰眼"主题

假设我们要添加一个新的"火焰眼"主题：

#### 1. 准备源文件

假设源文件为 `flame_eye.h`:
```cpp
#define FLAME_SCLERA_WIDTH 200
#define FLAME_SCLERA_HEIGHT 200
const uint16_t flame_sclera[] = { 0xF800, 0xFFFF, ... };
// 无独立 Iris 数据
```

#### 2. 运行转换脚本

```bash
python convert_theme_with_iris.py \
    main/display/graphics/flame_eye.h \
    flame \
    FlameTheme
```

#### 3. 检查生成文件

生成的文件：
- [main/display/flame_theme_data.h](../main/display/flame_theme_data.h)
- [main/display/flame_theme_data.cc](../main/display/flame_theme_data.cc)

#### 4. 更新 CMakeLists.txt

```cmake
list(APPEND SRCS "display/flame_theme_data.cc")
```

#### 5. 更新 eye_themes.h

```cpp
typedef enum {
    // ... 现有主题
    EYE_THEME_FLAME = 13,
} EyeTheme;
```

#### 6. 更新 eye_themes.cc

```cpp
#include "flame_theme_data.h"

{
    .name = "flame",
    .sclera = FlameTheme::flame_sclera,
    .iris = iris_xingkong,  // 使用星空虹膜
    .width = 375,
    .height = 375,
    .iris_min = FlameTheme::IRIS_MIN,
    .iris_max = FlameTheme::IRIS_MAX
},
```

#### 7. 更新 mcp_server.cc

```cpp
} else if (theme == "flame") {
    theme_id = EYE_THEME_FLAME;
```

#### 8. 编译测试

```bash
idf.py build
idf.py flash monitor
```

#### 9. 测试切换

发送 MCP 请求：
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.eye.set_theme",
    "arguments": {
      "theme": "flame"
    }
  },
  "id": 1
}
```

---

## 附录

### A. 当前项目主题清单

| 枚举值 | 主题名 | 文件 | Iris 来源 | 状态 |
|--------|--------|------|----------|------|
| 0 | xingkong | eyes_data.h | iris_xingkong | ✅ |
| 1 | shuimu | eyes_data.h | iris_xingkong | ✅ |
| 2 | keji | eyes_data.h | iris_xingkong | ✅ |
| 3 | dragon | dragon_theme_data.cc | iris_xingkong | ✅ |
| 4 | cat | cat_theme_data.cc | iris_xingkong | ✅ |
| 5 | default | default_theme_data.cc | default_iris | ✅ |
| 6 | doe | doe_theme_data.cc | doe_iris | ✅ |
| 7 | goat | goat_theme_data.cc | goat_iris | ✅ |
| 8 | nauga | nauga_theme_data.cc | iris_xingkong | ✅ |
| 9 | newt | newt_theme_data.cc | newt_iris | ✅ |
| 10 | noSclera | noSclera_theme_data.cc | noSclera_iris | ✅ |
| 11 | owl | owl_theme_data.cc | iris_xingkong | ✅ |
| 12 | terminator | terminator_theme_data.cc | terminator_iris | ✅ |

**总计**: 13 个主题

### B. 分区表对比

| 分区表 | OTA | App 大小 | 总 Flash | 适用场景 |
|--------|-----|---------|---------|---------|
| 32m.csv | ✅ | 12MB | 32MB | 开发测试 |
| 16m.csv | ✅ | 6MB | 16MB | ❌ 固件太大 |
| 16m_no_ota.csv | ❌ | 14MB | 16MB | ✅ 生产部署 |

### C. 工具和依赖

**Python 脚本依赖**:
```txt
 Pillow>=10.0.0
 numpy>=1.24.0
```

**ESP-IDF 版本**: v5.4.1

**编译器**: xtensa-esp32s3-elf-gcc

### D. 参考资源

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/)
- [GC9A01 LCD 驱动文档](https://github.com/Xinyuan-LilyGO/T-Display-S3)
- [RGB565 格式说明](https://en.wikipedia.org/wiki/RGB565)
- [最近邻插值算法](https://en.wikipedia.org/wiki/Nearest-neighbor_interpolation)

---

**文档版本**: 1.0
**最后更新**: 2025-12-29
**维护者**: Claude Code & 社区贡献者

**版权**: 本文档遵循与项目相同的开源许可证。
