# PNG 序列帧动画完整工作流程

## 📌 概述

本指南详细说明如何使用 Adobe After Effects 创建动画，并将其集成到您的 ESP32 双目机器人项目中。

## 🎬 第一步：在 After Effects 中创建动画

### 1.1 新建项目设置

```
分辨率: 240x240 像素
帧率: 12-15 FPS（推荐 12 FPS）
时长: 建议 3-5 秒（36-60 帧）
背景: 透明或纯色
```

### 1.2 动画设计建议

**表情动画示例：**
- **开心**: 眼睛变成弯月形，或者有星星闪烁
- **惊讶**: 眼睛放大，瞳孔收缩
- **伤心**: 眼睛变扁，可能有泪滴
- **思考**: 眼睛左右移动，或变成问号形状
- **生气**: 眉毛下垂，眼睛变红

**注意事项：**
- 保持简洁，避免过于复杂的动画
- 每个表情控制在 30-60 帧
- 考虑循环播放的流畅性（如待机动画）

### 1.3 导出 PNG 序列

在 After Effects 中：

1. **Composition** → **Add to Adobe Media Encoder Queue**
2. 在 Media Encoder 中设置：
   - **Format**: PNG
   - **Resolution**: 240x240
   - **Color Mode**: RGB (如果需要透明，选择 RGBA)
3. 选择输出文件夹，例如：`./animations/happy/`
4. 点击开始导出

**命名规范：**
```
happy_0000.png
happy_0001.png
happy_0002.png
...
happy_0059.png
```

## 🐍 第二步：转换 PNG 序列为 C 数组

### 2.1 安装依赖

```bash
pip install Pillow
```

### 2.2 运行转换脚本

```bash
# 基本用法
python tools/png_sequence_to_array.py animations/happy anim_happy

# 指定帧率
python tools/png_sequence_to_array.py animations/happy anim_happy --fps 15

# 指定分辨率（如果不是 240x240）
python tools/png_sequence_to_array.py animations/happy anim_happy --width 240 --height 240

# 指定输出路径
python tools/png_sequence_to_array.py animations/happy anim_happy -o main/display/animations/anim_happy.h
```

### 2.3 转换脚本参数说明

```
参数:
  folder        PNG 序列文件夹路径
  name          输出数组名称（如 "anim_happy"）

选项:
  --fps FPS     帧率（默认 12）
  --width W     图片宽度（默认 240）
  --height H    图片高度（默认 240）
  --rgb888      使用 RGB888 格式（默认 RGB565）
  --output,-o   输出文件路径
```

### 2.4 输出文件示例

转换后会生成类似以下内容的头文件：

```c
/**
 * 自动生成的动画数据
 *
 * - 帧数: 48
 * - 分辨率: 240x240
 * - 帧率: 12 FPS
 * - 总时长: 4.00 秒
 * - 数据大小: 450.0 KB
 * - 格式: RGB565
 */

#ifndef ANIM_ANIM_HAPPY_H
#define ANIM_ANIM_HAPPY_H

#include <stdint.h>

#define ANIM_ANIM_HAPPY_FRAME_COUNT     48
#define ANIM_ANIM_HAPPY_WIDTH           240
#define ANIM_ANIM_HAPPY_HEIGHT          240
#define ANIM_ANIM_HAPPY_FPS             12
#define ANIM_ANIM_HAPPY_DURATION_MS     4000

// 帧 0
const uint16_t anim_anim_happy_frame_0[57600] = {
    0x0021, 0x0000, 0x0001, ...
};

// 帧 1
const uint16_t anim_anim_happy_frame_1[57600] = {
    0x0022, 0x0001, 0x0002, ...
};

// ... 更多帧 ...

// 帧指针数组
const uint16_t* anim_anim_happy_frames[48] = {
    anim_anim_happy_frame_0,
    anim_anim_happy_frame_1,
    // ...
};

// 动画结构体
typedef struct {
    const uint16_t** frames;
    uint16_t frame_count;
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint32_t duration_ms;
} anim_anim_happy_t;

const anim_anim_happy_t anim_anim_happy = {
    .frames = anim_anim_happy_frames,
    .frame_count = 48,
    .width = 240,
    .height = 240,
    .fps = 12,
    .duration_ms = 4000,
};

#endif
```

## 💻 第三步：集成到 ESP32 项目

### 3.1 将生成的文件添加到项目

1. 将生成的 `.h` 文件移动到 `main/display/animations/` 目录
2. 更新 `main/display/CMakeLists.txt`：

```cmake
# 添加动画源文件
idf_component_register(SRCS "animation_player.c"
                       INCLUDE_DIRS "." "animations"
                       REQUIRES esp_timer driver)
```

### 3.2 在代码中使用动画

参考 `examples/animation_example.c` 中的示例：

