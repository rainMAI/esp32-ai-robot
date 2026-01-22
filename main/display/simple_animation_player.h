/**
 * @file simple_animation_player.h
 * @brief 简化的动画播放器 - 专用于播放 anim_eye 动画
 */

#ifndef SIMPLE_ANIMATION_PLAYER_H
#define SIMPLE_ANIMATION_PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 动画播放状态
typedef enum {
    ANIM_STOPPED = 0,
    ANIM_PLAYING = 1,
    ANIM_PAUSED = 2
} AnimState;

/**
 * @brief 开始播放动画
 * @param loop 是否循环播放
 */
void simple_anim_start(bool loop);

/**
 * @brief 停止播放动画
 */
void simple_anim_stop(void);

/**
 * @brief 暂停动画
 */
void simple_anim_pause(void);

/**
 * @brief 恢复动画
 */
void simple_anim_resume(void);

/**
 * @brief 更新动画帧（需要在主循环中调用）
 * @return true 表示有新帧需要显示，false 表示无更新
 */
bool simple_anim_update(void);

/**
 * @brief 获取当前帧数据
 * @return 当前帧的像素数据指针
 */
const uint16_t* simple_anim_get_current_frame(void);

/**
 * @brief 获取当前帧索引
 * @return 当前帧索引
 */
uint16_t simple_anim_get_frame_index(void);

/**
 * @brief 获取播放状态
 * @return 播放状态
 */
AnimState simple_anim_get_state(void);

/**
 * @brief 获取动画进度
 * @return 进度百分比 (0-100)
 */
uint8_t simple_anim_get_progress(void);

/**
 * @brief 绘制当前帧到两个屏幕
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t simple_anim_draw_frame(void);

/**
 * @brief 动画播放任务（在独立线程中运行）
 */
void task_animation_player(void* pvParameters);

/**
 * @brief 启动动画播放任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t simple_anim_start_task(void);

#ifdef __cplusplus
}
#endif

#endif // SIMPLE_ANIMATION_PLAYER_H
