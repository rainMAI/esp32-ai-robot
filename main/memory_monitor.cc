#include "memory_monitor.h"
#include "system_info.h"

#include <esp_log.h>
#include <map>
#include <string>

#define TAG "MemoryMonitor"

// 内存事件名称映射
static const char* event_names[] = {
    "STARTUP",
    "WAKE_WORD",
    "AUDIO_CHANNEL",
    "UDP_SEND",
    "TTS_START",
    "TTS_END",
    "ERROR"
};

void MemoryMonitor::LogEvent(MemoryEvent event, const char* detail) {
    // 获取当前内存状态
    int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    int free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    size_t largest_dma_block = heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    // 打印事件信息
    ESP_LOGI(TAG, "========== 事件: %s ==========", event_names[event]);
    if (detail) {
        ESP_LOGI(TAG, "详情: %s", detail);
    }

    // 关键内存指标
    ESP_LOGI(TAG, "内部 SRAM: %d KB (最小: %d KB)", free_sram / 1024, min_free_sram / 1024);
    ESP_LOGI(TAG, "DMA 内存:  %d KB [最大连续: %d KB]", free_dma / 1024, largest_dma_block / 1024);

    // 警告检查
    if (free_dma < 20480) {  // DMA 内存少于 20KB
        ESP_LOGW(TAG, "⚠️  DMA 内存不足！可能影响 UDP/WiFi 操作");
    }

    if (free_sram < 30720) {  // 内部 SRAM 少于 30KB
        ESP_LOGW(TAG, "⚠️  内部 SRAM 严重不足！系统可能不稳定");
    }

    ESP_LOGI(TAG, "====================================");
}

void MemoryMonitor::PrintSummary() {
    SystemInfo::PrintHeapStats();
}

void MemoryMonitor::GetLastStatus(size_t* free_sram, size_t* min_sram) {
    if (free_sram) {
        *free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    }
    if (min_sram) {
        *min_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    }
}
