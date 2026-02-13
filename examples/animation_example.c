/**
 * @file animation_example.c
 * @brief PNG 序列帧动画使用示例
 *
 * 这个示例展示了如何使用 PNG 序列帧动画系统
 */

#include "animation_player.h"
#include "esp_log.h"
#include "esp_timer.h"

// 假设你已经使用 png_sequence_to_array.py 生成了动画数据
// #include "anim_happy.h"
// #include "anim_surprised.h"

static const char* TAG = "anim_example";

// 全局动画播放器实例
static AnimationPlayer anim_player;

// 动画播放完成回调
void on_animation_complete() {
    ESP_LOGI(TAG, "动画播放完成！");

    // 可以在这里触发其他动作，例如：
    // - 切换到其他表情
    // - 返回默认眼睛模式
    // - 播放音效
}

/**
 * @brief 初始化动画系统
 */
void init_animation_system() {
    ESP_LOGI(TAG, "初始化动画系统...");

    // 配置动画播放参数
    AnimPlayConfig config = {
        .loop = false,              // 不循环播放
        .restart_on_end = true,     // 播放完成后自动停止
        .speed_multiplier = 128,    // 正常速度 (128 = 1.0x, 64 = 0.5x, 256 = 2.0x)
    };

    // 初始化播放器（使用实际的动画数据）
    // anim_player_init(&anim_player, &anim_happy, &config);

    // 设置播放完成回调
    anim_player_set_callback(&anim_player, on_animation_complete);

    ESP_LOGI(TAG, "动画系统初始化完成");
}

/**
 * @brief 播放表情动画
 * @param emotion 表情类型
 */
void play_emotion_animation(EmotionType emotion) {
    switch(emotion) {
        case EMOTION_HAPPY:
            // 播放开心表情
            // anim_player_init(&anim_player, &anim_happy, NULL);
            anim_player_start(&anim_player);
            ESP_LOGI(TAG, "播放开心表情");
            break;

        case EMOTION_SURPRISED:
            // 播放惊讶表情
            // anim_player_init(&anim_player, &anim_surprised, NULL);
            anim_player_start(&anim_player);
            ESP_LOGI(TAG, "播放惊讶表情");
            break;

        case EMOTION_SAD:
            // 播放伤心表情
            // anim_player_init(&anim_player, &anim_sad, NULL);
            anim_player_start(&anim_player);
            ESP_LOGI(TAG, "播放伤心表情");
            break;

        default:
            ESP_LOGW(TAG, "未知的表情类型");
            break;
    }
}

/**
 * @brief 主循环 - 定期更新动画
 *
 * 在你的主循环中调用这个函数来更新动画帧
 */
void update_animation() {
    // 更新动画播放器
    const uint16_t* frame_data = anim_player_update(&anim_player);

    // 如果有新帧需要显示
    if (frame_data != NULL) {
        // 绘制到 LCD 屏幕
        // 使用线程安全的绘图函数
        esp_lcd_safe_draw_bitmap(0, 0, 240, 240, frame_data);

        // 获取播放进度
        uint8_t progress = anim_player_get_progress(&anim_player);
        ESP_LOGD(TAG, "动画进度: %d%%", progress);
    }

    // 检查播放状态
    AnimState state = anim_player_get_state(&anim_player);
    if (state == ANIM_STATE_STOPPED) {
        // 动画已停止，可以返回正常的眼睛显示模式
        // 例如：恢复眼睛跟踪、眨眼等功能
    }
}

/**
 * @brief 示例：根据语音播放对应表情
 */
void play_emotion_by_voice(const char* emotion_text) {
    if (strstr(emotion_text, "开心") || strstr(emotion_text, "高兴")) {
        play_emotion_animation(EMOTION_HAPPY);
    } else if (strstr(emotion_text, "惊讶") || strstr(emotion_text, "意外")) {
        play_emotion_animation(EMOTION_SURPRISED);
    } else if (strstr(emotion_text, "伤心") || strstr(emotion_text, "难过")) {
        play_emotion_animation(EMOTION_SAD);
    } else {
        ESP_LOGI(TAG, "未识别的表情: %s", emotion_text);
    }
}

/**
 * @brief 示例：与现有眼睛系统集成
 *
 * 这个函数展示如何在动画播放和正常眼睛显示之间切换
 */
void toggle_animation_mode(bool enable_animation) {
    if (enable_animation) {
        // 停止正常的眼睛更新
        // vTaskSuspend(task_update_eye_handler);

        // 开始播放动画
        anim_player_start(&anim_player);

        ESP_LOGI(TAG, "切换到动画模式");
    } else {
        // 停止动画播放
        anim_player_stop(&anim_player);

        // 恢复正常的眼睛更新
        // vTaskResume(task_update_eye_handler);

        ESP_LOGI(TAG, "切换到正常眼睛模式");
    }
}

/**
 * @brief 示例：循环播放待机动画
 */
void play_idle_animation() {
    AnimPlayConfig config = {
        .loop = true,               // 循环播放
        .restart_on_end = false,    // 循环模式下不自动停止
        .speed_multiplier = 128,    // 正常速度
    };

    // 初始化待机动画
    // anim_player_init(&anim_player, &anim_idle, &config);

    // 开始播放
    anim_player_start(&anim_player);

    ESP_LOGI(TAG, "开始循环播放待机动画");
}

// ========================================
// 在你的代码中集成示例：
// ========================================

/*
1. 在 main.cc 或应用初始化中：

   void app_main() {
       // ... 其他初始化代码 ...

       // 初始化动画系统
       init_animation_system();

       // ... 其他初始化代码 ...
   }

2. 在语音识别回调中：

   void voice_command_callback(const char* command) {
       if (strstr(command, "表情")) {
           play_emotion_by_voice(command);
       }
   }

3. 在主循环中：

   void main_loop() {
       while(1) {
           // 更新动画
           update_animation();

           // 其他任务...

           vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 更新一次
       }
   }

4. 集成到现有的眼睛显示系统：

   // 在接收到特定事件时播放动画
   void on_wake_word_detected() {
       // 播放惊讶表情
       play_emotion_animation(EMOTION_SURPRISED);
   }

   void on_voice_response_start() {
       // 播放说话表情
       play_emotion_animation(EMOTION_TALKING);
   }

   void on_voice_response_end() {
       // 返回正常眼睛模式
       toggle_animation_mode(false);
   }
*/
