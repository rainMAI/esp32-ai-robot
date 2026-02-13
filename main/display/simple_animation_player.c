/**
 * @file simple_animation_player.c
 * @brief 简化的动画播放器 - 专用于播放 anim_eye 动画
 */

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "eye_display.h"
#include "animations/anim_eye.h"

static const char* TAG = "simple_anim";

// 动画播放状态
typedef enum {
    ANIM_STOPPED = 0,
    ANIM_PLAYING = 1,
    ANIM_PAUSED = 2
} AnimState;

// 动画播放器结构体
typedef struct {
    AnimState state;           // 播放状态
    uint16_t current_frame;    // 当前帧
    uint32_t last_frame_time;  // 上一帧的时间（微秒）
    bool loop;                 // 是否循环播放
    uint32_t frame_duration_us;// 每帧持续时间（微秒）
} SimpleAnimationPlayer;

static SimpleAnimationPlayer player = {
    .state = ANIM_STOPPED,
    .current_frame = 0,
    .last_frame_time = 0,
    .loop = true,  // 默认循环播放
    .frame_duration_us = 1000000 / ANIM_EYE_FPS  // 计算每帧持续时间
};

/**
 * @brief 开始播放动画
 */
void simple_anim_start(bool loop) {
    player.state = ANIM_PLAYING;
    player.current_frame = 0;
    // 设置为过去，确保第一帧立即绘制
    player.last_frame_time = esp_timer_get_time() - player.frame_duration_us;
    player.loop = loop;

    ESP_LOGI(TAG, "动画开始播放 (帧数: %d, FPS: %d, 循环: %d)",
             ANIM_EYE_FRAME_COUNT, ANIM_EYE_FPS, loop);
}

/**
 * @brief 停止播放动画
 */
void simple_anim_stop(void) {
    player.state = ANIM_STOPPED;
    player.current_frame = 0;
    ESP_LOGI(TAG, "动画停止");
}

/**
 * @brief 暂停动画
 */
void simple_anim_pause(void) {
    if (player.state == ANIM_PLAYING) {
        player.state = ANIM_PAUSED;
        ESP_LOGI(TAG, "动画暂停");
    }
}

/**
 * @brief 恢复动画
 */
void simple_anim_resume(void) {
    if (player.state == ANIM_PAUSED) {
        player.state = ANIM_PLAYING;
        player.last_frame_time = esp_timer_get_time();
        ESP_LOGI(TAG, "动画恢复");
    }
}

/**
 * @brief 更新动画帧（需要在主循环中调用）
 * @return true 表示有新帧需要显示，false 表示无更新
 */
bool simple_anim_update(void) {
    // 检查播放状态
    if (player.state != ANIM_PLAYING) {
        return false;
    }

    uint64_t current_time = esp_timer_get_time();
    uint64_t elapsed = current_time - player.last_frame_time;

    // 检查是否应该播放下一帧
    if (elapsed < player.frame_duration_us) {
        return false;  // 还没到下一帧的时间
    }

    // 更新到下一帧
    player.current_frame++;
    player.last_frame_time = current_time;

    // 检查是否播放完成
    if (player.current_frame >= ANIM_EYE_FRAME_COUNT) {
        if (player.loop) {
            // 循环播放
            player.current_frame = 0;
            ESP_LOGD(TAG, "动画循环");
        } else {
            // 非循环播放，停止
            player.current_frame = ANIM_EYE_FRAME_COUNT - 1;
            player.state = ANIM_STOPPED;
            ESP_LOGI(TAG, "动画播放完成");
            return false;
        }
    }

    return true;  // 有新帧需要显示
}

/**
 * @brief 获取当前帧数据
 * @return 当前帧的像素数据指针
 */
const uint16_t* simple_anim_get_current_frame(void) {
    if (player.current_frame >= ANIM_EYE_FRAME_COUNT) {
        return NULL;
    }

    // 新格式：frames 是指针数组，直接返回对应帧的指针
    return anim_eye.frames[player.current_frame];
}

/**
 * @brief 获取当前帧索引
 * @return 当前帧索引
 */
uint16_t simple_anim_get_frame_index(void) {
    return player.current_frame;
}

/**
 * @brief 获取播放状态
 * @return 播放状态
 */
AnimState simple_anim_get_state(void) {
    return player.state;
}

/**
 * @brief 获取动画进度
 * @return 进度百分比 (0-100)
 */
uint8_t simple_anim_get_progress(void) {
    return (player.current_frame * 100) / ANIM_EYE_FRAME_COUNT;
}

// 每次传输的行数（和眼睛渲染保持一致）
#define LINES_PER_BATCH 10

