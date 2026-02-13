/**
 * @file multi_animation_manager.c
 * @brief 多表情动画管理器实现
 */

#include "multi_animation_manager.h"
#include "animations/anim_eye.h"
#include "animations/anim_grok.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "eye_display.h"

static const char* TAG = "multi_anim";

// 当前播放状态
static expression_type_t current_expression = EXPRESSION_DEFAULT;
static anim_state_t current_state = ANIM_STATE_STOPPED;
static bool should_loop = false;

// 帧控制
static uint16_t current_frame = 0;
static uint64_t last_frame_time = 0;
static uint64_t frame_duration_us = 0;  // 每帧的微秒数

// 动画元数据结构
typedef struct {
    const uint16_t** frames;
    uint16_t frame_count;
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint32_t duration_ms;
    const char* name;
} animation_metadata_t;

// 动画元数据表
static const animation_metadata_t animations[] = {
    [EXPRESSION_EYE] = {
        .frames = anim_eye.frames,
        .frame_count = anim_eye.frame_count,
        .width = anim_eye.width,
        .height = anim_eye.height,
        .fps = anim_eye.fps,
        .duration_ms = anim_eye.duration_ms,
        .name = "Eye"
    },
    [EXPRESSION_GROK] = {
        .frames = anim_grok.frames,
        .frame_count = anim_grok.frame_count,
        .width = anim_grok.width,
        .height = anim_grok.height,
        .fps = anim_grok.fps,
        .duration_ms = anim_grok.duration_ms,
        .name = "Grok"
    }
};

// LCD 面板句柄和互斥锁（在 eye_display.h 中声明为 extern）
// lcd_panel_eye, lcd_panel_eye2, lcd_mutex

// 每次传输的行数
#define LINES_PER_BATCH 10

esp_err_t multi_anim_init(void) {
    ESP_LOGI(TAG, "多表情动画管理器初始化");
    current_expression = EXPRESSION_DEFAULT;
    current_state = ANIM_STATE_STOPPED;
    return ESP_OK;
}

esp_err_t multi_anim_start_task(void) {
    ESP_LOGI(TAG, "启动多表情动画管理任务");
    // 任务在 eye_display.cc 中已启动
    return ESP_OK;
}

esp_err_t multi_anim_switch_expression(expression_type_t expression, bool loop) {
    if (expression >= EXPRESSION_MAX || expression == EXPRESSION_DEFAULT) {
        ESP_LOGE(TAG, "无效的表情类型: %d", expression);
        return ESP_FAIL;
    }

    if (!multi_anim_is_expression_available(expression)) {
        ESP_LOGE(TAG, "表情不可用: %s", multi_anim_get_expression_name(expression));
        return ESP_FAIL;
    }

    // 停止当前动画
    multi_anim_stop();

    // 切换到新表情
    current_expression = expression;
    should_loop = loop;
    current_frame = 0;
    current_state = ANIM_STATE_PLAYING;

    const animation_metadata_t* anim = &animations[expression];
    frame_duration_us = 1000000 / anim->fps;  // 微秒/帧
    // 设置为过去，确保第一帧立即绘制
    last_frame_time = esp_timer_get_time() - frame_duration_us;

    ESP_LOGI(TAG, "切换到表情: %s (帧数: %d, FPS: %d, 循环: %d)",
             anim->name, anim->frame_count, anim->fps, loop);

    return ESP_OK;
}

void multi_anim_stop(void) {
    current_state = ANIM_STATE_STOPPED;
    current_frame = 0;
}

void multi_anim_pause(void) {
    if (current_state == ANIM_STATE_PLAYING) {
        current_state = ANIM_STATE_PAUSED;
    }
}

void multi_anim_resume(void) {
    if (current_state == ANIM_STATE_PAUSED) {
        current_state = ANIM_STATE_PLAYING;
        last_frame_time = esp_timer_get_time();  // 重置时间，避免跳帧
    }
}

expression_type_t multi_anim_get_current_expression(void) {
    return current_expression;
}

anim_state_t multi_anim_get_state(void) {
    return current_state;
}

bool multi_anim_is_expression_available(expression_type_t expression) {
    if (expression == EXPRESSION_DEFAULT || expression >= EXPRESSION_MAX) {
        return false;
    }

    const animation_metadata_t* anim = &animations[expression];
    return (anim->frames != NULL && anim->frame_count > 0);
}

