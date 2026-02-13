#ifndef _MEMORY_MONITOR_H_
#define _MEMORY_MONITOR_H_

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

// 内存监控事件类型
enum MemoryEvent {
    MEM_EVENT_STARTUP,           // 启动时
    MEM_EVENT_WAKE_WORD,         // 唤醒词检测
    MEM_EVENT_AUDIO_CHANNEL,     // 音频通道打开
    MEM_EVENT_UDP_SEND,          // UDP 发送
    MEM_EVENT_TTS_START,         // TTS 开始
    MEM_EVENT_TTS_END,           // TTS 结束
    MEM_EVENT_ERROR,             // 错误发生
};

class MemoryMonitor {
public:
    // 记录内存事件
    static void LogEvent(MemoryEvent event, const char* detail = nullptr);

    // 打印内存状态摘要
    static void PrintSummary();

    // 获取上次的内存状态
    static void GetLastStatus(size_t* free_sram, size_t* min_sram);
};

#endif // _MEMORY_MONITOR_H_
