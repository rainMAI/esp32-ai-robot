# 内存监控增强指南

## 概述

为了更好地调试内存问题（特别是 errno=12 错误），我们增强了系统的内存监控功能，在关键事件时自动记录详细的内存状态。

## 新增功能

### 1. 增强的 `SystemInfo::PrintHeapStats()`

**文件**: [main/system_info.cc](../main/system_info.cc#L141)

现在打印更详细的内存信息：

```
========== 内存状态 ==========
内部 SRAM:     XX KB (最小:   XX KB) [最大连续块:   XX KB]
DMA 内存:      XX KB (最小:   XX KB) [最大连续块:   XX KB]
PSRAM:        XXX KB (最小:  XXX KB)
总堆内存:     XXX KB (最小:  XXX KB)
==============================
```

**关键字段说明**:
- **内部 SRAM**: 可用的内部内存总量
- **DMA 内存**: WiFi 驱动需要的 DMA 能力内存（errno=12 关键指标）
- **最大连续块**: 用于检测内存碎片化
- **最小**: 历史最低值，表示系统曾达到的最紧张状态

### 2. 内存监控工具类 `MemoryMonitor`

**文件**:
- [main/memory_monitor.h](../main/memory_monitor.h)
- [main/memory_monitor.cc](../main/memory_monitor.cc)

**功能**:
- 在关键事件时自动记录内存状态
- 提供 DMA 内存不足警告
- 显示事件相关的详细信息

**支持的事件类型**:
```cpp
enum MemoryEvent {
    MEM_EVENT_STARTUP,           // 启动时
    MEM_EVENT_WAKE_WORD,         // 唤醒词检测
    MEM_EVENT_AUDIO_CHANNEL,     // 音频通道打开
    MEM_EVENT_UDP_SEND,          // UDP 发送
    MEM_EVENT_TTS_START,         // TTS 开始
    MEM_EVENT_TTS_END,           // TTS 结束
    MEM_EVENT_ERROR,             // 错误发生
};
```

**使用示例**:
```cpp
#include "memory_monitor.h"

// 记录事件
MemoryMonitor::LogEvent(MEM_EVENT_WAKE_WORD, "你好小鑫");

// 打印摘要
MemoryMonitor::PrintSummary();
```

### 3. 唤醒词检测时的内存监控

**文件**: [main/audio/wake_words/afe_wake_word.cc](../main/audio/wake_words/afe_wake_word.cc#L144)

现在每次检测到唤醒词时，会自动输出：

```
I (xxxx) AfeWakeWord: ⭐ WakeWord DETECTED! Word: 你好小鑫
I (xxxx) MemoryMonitor: ========== 事件: WAKE_WORD ==========
I (xxxx) MemoryMonitor: 详情: 唤醒词: 你好小鑫
I (xxxx) MemoryMonitor: 内部 SRAM: 66 KB (最小: 60 KB)
I (xxxx) MemoryMonitor: DMA 内存:  66 KB [最大连续:  60 KB]
I (xxxx) MemoryMonitor: ====================================
```

## 警告阈值

系统会在以下情况下发出警告：

| 指标 | 阈值 | 警告信息 | 含义 |
|------|------|----------|------|
| DMA 内存 | < 20KB | ⚠️ DMA 内存不足！可能影响 UDP/WiFi 操作 | errno=12 风险高 |
| 内部 SRAM | < 30KB | ⚠️ 内部 SRAM 严重不足！系统可能不稳定 | 系统不稳定风险 |

## 定时监控

系统每 10 秒自动打印一次内存状态（在 `application.cc` 的 `OnClockTimer()` 中）。

## 调试流程

### 1. 查看当前内存状态

从日志中找到 `========== 内存状态 ==========` 部分，关注：

1. **DMA 内存** - 如果低于 20KB，UDP 发送很可能失败
2. **最大连续块** - 如果远小于总 DMA 内存，说明碎片化严重
3. **最小值** - 历史最低点，表示系统曾经的最紧张状态

### 2. 定位内存泄漏

对比不同时间点的内存状态：

```
启动时:    DMA 内存:  80 KB (最小:  80 KB) [最大连续:  80 KB]
唤醒词:   DMA 内存:  60 KB (最小:  60 KB) [最大连续:  60 KB]
UDP 发送: DMA 内存:  20 KB (最小:  20 KB) [最大连续:  15 KB]  ⚠️
```

### 3. errno=12 问题诊断

如果看到大量 `Send failed: ret=-1, errno=12` 错误：

1. 查找错误发生前最近的内存监控输出
2. 检查 **DMA 内存** 和 **最大连续块** 的值
3. 如果 DMA 内存 < 20KB 或最大连续块 < 10KB：
   - 说明内存不足或碎片化严重
   - 检查是否有内存泄漏
   - 考虑优化内存配置（`sdkconfig.defaults`）

## 内存配置优化

检查 [sdkconfig.defaults](../sdkconfig.defaults) 中的关键配置：

```ini
# PSRAM 分配策略（影响内部 SRAM 可用量）
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=512
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768  ← 应该是 32768 (32KB)

# WiFi 缓冲区（影响 DMA 内存消耗）
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=10
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=16
```

验证：
```bash
grep CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL sdkconfig
# 应该输出: CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
```

如果看到 65536 (64KB)，说明配置被覆盖，需要：
```bash
rm sdkconfig sdkconfig.old
idf.py build
```

## 常见内存状态示例

### 正常状态
```
========== 内存状态 ==========
内部 SRAM:     66 KB (最小:   60 KB) [最大连续块:   60 KB]
DMA 内存:      66 KB (最小:   60 KB) [最大连续块:   60 KB]
PSRAM:       8000 KB (最小: 7800 KB)
总堆内存:    8066 KB (最小: 7860 KB)
==============================
```
✅ 系统健康，DMA 内存充足

### 警告状态
```
========== 内存状态 ==========
内部 SRAM:     25 KB (最小:   20 KB) [最大连续块:   18 KB]
DMA 内存:      15 KB (最小:   12 KB) [最大连续块:   10 KB]
PSRAM:       7900 KB (最小: 7500 KB)
总堆内存:    7925 KB (最小: 7520 KB)
==============================
W (xxxx) MemoryMonitor: ⚠️  DMA 内存不足！可能影响 UDP/WiFi 操作
```
⚠️ DMA 内存不足，可能出现 errno=12 错误

### 危险状态
```
========== 内存状态 ==========
内部 SRAM:     18 KB (最小:   15 KB) [最大连续块:    8 KB]
DMA 内存:       8 KB (最小:    5 KB) [最大连续块:    3 KB]
PSRAM:       7800 KB (最小: 7200 KB)
总堆内存:    7818 KB (最小: 7215 KB)
==============================
W (xxxx) MemoryMonitor: ⚠️  DMA 内存不足！可能影响 UDP/WiFi 操作
W (xxxx) MemoryMonitor: ⚠️  内部 SRAM 严重不足！系统可能不稳定
```
🚨 系统极不稳定，很可能崩溃或无法正常通信

## 文件清单

新增/修改的文件：

1. ✅ [main/system_info.cc](../main/system_info.cc) - 增强的内存状态打印
2. ✅ [main/memory_monitor.h](../main/memory_monitor.h) - 内存监控工具类声明
3. ✅ [main/memory_monitor.cc](../main/memory_monitor.cc) - 内存监控工具类实现
4. ✅ [main/audio/wake_words/afe_wake_word.cc](../main/audio/wake_words/afe_wake_word.cc) - 集成内存监控
5. ✅ [main/CMakeLists.txt](../main/CMakeLists.txt) - 添加 memory_monitor.cc 到构建

## 下一步

编译并烧录后，观察日志输出：

1. **启动时** - 查看初始内存状态
2. **唤醒词检测时** - 查看事件相关的内存变化
3. **每 10 秒** - 定时内存快照
4. **错误发生时** - 快速定位内存问题

这些信息将帮助你快速诊断内存不足的根本原因。
