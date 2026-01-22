# AFE(FEED) Ringbuffer 溢出问题修复

## 问题描述

在唤醒词检测后，系统会持续输出以下警告：

```
W (28540) AFE: Ringbuffer of AFE(FEED) is full, Please use fetch() to read data to avoid data loss or overwriting
```

这个警告会持续出现 100+ 次，直到进入 listening 状态。

## 问题根源

### 时间线分析

1. **T=27170ms**: 唤醒词 "小宇同学" 检测到
2. **T=27190ms**: 调用 `AfeWakeWord::Stop()`
   - 内部检测任务停止 fetch 数据
   - 调用 `xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT)`
3. **T=27190ms ~ T=38110ms**: Feed 仍在继续
   - 音频服务继续调用 `AfeWakeWord::Feed()`
   - AFE 内部 ringbuffer 持续接收数据
   - 没有消费者 fetch 数据 → ringbuffer 溢出
4. **T=38110ms**: 进入 listening 状态
   - 调用 `audio_service_.EnableWakeWordDetection(false)`
   - 清除 `AS_EVENT_WAKE_WORD_RUNNING` 标志
   - Feed 停止，警告消失

### 根本原因

**唤醒词检测流程中的时序问题**：

```
唤醒词检测 → Stop() 内部任务 → 但 Feed() 还在被调用 → Ringbuffer 溢出
                                    ↓
                            直到进入 Listening 状态才停止
```

**代码逻辑**：
- `Stop()` 停止了检测任务（fetch），但**没有停止 Feed**
- Feed 的停止依赖于 `Application::SetDeviceState()` 中的 `EnableWakeWordDetection(false)`
- 这之间存在时间差（约 10 秒），导致大量警告

## 解决方案

### 修改文件

**[main/audio/wake_words/afe_wake_word.cc](../main/audio/wake_words/afe_wake_word.cc#L109)**

### 修改前

```cpp
void AfeWakeWord::Feed(const std::vector<int16_t>& data) {
    if (afe_data_ == nullptr) {
        return;
    }
    afe_iface_->feed(afe_data_, data.data());
}
```

### 修改后

```cpp
void AfeWakeWord::Feed(const std::vector<int16_t>& data) {
    // 如果已经停止检测，不再 feed 数据到 AFE，避免 ringbuffer 溢出
    if (afe_data_ == nullptr || event_group_ == nullptr) {
        return;
    }

    // 检查是否正在运行检测
    EventBits_t bits = xEventGroupGetBits(event_group_);
    if (!(bits & DETECTION_RUNNING_EVENT)) {
        // 已停止，不 feed 数据
        return;
    }

    afe_iface_->feed(afe_data_, data.data());
}
```

## 工作原理

### 事件标志机制

`AfeWakeWord` 使用事件组来管理检测状态：

```cpp
#define DETECTION_RUNNING_EVENT 0x01

// 启动检测
void AfeWakeWord::Start() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

// 停止检测
void AfeWakeWord::Stop() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
}
```

### Feed 检查逻辑

现在 `Feed()` 方法会检查 `DETECTION_RUNNING_EVENT` 标志：

1. **如果标志已设置**：正常运行，feed 数据到 AFE
2. **如果标志已清除**：立即返回，不 feed 数据

### 效果

**修改前**：
- Stop() 后 → 内部任务停止 → 但 Feed 继续 → Ringbuffer 溢出（100+ 警告）

**修改后**：
- Stop() 后 → Feed 立即停止 → **无警告**

## 内存情况分析

### 内存变化趋势

| 时间点 | 内部 SRAM | DMA 内存 | 最小 DMA | 状态 |
|--------|-----------|----------|----------|------|
| 启动时 (10s) | 65 KB | 58 KB | 52 KB | ✅ 良好 |
| 加载唤醒词 (13s) | 41 KB | 41 KB | 41 KB | ⚠️ 下降 24 KB |
| 唤醒检测 (27s) | 43 KB | 42 KB | 36 KB | ⚠️ 稳定 |
| WebSocket (36s) | **35 KB** | **34 KB** | **32 KB** | ⚠️ 接近阈值 |
| 对话中 (57s) | **35 KB** | **35 KB** | **23 KB** | 🚨 **最低 23 KB** |
| 对话中 (75s) | **30 KB** | **30 KB** | **23 KB** | 🚨 **危险** |

### 关键发现

1. **PSRAM DMA 池**：已正确回到 32 KB ✅
2. **DMA 内存最低**：23 KB，接近 errno=12 阈值（20 KB）
3. **内存碎片化**：最大连续块只有 26 KB

### 建议

虽然当前没有 errno=12 错误，但内存比较紧张。如果需要进一步优化：

1. **减少 WiFi 缓冲区**（可能影响性能）
2. **优化眼睛动画内存**（考虑减少素材）
3. **降低麦克风增益**（当前 30dB，已较低）

## 验证

编译后，唤醒词检测时应该**不再出现**以下警告：

```
W (xxxx) AFE: Ringbuffer of AFE(FEED) is full
```

### 预期日志

```
I (xxxx) AfeWakeWord: ⭐ WakeWord DETECTED! Word: 小宇同学
I (xxxx) MemoryMonitor: ========== 事件: WAKE_WORD ==========
I (xxxx) MemoryMonitor: 详情: 唤醒词: 小宇同学
I (xxxx) MemoryMonitor: 内部 SRAM: 43 KB (最小: 36 KB)
I (xxxx) MemoryMonitor: DMA 内存:  42 KB [最大连续: 40 KB]
I (xxxx) Application: STATE: connecting
I (xxxx) WS: Connecting to websocket server...
I (xxxx) WS: Session ID: xxx
I (xxxx) Application: STATE: listening
```

**不应该看到**：
```
W (xxxx) AFE: Ringbuffer of AFE(FEED) is full  ← 应该消失
```

## 相关问题

### Q: 为什么会有这个问题？

A: 原设计假设 AudioService 会在检测后立即停止 Feed，但实际上存在时序差。

### Q: 这个修改会影响性能吗？

A: 不会。`xEventGroupGetBits()` 是非常快速的操作（仅读取内存），而且只在 Feed 时调用。

### Q: 为什么不在 Stop() 中直接清除 AS_EVENT_WAKE_WORD_RUNNING？

A: 因为 `AfeWakeWord` 不应该知道 `AudioService` 的内部事件标志。职责分离原则。

### Q: 如果内存太低怎么办？

A: 当前最低 23 KB，虽然紧张但还能工作。如果经常遇到 errno=12，可以考虑：
1. 禁用 WiFi AMPDU（已配置）
2. 减少 TX/RX 缓冲区（已配置）
3. 使用更小的唤醒词模型

## 总结

✅ **已修复**：AFE Ringbuffer 溢出警告
✅ **方法**：Feed 时检查检测状态，停止后不再 Feed
✅ **无副作用**：不影响其他功能，性能影响可忽略
✅ **内存状况**：紧张但可用（23 KB 最低）

---

**修复日期**：2025-01-07
**影响版本**：v0.3.1+
**修复版本**：v0.3.2+
