# 表情切换使用指南

## 🎭 功能概述

已成功实现多表情动画系统，支持通过 AI 对话切换不同的眼睛动画表情。

### 支持的表情

| 表情名称 | 分辨率 | 帧数 | 帧率 | 时长 | 文件大小 |
|---------|-------|------|------|------|---------|
| **eye** | 240x240 | 16 帧 | 8 FPS | 2 秒 | 6.5 MB |
| **grok** | 240x240 | 16 帧 | 8 FPS | 2 秒 | 6.5 MB |

---

## 🗣️ 通过 AI 对话切换

### 方法 1：自然语言命令

直接对 AI 说：

```
"切换到眼睛动画"
"播放 Grok 动画"
"换成 Grok 表情"
"播放 Grok 动画并循环"
"切换到默认眼睛动画"
```

### 方法 2：详细控制命令

**切换到眼睛动画并循环播放：**
```
"Switch to eye animation and loop"
```

**播放 Grok 动画一次（不循环）：**
```
"Play Grok animation once"
```

**切换到 Grok 动画并循环：**
```
"Change to Grok and loop"
```

---

## 💻 在代码中切换

### C/C++ 代码

```c
#include "display/eye_display.h"

// 切换到眼睛动画并循环播放
switch_expression("eye", true);

// 切换到 Grok 动画并循环播放
switch_expression("grok", true);

// 播放 Grok 动画一次（不循环）
switch_expression("grok", false);

// 获取当前表情名称
const char* current = get_current_expression_name();
printf("当前表情: %s\n", current);
```

### 示例：定时切换表情

```c
// 每 5 秒切换一次表情
void expression_timer_example() {
    while (1) {
        switch_expression("eye", true);
        vTaskDelay(pdMS_TO_TICKS(5000));

        switch_expression("grok", true);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

---

## 🔧 MCP 工具接口

### 工具名称
`self.eye.set_expression`

### 参数
- `expression` (字符串): 表情名称
  - `"eye"`: 眼睛动画
  - `"grok"`: Grok 动画
- `loop` (布尔值): 是否循环播放
  - `true`: 循环播放
  - `false`: 播放一次

### 返回值
**成功：**
```json
{
  "success": true,
  "message": "Expression changed to eye",
  "expression": "eye",
  "loop": "true"
}
```

**失败：**
```json
{
  "success": false,
  "message": "Failed to change expression to xxx. Available expressions: eye, grok"
}
```

---

## 📝 技术细节

### 动画数据存储
- **位置**: `main/display/animations/`
- **文件**:
  - `anim_eye.h` (6.5 MB)
  - `anim_grok.h` (6.5 MB)
- **格式**: RGB565 C 数组
- **总大小**: 13 MB

### 内存使用
- **Flash 存储**: 13 MB（两个动画文件）
- **运行时 RAM**: ~5 KB（单个 10 行缓冲区）
- **不会同时加载两个动画到 RAM**

### 分批传输
- 每次传输 10 行（`LINES_PER_BATCH = 10`）
- 缓冲区大小：10 × 240 × 2 = 4.8 KB
- 避免 SPI 超时和缓冲区溢出

---

## 🎨 添加新表情

### 1. 准备视频文件

使用 AI 工具生成视频（如 Grok、Runway 等），导出为 MP4。

### 2. 提取 PNG 帧

```bash
# 创建输出目录
mkdir new_frames

# 提取帧（240x240, 8 FPS, 2 秒）
ffmpeg -i input.mp4 -vf "fps=8,scale=240:240" -t 2 new_frames/new_%05d.png
```

### 3. 转换为 C 数组

```bash
python tools/png_to_array_optimized.py new_frames new --width 240 --height 240 --fps 8
```

### 4. 修改代码

**在 `multi_animation_manager.h` 中添加新表情类型：**
```c
typedef enum {
    EXPRESSION_DEFAULT = 0,
    EXPRESSION_EYE = 1,
    EXPRESSION_GROK = 2,
    EXPRESSION_NEW = 3,      // 新增
    EXPRESSION_MAX
} expression_type_t;
```

**在 `multi_animation_manager.c` 中添加动画元数据：**
```c
static const animation_metadata_t animations[] = {
    [EXPRESSION_EYE] = { ... },
    [EXPRESSION_GROK] = { ... },
    [EXPRESSION_NEW] = {       // 新增
        .frames = anim_new.frames,
        .frame_count = anim_new.frame_count,
        .width = anim_new.width,
        .height = anim_new.height,
        .fps = anim_new.fps,
        .duration_ms = anim_new.duration_ms,
        .name = "New"
    }
};
```

**在 `eye_display.cc` 的 `switch_expression` 函数中添加：**
```c
if (strcmp(expression_name, "new") == 0) {
    expression = EXPRESSION_NEW;
}
```

### 5. 更新 MCP 工具描述

在 `mcp_server.cc` 中更新工具描述，添加新表情说明。

---

## ❓ 常见问题

### Q: 如何知道当前播放的是哪个表情？
A: 使用 `get_current_expression_name()` 函数获取当前表情名称。

### Q: 动画可以同时播放多个吗？
A: 不可以。同一时间只能播放一个动画表情。

### Q: 如何停止动画？
A: 调用 `multi_anim_stop()` 函数。

### Q: 动画切换需要多长时间？
A: 几乎即时切换（< 50ms）。

### Q: 文件太大了怎么办？
A: 可以减少帧数或降低分辨率：
- 减少帧数：`-vframes 12`（12 帧而不是 16）
- 降低分辨率：`scale=200:200`（200x200 而不是 240x240）

### Q: 支持 GIF 格式吗？
A: 不直接支持。需要先用 FFmpeg 将 GIF 转为 PNG 序列。

---

## 🔗 相关文档

- [动画系统完整指南](animation_system_complete_guide.md) - MP4 转换详细流程
- [动画优化总结](animation_fix_summary.md) - v0.2.1 版本优化记录
- [MCP 协议文档](mcp-usage.md) - MCP 协议用于物联网控制

---

**版本**: v0.3.0
**更新时间**: 2025-01-05
**作者**: Claude Code
