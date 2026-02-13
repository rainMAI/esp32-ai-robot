# PNG 序列帧动画系统

## 🎬 简介

这是一个为 ESP32-S3 双目机器人设计的完整动画系统，允许您使用 Adobe After Effects 创建丰富的表情动画，并将其集成到机器人项目中。

### ✨ 主要特性

- ✅ **支持 PNG 序列帧动画** - 从 AE 直接导出，无需手动绘制
- ✅ **自动转换为 C 数组** - Python 脚本一键转换
- ✅ **高性能播放器** - 优化的渲染管线，支持 12+ FPS
- ✅ **灵活的播放控制** - 播放、暂停、循环、速度调整
- ✅ **完整集成示例** - 与现有眼睛系统无缝切换
- ✅ **详细的文档** - 从 AE 制作到 ESP32 集成的完整流程

## 📁 文件结构

```
eyes/
├── tools/
│   └── png_sequence_to_array.py    # PNG 转 C 数组工具
├── main/display/
│   ├── animation_player.h           # 播放器头文件
│   ├── animation_player.c           # 播放器实现
│   └── animations/                  # 动画数据文件夹（创建）
│       ├── anim_happy.h
│       └── anim_surprised.h
├── examples/
│   └── animation_example.c          # 使用示例
└── docs/
    ├── animation_quickstart.md      # 5分钟快速开始
    ├── animation_workflow.md        # 完整工作流程
    └── after_effects_template.md    # AE 模板指南
```

## 🚀 快速开始

### 1. 安装依赖

```bash
pip install Pillow
```

### 2. 在 After Effects 中创建动画

- **分辨率**: 240x240 像素
- **帧率**: 12 FPS
- **时长**: 3-5 秒
- **导出**: PNG 序列

详见：[after_effects_template.md](after_effects_template.md)

### 3. 转换 PNG 为 C 数组

```bash
python tools/png_sequence_to_array.py animations/happy anim_happy
```

### 4. 集成到代码

```c
#include "anim_happy.h"
#include "animation_player.h"

static AnimationPlayer player;

void play_happy() {
    anim_player_init(&player, &anim_happy, NULL);
    anim_player_start(&player);
}

// 在主循环中
const uint16_t* frame = anim_player_update(&player);
if (frame != NULL) {
    esp_lcd_safe_draw_bitmap(0, 0, 240, 240, frame);
}
```

详见：[animation_example.c](../examples/animation_example.c)

### 5. 编译并烧录

```bash
idf.py build flash monitor
```

## 📚 文档

| 文档 | 描述 |
|------|------|
| [animation_quickstart.md](animation_quickstart.md) | 5 分钟快速入门指南 |
| [animation_workflow.md](animation_workflow.md) | 完整工作流程和优化建议 |
| [after_effects_template.md](after_effects_template.md) | AE 项目模板和动画技巧 |

## 🎨 表情示例

### 推荐的表情列表

1. **Happy（开心）** - 弯眼笑，星星闪烁
2. **Surprised（惊讶）** - 眼睛放大，瞳孔收缩
3. **Sad（伤心）** - 垂眼，泪滴效果
4. **Angry（生气）** - 眉毛下垂，眼睛变红
5. **Thinking（思考）** - 眼睛左右移动
6. **Idle（待机）** - 轻微眨眼，瞳孔移动（循环）

### 创建表情的步骤

1. 在 AE 中创建 240x240 合成
2. 使用形状工具绘制眼睛
3. 添加关键帧动画
4. 导出为 PNG 序列
5. 使用 Python 脚本转换
6. 集成到 ESP32 项目

## 📊 性能和存储

### 存储空间计算

```
单帧大小（RGB565）= 240 × 240 × 2 = 115.2 KB

总大小 = 单帧大小 × 帧数

示例：
- 30 帧 ≈ 3.4 MB
- 60 帧 ≈ 6.9 MB
```

### 性能优化建议

- ✅ 使用 10-12 FPS（足够流畅）
- ✅ 限制帧数在 30-60 帧
- ✅ 使用 RGB565 格式（而非 RGB888）
- ✅ 启用双缓冲（已配置）
- ✅ 批量渲染（LINES_PER_BATCH=10）

### 内存管理

如果 Flash 空间不足：
1. 减少表情数量
2. 降低帧率或分辨率
3. 使用 SPIFFS 按需加载（未来功能）
4. 实现 RLE 压缩（未来功能）

## 🔧 高级功能

### 循环播放

```c
AnimPlayConfig config = {
    .loop = true,  // 启用循环
    .restart_on_end = false,
    .speed_multiplier = 128,
};
```

### 调整播放速度

```c
config.speed_multiplier = 64;   // 0.5x 慢速
config.speed_multiplier = 128;  // 1.0x 正常
config.speed_multiplier = 256;  // 2.0x 快速
```

### 播放完成回调

```c
void on_complete() {
    ESP_LOGI(TAG, "动画完成！");
    // 恢复正常眼睛模式
}

anim_player_set_callback(&player, on_complete);
```

### 与现有系统集成

暂停正常的眼睛渲染任务：

```c
// 播放动画前
vTaskSuspend(task_update_eye_handler);
anim_player_start(&player);

// 动画完成后
vTaskResume(task_update_eye_handler);
```

## 🎯 使用场景

### 1. 语音交互

```c
void on_voice_response(const char* text) {
    if (strstr(text, "开心")) {
        play_emotion(EMOTION_HAPPY);
    }
}
```

### 2. MQTT/WebSocket 控制

```c
void on_mqtt_message(const char* topic, const char* payload) {
    if (strcmp(topic, "robot/emotion") == 0) {
        play_emotion_by_name(payload);
    }
}
```

### 3. 待机动画

```c
// 在无交互时循环播放待机动画
void start_idle_mode() {
    AnimPlayConfig config = { .loop = true };
    anim_player_init(&player, &anim_idle, &config);
    anim_player_start(&player);
}
```

## 🐛 常见问题

### Q: 编译时出现 "Flash full" 错误
**A:** 减少动画帧数或表情数量，参考性能优化建议

### Q: 动画播放卡顿
**A:** 降低帧率到 10-12 FPS，或减少 LINES_PER_BATCH

### Q: PNG 转换失败
**A:** 确保安装了 Pillow：`pip install Pillow`

### Q: 动画和眼睛模式切换有闪烁
**A:** 使用双缓冲，在切换前清空屏幕

## 🔮 未来改进

- [ ] RLE 压缩支持（减少 50%+ 存储空间）
- [ ] SPIFFS 按需加载（支持更多表情）
- [ ] Alpha 通道支持（透明背景）
- [ ] 动画过渡效果（淡入淡出）
- [ ] 动画编辑器 GUI 工具

## 📞 获取帮助

1. 查看 [完整文档](animation_workflow.md)
2. 参考 [使用示例](../examples/animation_example.c)
3. 查看现有代码中的眼睛渲染实现

## 📄 许可证

本项目遵循 Apache-2.0 许可证。

## 🙏 致谢

- 原始项目：[xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
- Adobe After Effects 文档和教程

---

**开始创建你的第一个表情动画吧！** 🎉✨
