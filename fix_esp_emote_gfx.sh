#!/bin/bash
# 修复 esp_emote_gfx 组件的格式化字符串错误
# 每次组件管理器更新后运行此脚本

# gfx_label.c
sed -i '13 i #include <inttypes.h>' managed_components/espressif2022__esp_emote_gfx/src/widget/gfx_label.c
sed -i 's/ESP_LOGI(TAG, "aligned_offset: %d, text_width: %d, scroll_offset: %d,/ESP_LOGI(TAG, "aligned_offset: %" PRId32 ", text_width: %" PRId32 ", scroll_offset: %" PRId32,/' managed_components/espressif2022__esp_emote_gfx/src/widget/gfx_label.c

# gfx_refr.c
sed -i '7 i #include <inttypes.h>' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_refr.c
sed -i 's/ESP_LOGD(TAG, "Merged area \[%d\] into \[%d\]/ESP_LOGD(TAG, "Merged area [%" PRIu32 "] into [%" PRIu32 "]/' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_refr.c
sed -i 's/ESP_LOGD(TAG, "Area already covered by existing dirty area %d"/ESP_LOGD(TAG, "Area already covered by existing dirty area %u"/' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_refr.c
sed -i 's/ESP_LOGD(TAG, "Added dirty area \[%d,%d,%d,%d\], total: %d"/ESP_LOGD(TAG, "Added dirty area [%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "], total: %" PRIu32"/' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_refr.c

# gfx_render.c
sed -i '7 i #include <string.h>\n#include <inttypes.h>' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_render.c
sed -i 's/ESP_LOGD(TAG, "Draw area \[%d\]: (\(%d,%d\)->(\(%d,%d\) %dx%d/ESP_LOGD(TAG, "Draw area [%u]: (%" PRId32 ",%" PRId32 ")->(%" PRId32 ",%" PRId32 ") %" PRId32 "x%" PRId32/' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_render.c
sed -i 's/ESP_LOGE(TAG, "Area\[%d\] width %lu/ESP_LOGE(TAG, "Area[%u] width %lu/' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_render.c
sed -i 's/ESP_LOGD(TAG, "Flush\[%lu\]: (\(%d,%d\)->(\(%d,%d\) %lupx/ESP_LOGD(TAG, "Flush[%" PRIu32 "]: (%" PRId32 ",%" PRId32 ")->(%" PRId32 ",%" PRId32 ") %" PRIu32 "px/' managed_components/espressif2022__esp_emote_gfx/src/core/gfx_render.c

echo "esp_emote_gfx 格式化字符串错误已修复！"
