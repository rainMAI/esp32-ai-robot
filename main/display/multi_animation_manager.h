/**
 * @file multi_animation_manager.h
 * @brief 多表情动画管理器 - 支持多个动画表情的切换和播放
 */

#ifndef MULTI_ANIMATION_MANAGER_H
#define MULTI_ANIMATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 支持的表情类型
typedef enum {
    EXPRESSION_DEFAULT = 0,   // 默认表情
    EXPRESSION_EYE = 1,       // 眼睛动画
    EXPRESSION_GROK = 2,      // Grok 动画
    EXPRESSION_MAX
} expression_type_t;

// 动画播放状态
typedef enum {
    ANIM_STATE_STOPPED = 0,
    ANIM_STATE_PLAYING = 1,
    ANIM_STATE_PAUSED = 2
} anim_state_t;

/**
 * @brief 初始化多表情动画管理器
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t multi_anim_init(void);

/**
 * @brief 启动动画管理任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t multi_anim_start_task(void);

/**
 * @brief 切换到指定表情
 * @param expression 表情类型
 * @param loop 是否循环播放
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t multi_anim_switch_expression(expression_type_t expression, bool loop);

/**
 * @brief 停止当前动画
 */
void multi_anim_stop(void);

/**
 * @brief 暂停当前动画
 */
void multi_anim_pause(void);

/**
 * @brief 恢复当前动画
 */
void multi_anim_resume(void);

/**
 * @brief 获取当前播放的表情
 * @return 当前表情类型
 */
expression_type_t multi_anim_get_current_expression(void);

/**
 * @brief 获取当前播放状态
 * @return 播放状态
 */
anim_state_t multi_anim_get_state(void);

/**
 * @brief 检查指定的表情是否可用
 * @param expression 表情类型
 * @return true 可用, false 不可用
 */
bool multi_anim_is_expression_available(expression_type_t expression);

/**
 * @brief 获取表情名称（用于调试）
 * @param expression 表情类型
 * @return 表情名称字符串
 */
const char* multi_anim_get_expression_name(expression_type_t expression);

/**
 * @brief 更新并绘制动画（在任务循环中调用）
 */
void multi_anim_update_and_draw(void);

#ifdef __cplusplus
}
#endif

#endif // MULTI_ANIMATION_MANAGER_H