```c
#include "animation_player.h"
#include "anim_happy.h"  // 包含你的动画数据

// 创建播放器实例
static AnimationPlayer anim_player;

// 初始化
void init() {
    AnimPlayConfig config = {
        .loop = false,
        .restart_on_end = true,
        .speed_multiplier = 128,  // 正常速度
    };

    anim_player_init(&anim_player, &anim_happy, &config);
}

// 播放动画
void play_happy_emotion() {
    anim_player_start(&anim_player);
}

// 在主循环中更新
void main_loop() {
    while(1) {
        const uint16_t* frame = anim_player_update(&anim_player);
        if (frame != NULL) {
            // 绘制到屏幕
            esp_lcd_safe_draw_bitmap(0, 0, 240, 240, frame);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 🔧 第四步：与现有系统集成

### 4.1 暂时停用正常眼睛更新

在播放动画时，暂停正常的眼睛渲染任务：

```c
// 播放动画前
vTaskSuspend(task_update_eye_handler);

// 播放动画
anim_player_start(&anim_player);

// 动画播放完成后
vTaskResume(task_update_eye_handler);
```

### 4.2 与语音系统集成

```c
// 语音回调中触发表情
void on_voice_response(const char* text) {
    if (strstr(text, "开心")) {
        play_emotion_animation(EMOTION_HAPPY);
    } else if (strstr(text, "惊讶")) {
        play_emotion_animation(EMOTION_SURPRISED);
    }
    // ... 更多表情
}
```

### 4.3 与 MQTT/WebSocket 集成

```c
// 通过 MQTT 触发表情
void on_mqtt_message(const char* topic, const char* payload) {
    if (strcmp(topic, "robot/emotion") == 0) {
        play_emotion_by_voice(payload);
    }
}
```

## 📊 第五步：优化和管理

### 5.1 Flash 空间管理

**估算方法：**
- 单帧（240x240 RGB565）= 115.2 KB
- 60 帧动画 ≈ 6.9 MB
- ESP32-S3 典型 Flash 大小：4-16 MB

**优化建议：**
1. 限制帧数（建议不超过 60 帧）
2. 降低帧率（10-12 FPS 足够）
3. 使用压缩（后续可以实现 RLE 压缩）
4. 只保留常用表情在 Flash，其他可通过 SPIFFS 加载

### 5.2 性能优化

```c
// 使用 DMA 传输加速
esp_lcd_panel_draw_bitmap(...);

// 双缓冲减少闪烁
#define GC9A01_LCD_DRAW_BUFF_DOUBLE (1)

// 批量渲染多行（已实现）
#define LINES_PER_BATCH 10
```

### 5.3 内存优化

如果 Flash 空间不足，可以考虑：

1. **减少表情数量**：只保留最常用的 3-5 个
2. **降低分辨率**：120x120 然后放大显示
3. **共享帧数据**：多个表情共享起始/结束帧
4. **使用文件系统**：将动画存储在 SPIFFS/FATFS，按需加载

## 🎨 表情设计建议

### 常用表情列表

1. **开心 (happy)**: 弯眼笑，星星眼睛
2. **惊讶 (surprised)**: 圆眼，瞳孔放大
3. **伤心 (sad)**: 垂眼，可能有泪滴
4. **生气 (angry)**: 怒眼，眉毛下垂
5. **思考 (thinking)**: 眼睛左右移动
6. **睡觉 (sleeping)**: 闭眼，"zzz" 符号
7. **待机 (idle)**: 轻微眨眼，瞳孔缓慢移动
8. **说话 (talking)**: 嘴巴或眼睛随语音节奏变化

### After Effects 动画技巧

1. **使用 Shape Layers**：矢量图形，放大不失真
2. **Ease 关键帧**：让动画更自然（F9）
3. **Loop Expression**：待机动画使用循环表达式
4. **Graph Editor**：精细调整动画曲线

## 🐛 常见问题

### Q1: 编译时出现 "Flash full" 错误
**A:** 减少动画帧数或表情数量，参考优化建议

### Q2: 动画播放卡顿
**A:** 降低帧率或减少每次渲染的行数

### Q3: PNG 转换失败
**A:** 确保 PNG 格式正确，安装 Pillow 库

### Q4: 动画和眼睛模式切换有闪烁
**A:** 在切换前清空屏幕，使用双缓冲

## 📚 参考资源

- [Adobe After Effects 教程](https://helpx.adobe.com/after-effects/tutorials.html)
- [ESP32 LCD 编程指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html)
- [Pillow 文档](https://pillow.readthedocs.io/)

## 🎯 快速开始示例

```bash
# 1. 在 After Effects 中创建动画并导出为 PNG 序列
# 2. 运行转换脚本
python tools/png_sequence_to_array.py animations/happy anim_happy --fps 12

# 3. 将生成的文件移动到项目目录
mv anim_anim_happy.h main/display/animations/

# 4. 在代码中使用（参考 examples/animation_example.c）

# 5. 编译并烧录
idf.py build flash monitor
```

---

**祝您创建出精彩的表情动画！** 🎉