/**
 * @brief 绘制当前帧到两个屏幕
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t simple_anim_draw_frame(void) {
    const uint16_t* frame_data = simple_anim_get_current_frame();

    if (frame_data == NULL) {
        ESP_LOGE(TAG, "帧数据为空,当前帧: %d", player.current_frame);
        return ESP_FAIL;
    }

    // 240x240 全屏显示 - 无需偏移
    int offset_x = 0;
    int offset_y = 0;

    // 分批传输的行缓冲区（每批10行）
    uint16_t* line_buffer = malloc(LINES_PER_BATCH * ANIM_EYE_WIDTH * sizeof(uint16_t));
    if (line_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_FAIL;
    }

    // 输出调试信息
    // ESP_LOGI(TAG, "绘制帧 %d, 前4像素: 0x%04X 0x%04X 0x%04X 0x%04X",
    //          player.current_frame,
    //          frame_data[0], frame_data[1], frame_data[2], frame_data[3]);

    // 使用互斥锁保护 LCD 操作
    esp_err_t ret1 = ESP_OK, ret2 = ESP_OK;
    if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
        // 分批绘制每一批行
        for (uint16_t y = 0; y < ANIM_EYE_HEIGHT; y += LINES_PER_BATCH) {
            // 计算本次批处理的实际行数
            uint8_t lines_to_process = (ANIM_EYE_HEIGHT - y) < LINES_PER_BATCH ?
                                       (ANIM_EYE_HEIGHT - y) : LINES_PER_BATCH;

            // 字节交换并填充当前批次的行缓冲区
            for (uint8_t line = 0; line < lines_to_process; line++) {
                uint16_t src_offset = (y + line) * ANIM_EYE_WIDTH;
                uint16_t dst_offset = line * ANIM_EYE_WIDTH;

                for (uint16_t x = 0; x < ANIM_EYE_WIDTH; x++) {
                    // 字节交换 - 和眼睛渲染逻辑一致
                    line_buffer[dst_offset + x] = (frame_data[src_offset + x] >> 8) |
                                                   (frame_data[src_offset + x] << 8);
                }
            }

            // 绘制到屏幕 - 根据 NUM_EYES 决定绘制几块
#if NUM_EYES == 1
            // 单眼模式 - 只绘制到有效的屏幕
            esp_lcd_panel_handle_t target_panel = (lcd_panel_eye2 != NULL) ? lcd_panel_eye2 : lcd_panel_eye;
            if (target_panel != NULL) {
                ret1 = esp_lcd_panel_draw_bitmap(
                    target_panel,
                    offset_x, offset_y + y,
                    offset_x + ANIM_EYE_WIDTH, offset_y + y + lines_to_process,
                    line_buffer
                );
            } else {
                ret1 = ESP_OK;  // 没有有效屏幕，跳过
            }
            ret2 = ESP_OK;  // 单眼模式，忽略第二块屏幕
#else
            // 双眼模式 - 绘制到两块屏幕
            ret1 = esp_lcd_panel_draw_bitmap(
                lcd_panel_eye,
                offset_x, offset_y + y,
                offset_x + ANIM_EYE_WIDTH, offset_y + y + lines_to_process,
                line_buffer
            );

            ret2 = esp_lcd_panel_draw_bitmap(
                lcd_panel_eye2,
                offset_x, offset_y + y,
                offset_x + ANIM_EYE_WIDTH, offset_y + y + lines_to_process,
                line_buffer
            );
#endif

            if (ret1 != ESP_OK || ret2 != ESP_OK) {
                ESP_LOGE(TAG, "绘制失败 (左眼: %d, 右眼: %d), 批次: y=%d, lines=%d",
                         ret1, ret2, y, lines_to_process);
                break;
            }
        }

        xSemaphoreGive(lcd_mutex);

        if (ret1 != ESP_OK || ret2 != ESP_OK) {
            ESP_LOGE(TAG, "绘制帧 %d 失败 (左眼: %d, 右眼: %d)", player.current_frame, ret1, ret2);
            free(line_buffer);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "获取 LCD 互斥锁失败");
        free(line_buffer);
        return ESP_FAIL;
    }

    free(line_buffer);
    return ESP_OK;
}

/**
 * @brief 动画播放任务（在独立线程中运行）
 */
void task_animation_player(void* pvParameters) {
    ESP_LOGI(TAG, "动画播放任务启动");

    // 自动开始播放（循环模式）
    simple_anim_start(true);

    while (1) {
        // 更新动画
        if (simple_anim_update()) {
            // 有新帧，绘制到屏幕
            simple_anim_draw_frame();

            // 输出调试信息（每秒输出一次）
            // static uint32_t last_debug_time = 0;
            // uint32_t current_time = esp_timer_get_time() / 1000;
            // if (current_time - last_debug_time > 1000) {
            //     ESP_LOGI(TAG, "播放帧: %d/%d (%d%%)",
            //              player.current_frame + 1,
            //              ANIM_EYE_FRAME_COUNT,
            //              simple_anim_get_progress());
            //     last_debug_time = current_time;
            // }
        }

        // 短暂延时，避免占用 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 启动动画播放任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t simple_anim_start_task(void) {
    BaseType_t ret = xTaskCreate(
        task_animation_player,
        "anim_task",
        4096,
        NULL,
        5,  // 优先级
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建动画任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "动画播放任务已启动");
    return ESP_OK;
}
