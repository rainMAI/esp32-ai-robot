#include "touch_button_manager.h"
#include "config.h"
#include "esp_log.h"
#include "iot_button.h"
#include "touch_button.h"
#include "touch_sensor_lowlevel.h"

static const char *TAG = "TouchBtn";

TouchButtonManager::TouchButtonManager()
    : touch_sensor_callback_()
    , touch_sensor2_callback_()
    , btn_touch1_(nullptr)
    , btn_touch2_(nullptr) {
}

TouchButtonManager::~TouchButtonManager() {
    if (btn_touch1_) {
        iot_button_delete(btn_touch1_);
    }
    if (btn_touch2_) {
        iot_button_delete(btn_touch2_);
    }
}

void TouchButtonManager::Initialize() {
    esp_err_t ret;

    // 1. 初始化底层触摸传感器（只初始化一次）
    // 注意：ESP32-S3 的通道 0 是内部降噪通道，不能使用
    // 使用通道 1 和 2 (对应 GPIO 0 和 GPIO 2)
    uint32_t touch_channel_list[] = {1, 2};  // 通道 1 和 2
    touch_lowlevel_type_t *channel_type = (touch_lowlevel_type_t *)calloc(2, sizeof(touch_lowlevel_type_t));
    channel_type[0] = TOUCH_LOWLEVEL_TYPE_TOUCH;
    channel_type[1] = TOUCH_LOWLEVEL_TYPE_TOUCH;

    touch_lowlevel_config_t low_config = {
        .channel_num = 2,
        .channel_list = touch_channel_list,
        .channel_type = channel_type,
    };
    ret = touch_sensor_lowlevel_create(&low_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch_sensor_lowlevel_create failed: %s", esp_err_to_name(ret));
        free(channel_type);
        return;
    }
    free(channel_type);

    // 2. 启动底层触摸传感器
    ret = touch_sensor_lowlevel_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch_sensor_lowlevel_start failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Touch sensor lowlevel initialized and started");

    // 3. 创建触摸按钮 1（GPIO 0 - 轻触）
    const button_config_t btn_cfg = {
        .long_press_time = 2000,    // 长按 2000ms
        .short_press_time = 300,    // 短按 300ms
    };

    button_touch_config_t touch_cfg1 = {
        .touch_channel = TOUCH_SENSOR_CHANNEL,
        .channel_threshold = TOUCH_THRESHOLD_PERCENT,  // 15% 变化率
        .skip_lowlevel_init = true,  // 已经手动初始化过了
    };

    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg1, &btn_touch1_);
    if (ret != ESP_OK || btn_touch1_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create touch button 1: %s", esp_err_to_name(ret));
        return;
    }

    // 注册回调：轻触触发
    iot_button_register_cb(btn_touch1_, BUTTON_PRESS_DOWN, NULL,
                           [](void *arg, void *data) {
                               TouchButtonManager *mgr = (TouchButtonManager *)data;
                               ESP_LOGI(TAG, "Touch sensor 1 (GPIO 1) triggered");
                               if (mgr && mgr->touch_sensor_callback_) {
                                   mgr->touch_sensor_callback_();
                               }
                           }, this);

    ESP_LOGI(TAG, "Touch button 1 (GPIO 1) created successfully");

    // 4. 创建触摸按钮 2（GPIO 2 - 轻触）
    button_touch_config_t touch_cfg2 = {
        .touch_channel = TOUCH_SENSOR2_CHANNEL,
        .channel_threshold = TOUCH_THRESHOLD_PERCENT,
        .skip_lowlevel_init = true,
    };

    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg2, &btn_touch2_);
    if (ret != ESP_OK || btn_touch2_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create touch button 2: %s", esp_err_to_name(ret));
        // 第一个按钮已经创建成功，继续使用
    } else {
        // 注册回调：触摸触发
        iot_button_register_cb(btn_touch2_, BUTTON_PRESS_DOWN, NULL,
                               [](void *arg, void *data) {
                                   TouchButtonManager *mgr = (TouchButtonManager *)data;
                                   ESP_LOGI(TAG, "Touch sensor 2 (GPIO 2) triggered");
                                   if (mgr && mgr->touch_sensor2_callback_) {
                                       mgr->touch_sensor2_callback_();
                                   }
                               }, this);

        ESP_LOGI(TAG, "Touch button 2 (GPIO 2) created successfully");
    }

    ESP_LOGI(TAG, "Touch button manager initialized using ESP official components");
}

void TouchButtonManager::SetTouchSensorCallback(std::function<void()> callback) {
    touch_sensor_callback_ = callback;
}

void TouchButtonManager::SetTouchSensor2Callback(std::function<void()> callback) {
    touch_sensor2_callback_ = callback;
}
