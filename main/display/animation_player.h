/**
 * @file animation_player.h
 * @brief PNG 序列帧动画播放器
 *
 * 支持播放由 png_sequence_to_array.py 生成的动画
 */

#ifndef ANIMATION_PLAYER_H
#define ANIMATION_PLAYER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 动画播放状态
typedef enum {
    ANIM_STATE_STOPPED = 0,    // 停止
    ANIM_STATE_PLAYING = 1,    // 播放中
    ANIM_STATE_PAUSED = 2,     // 暂停
    ANIM_STATE_LOOPING = 3,    // 循环播放
} AnimState;

// 动画播放配置
typedef struct {
    bool loop;                 // 是否循环播放
    bool restart_on_end;       // 播放结束后是否自动停止
    uint8_t speed_multiplier;  // 速度倍数 (1-255, 128=正常速度)
} AnimPlayConfig;

// 动画播放器结构体
typedef struct {
    const void* anim_data;      // 动画数据指针（指向具体的动画结构体）
    AnimState state;           // 播放状态
    uint16_t current_frame;    // 当前帧
    uint32_t last_frame_time;  // 上一帧的时间（微秒）
    uint16_t frame_count;      // 总帧数
    uint8_t fps;               // 帧率
    uint32_t frame_duration_us;// 每帧持续时间（微秒）
    AnimPlayConfig config;     // 播放配置
    void (*on_complete)(void); // 播放完成回调
} AnimationPlayer;

/**
 * @brief 初始化动画播放器
 * @param player 播放器指针
 * @param anim_data 动画数据指针
 * @param config 播放配置（NULL 使用默认配置）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t anim_player_init(AnimationPlayer* player,
                          const void* anim_data,
                          const AnimPlayConfig* config);

/**
 * @brief 开始播放动画
 * @param player 播放器指针
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t anim_player_start(AnimationPlayer* player);

/**
 * @brief 暂停动画
 * @param player 播放器指针
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t anim_player_pause(AnimationPlayer* player);

/**
 * @brief 停止动画
 * @param player 播放器指针
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t anim_player_stop(AnimationPlayer* player);

/**
 * @brief 跳转到指定帧
 * @param player 播放器指针
 * @param frame_index 目标帧索引
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t anim_player_seek(AnimationPlayer* player, uint16_t frame_index);

/**
 * @brief 更新动画帧（需要在主循环中定期调用）
 * @param player 播放器指针
 * @return 当前帧的图像数据指针，NULL 表示无更新
 */
const uint16_t* anim_player_update(AnimationPlayer* player);

/**
 * @brief 获取当前播放状态
 * @param player 播放器指针
 * @return 播放状态
 */
AnimState anim_player_get_state(const AnimationPlayer* player);

/**
 * @brief 获取当前帧索引
 * @param player 播放器指针
 * @return 当前帧索引
 */
uint16_t anim_player_get_current_frame(const AnimationPlayer* player);

/**
 * @brief 设置播放完成回调
 * @param player 播放器指针
 * @param callback 回调函数指针
 */
void anim_player_set_callback(AnimationPlayer* player,
                             void (*callback)(void));

/**
 * @brief 获取动画进度
 * @param player 播放器指针
 * @return 进度百分比 (0-100)
 */
uint8_t anim_player_get_progress(const AnimationPlayer* player);

#ifdef __cplusplus
}
#endif

#endif // ANIMATION_PLAYER_H
