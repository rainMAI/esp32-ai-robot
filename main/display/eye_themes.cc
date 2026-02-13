#include "eye_themes.h"
#include "esp_log.h"
#include <string.h>

// 包含原有的大尺寸眼睛数据
#include "eyes_data.h"

static const char* TAG = "eye_themes";

// 主题配置表
static const EyeThemeConfig theme_configs[] = {
    // 原有大尺寸主题 (375x375)
    {
        .name = "xingkong",
        .sclera = sclera_xingkong,
        .iris = iris_xingkong,
        .width = 375,
        .height = 375,
        .iris_min = 180,
        .iris_max = 280
    },
    {
        .name = "shuimu",
        .sclera = sclera_shuimu,
        .iris = iris_xingkong,
        .width = 375,
        .height = 375,
        .iris_min = 180,
        .iris_max = 280
    },
    {
        .name = "keji",
        .sclera = sclera_keji,
        .iris = iris_xingkong,
        .width = 375,
        .height = 375,
        .iris_min = 180,
        .iris_max = 280
    }
};

#define NUM_THEMES (sizeof(theme_configs) / sizeof(EyeThemeConfig))

// 获取主题配置
const EyeThemeConfig* GetEyeThemeConfig(EyeTheme theme) {
    if (theme < 0 || theme >= NUM_THEMES) {
        ESP_LOGW(TAG, "Invalid theme ID: %d, using default (xingkong)", theme);
        theme = EYE_THEME_XINGKONG;
    }

    return &theme_configs[theme];
}

// 获取主题名称
const char* GetEyeThemeName(EyeTheme theme) {
    const EyeThemeConfig* config = GetEyeThemeConfig(theme);
    return config->name;
}
