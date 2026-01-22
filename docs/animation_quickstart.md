# PNG 序列帧动画 - 快速开始

## 🎯 5 分钟快速入门

### 步骤 1：准备动画（2 分钟）

在 Adobe After Effects 中：
1. 创建 240x240 像素的合成
2. 设计你的表情动画（3-5 秒）
3. 导出为 PNG 序列（File → Export → Add to Media Encoder Queue）

**导出设置：**
```
格式: PNG
分辨率: 240x240
帧率: 12 FPS
色彩: RGB
```

### 步骤 2：转换为 C 数组（1 分钟）

```bash
# 安装依赖（只需执行一次）
pip install Pillow

# 转换 PNG 序列
python tools/png_sequence_to_array.py <PNG文件夹路径> <动画名称>

# 示例：
python tools/png_sequence_to_array.py ./animations/happy anim_happy
```

### 步骤 3：集成到代码（2 分钟）

```c
// 1. 包含生成的头文件
#include "anim_happy.h"

// 2. 创建播放器
AnimationPlayer player;

// 3. 初始化并播放
void play_happy() {
    AnimPlayConfig config = {
        .loop = false,
        .restart_on_end = true,
        .speed_multiplier = 128,  // 正常速度
    };

    anim_player_init(&player, &anim_happy, &config);
    anim_player_start(&player);
}

// 4. 在主循环中更新
void main_loop() {
    const uint16_t* frame = anim_player_update(&player);
    if (frame != NULL) {
        esp_lcd_safe_draw_bitmap(0, 0, 240, 240, frame);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

### 步骤 4：编译并烧录

```bash
idf.py build flash monitor
```

## 📋 完整示例：语音触发表情

```c
#include "animation_player.h"
#include "anim_happy.h"
#include "anim_surprised.h"

static AnimationPlayer player;

// 播放表情
void play_emotion(const char* emotion) {
    if (strcmp(emotion, "happy") == 0) {
        anim_player_init(&player, &anim_happy, NULL);
    } else if (strcmp(emotion, "surprised") == 0) {
        anim_player_init(&player, &anim_surprised, NULL);
    }

    anim_player_start(&player);
}

// 语音回调
void on_voice_command(const char* command) {
    if (strstr(command, "开心")) {
        play_emotion("happy");
    } else if (strstr(command, "惊讶")) {
        play_emotion("surprised");
    }
}
```

## 🔧 高级配置

### 循环播放（待机动画）

```c
AnimPlayConfig config = {
    .loop = true,  // 启用循环
    .restart_on_end = false,
    .speed_multiplier = 128,
};
anim_player_init(&player, &anim_idle, &config);
anim_player_start(&player);  // 会一直循环播放
```

### 调整播放速度

```c
config.speed_multiplier = 64;   // 0.5x 慢速
config.speed_multiplier = 128;  // 1.0x 正常
config.speed_multiplier = 256;  // 2.0x 快速
```

### 播放完成回调

```c
void on_anim_complete() {
    ESP_LOGI(TAG, "动画播放完成！");
    // 返回正常眼睛模式
}

anim_player_set_callback(&player, on_anim_complete);
```

## 📊 存储空间参考

| 帧数 | 分辨率 | 格式 | 大小 |
|------|--------|------|------|
| 30   | 240x240| RGB565 | ~3.4 MB |
| 60   | 240x240| RGB565 | ~6.9 MB |
| 30   | 120x120| RGB565 | ~0.9 MB |

**建议：** 总动画数据不超过 8 MB（ESP32-S3 4MB Flash 设备）

## 🎨 推荐的表情列表

1. ✅ **happy** - 开心（优先）
2. ✅ **surprised** - 惊讶（优先）
3. ✅ **sad** - 伤心
4. **angry** - 生气
5. **thinking** - 思考
6. **idle** - 待机（循环）

## ⚠️ 常见错误

### 错误 1：找不到 PIL/Pillow
```bash
pip install Pillow
```

### 错误 2：Flash 空间不足
**解决：** 减少帧数或降低分辨率

### 错误 3：动画播放卡顿
**解决：** 降低帧率到 10-12 FPS

## 📞 需要帮助？

查看完整文档：[docs/animation_workflow.md](animation_workflow.md)

---

**开始创建你的第一个表情动画吧！** 🎉
