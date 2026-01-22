/**
 * @file animation_player.c
 * @brief PNG 序列帧动画播放器实现
 */

#include "animation_player.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "eye_display.h"  // 使用已有的 LCD 互斥锁和绘图函数

static const char* TAG = "anim_player";

// 默认播放配置
static const AnimPlayConfig default_config = {
    .loop = false,
    .restart_on_end = true,
    .speed_multiplier = 128,  // 正常速度
};

esp_err_t anim_player_init(AnimationPlayer* player,
                          const void* anim_data,
                          const AnimPlayConfig* config) {
    if (!player || !anim_data) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_FAIL;
    }

    // 假设动画数据结构有以下字段（需要根据实际的动画结构体调整）
    // 这里使用通用的指针访问，实际使用时需要根据具体的动画结构体定义

    player->anim_data = anim_data;
    player->state = ANIM_STATE_STOPPED;
    player->current_frame = 0;
    player->last_frame_time = 0;

    // 使用默认配置或用户配置
    if (config) {
        player->config = *config;
    } else {
        player->config = default_config;
    }

    player->on_complete = NULL;

    ESP_LOGI(TAG, "Animation player initialized");
    return ESP_OK;
}

esp_err_t anim_player_start(AnimationPlayer* player) {
    if (!player) {
        return ESP_FAIL;
    }

    player->state = player->config.loop ? ANIM_STATE_LOOPING : ANIM_STATE_PLAYING;
    player->current_frame = 0;
    player->last_frame_time = esp_timer_get_time();

    ESP_LOGI(TAG, "Animation started (loop=%d)", player->config.loop);
    return ESP_OK;
}

esp_err_t anim_player_pause(AnimationPlayer* player) {
    if (!player || player->state == ANIM_STATE_STOPPED) {
        return ESP_FAIL;
    }

    player->state = ANIM_STATE_PAUSED;
    ESP_LOGI(TAG, "Animation paused");
    return ESP_OK;
}

esp_err_t anim_player_stop(AnimationPlayer* player) {
    if (!player) {
        return ESP_FAIL;
    }

    player->state = ANIM_STATE_STOPPED;
    player->current_frame = 0;

    ESP_LOGI(TAG, "Animation stopped");
    return ESP_OK;
}

esp_err_t anim_player_seek(AnimationPlayer* player, uint16_t frame_index) {
    if (!player || !player->anim_data) {
        return ESP_FAIL;
    }

    // TODO: 需要根据实际的动画结构体访问 frame_count
    // if (frame_index >= player->frame_count) {
    //     ESP_LOGE(TAG, "Frame index out of range");
    //     return ESP_FAIL;
    // }

    player->current_frame = frame_index;
    return ESP_OK;
}

const uint16_t* anim_player_update(AnimationPlayer* player) {
    if (!player || !player->anim_data) {
        return NULL;
    }

    // 检查播放状态
    if (player->state == ANIM_STATE_STOPPED || player->state == ANIM_STATE_PAUSED) {
        return NULL;
    }

    uint64_t current_time = esp_timer_get_time();
    uint64_t elapsed = current_time - player->last_frame_time;

    // 计算帧持续时间（考虑速度倍数）
    uint32_t adjusted_duration = (player->frame_duration_us * 256) / player->config.speed_multiplier;

    // 检查是否应该播放下一帧
    if (elapsed < adjusted_duration) {
        return NULL;  // 还没到下一帧的时间
    }

    // 更新到下一帧
    player->current_frame++;
    player->last_frame_time = current_time;

    // 检查是否播放完成
    if (player->current_frame >= player->frame_count) {
        if (player->config.loop) {
            // 循环播放
            player->current_frame = 0;
            ESP_LOGD(TAG, "Animation looped");
        } else {
            // 非循环播放，停止
            player->current_frame = player->frame_count - 1;
            player->state = ANIM_STATE_STOPPED;

            ESP_LOGI(TAG, "Animation completed");

            // 触发完成回调
            if (player->on_complete) {
                player->on_complete();
            }

            return NULL;
        }
    }

    // TODO: 返回当前帧的图像数据
    // 需要根据实际的动画结构体访问 frames 数组
    // return player->anim_data->frames[player->current_frame];

    return NULL;
}

AnimState anim_player_get_state(const AnimationPlayer* player) {
    if (!player) {
        return ANIM_STATE_STOPPED;
    }
    return player->state;
}

uint16_t anim_player_get_current_frame(const AnimationPlayer* player) {
    if (!player) {
        return 0;
    }
    return player->current_frame;
}

void anim_player_set_callback(AnimationPlayer* player,
                             void (*callback)(void)) {
    if (player) {
        player->on_complete = callback;
    }
}

uint8_t anim_player_get_progress(const AnimationPlayer* player) {
    if (!player || player->frame_count == 0) {
        return 0;
    }

    return (player->current_frame * 100) / player->frame_count;
}