const char* multi_anim_get_expression_name(expression_type_t expression) {
    switch (expression) {
        case EXPRESSION_DEFAULT:
            return "Default";
        case EXPRESSION_EYE:
            return "Eye";
        case EXPRESSION_GROK:
            return "Grok";
        default:
            return "Unknown";
    }
}

// 内部函数：更新动画帧
static bool update_frame(void) {
    if (current_state != ANIM_STATE_PLAYING) {
        return false;
    }

    uint64_t current_time = esp_timer_get_time();
    uint64_t elapsed = current_time - last_frame_time;

    // 检查是否应该播放下一帧
    if (elapsed < frame_duration_us) {
        return false;
    }

    // 更新到下一帧
    current_frame++;
    last_frame_time = current_time;

    const animation_metadata_t* anim = &animations[current_expression];

    // 检查是否播放完成
    if (current_frame >= anim->frame_count) {
        if (should_loop) {
            // 循环播放
            current_frame = 0;
        } else {
            // 非循环播放，停止
            current_frame = anim->frame_count - 1;
            current_state = ANIM_STATE_STOPPED;
            ESP_LOGI(TAG, "动画播放完成: %s", anim->name);
            return false;
        }
    }

    return true;
}

// 内部函数：绘制当前帧
static esp_err_t draw_current_frame(void) {
    if (current_expression >= EXPRESSION_MAX) {
        ESP_LOGE(TAG, "当前表情无效: %d", current_expression);
        return ESP_FAIL;
    }

    const animation_metadata_t* anim = &animations[current_expression];
    const uint16_t* frame_data = anim->frames[current_frame];

    if (frame_data == NULL) {
        ESP_LOGE(TAG, "帧数据为空, 当前帧: %d", current_frame);
        return ESP_FAIL;
    }

    // 分批传输的行缓冲区
    uint16_t* line_buffer = malloc(LINES_PER_BATCH * anim->width * sizeof(uint16_t));
    if (line_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_FAIL;
    }

    esp_err_t ret1 = ESP_OK, ret2 = ESP_OK;
    if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
        // 分批绘制每一批行
        for (uint16_t y = 0; y < anim->height; y += LINES_PER_BATCH) {
            uint8_t lines_to_process = (anim->height - y) < LINES_PER_BATCH ?
                                       (anim->height - y) : LINES_PER_BATCH;

            // 字节交换并填充当前批次的行缓冲区
            for (uint8_t line = 0; line < lines_to_process; line++) {
                uint16_t src_offset = (y + line) * anim->width;
                uint16_t dst_offset = line * anim->width;

                for (uint16_t x = 0; x < anim->width; x++) {
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
                    0, y,
                    anim->width, y + lines_to_process,
                    line_buffer
                );
            } else {
                ESP_LOGE(TAG, "目标面板为 NULL！");
                ret1 = ESP_ERR_INVALID_ARG;
            }
            ret2 = ESP_OK;  // 单眼模式，忽略第二块屏幕
#else
            // 双眼模式 - 绘制到两块屏幕
            ret1 = esp_lcd_panel_draw_bitmap(
                lcd_panel_eye,
                0, y,
                anim->width, y + lines_to_process,
                line_buffer
            );

            ret2 = esp_lcd_panel_draw_bitmap(
                lcd_panel_eye2,
                0, y,
                anim->width, y + lines_to_process,
                line_buffer
            );
#endif

            if (ret1 != ESP_OK || ret2 != ESP_OK) {
                ESP_LOGE(TAG, "绘制失败 (左眼: %d, 右眼: %d)", ret1, ret2);
                break;
            }
        }

        xSemaphoreGive(lcd_mutex);
    } else {
        ESP_LOGE(TAG, "获取 LCD 互斥锁失败");
        free(line_buffer);
        return ESP_FAIL;
    }

    free(line_buffer);
    return (ret1 == ESP_OK && ret2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

// 导出函数供 eye_display.cc 调用
void multi_anim_update_and_draw(void) {
    if (current_state == ANIM_STATE_STOPPED) {
        return;
    }

    if (update_frame()) {
        draw_current_frame();
    }
}
