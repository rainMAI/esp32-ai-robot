# 提醒功能音频 Timestamp 修复

## 问题描述

提醒到时间后，**服务器不返回 TTS 音频**。对比 14-handheld 项目（工作正常）发现关键差异。

## 日志分析

### eyes 项目（修复前）

```
I (212810) Protocol: Sending reminder: 去给萱萱收碗
I (212830) Application: ProcessReminderTts: 准备回传提醒音频
I (212880) Application: Streaming PCM data from TTS service...
I (222940) Application: Reminder audio upload finished, total 141346 bytes
（之后没有服务器返回 TTS 的日志）❌
```

### 14-handheld 项目（正常）

```
I (xxxx) Protocol: Sending reminder: ...
I (xxxx) Application: ProcessReminderTts: 准备回传提醒音频
I (xxxx) Application: Streaming PCM data to AudioService for upload...
I (xxxx) Application: Reminder audio upload finished
I (xxxx) Application: << 时间到了，该去给萱萱收碗了，记得准时哦。✅
```

## 根本原因

**关键差异**：音频上传时缺少 `timestamp` 参数

### eyes 修复前

```cpp
// main/audio/audio_service.h:109
void PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm);
//                                                                   ^^^^^^^^^^
//                                                                   没有 timestamp

// main/application.cc:960
audio_service.PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(pcm));
//                                                                           ^^^^^^^
//                                                                           没有传 timestamp ❌
```

**问题**：
- 上传的音频数据没有 timestamp
- 服务器无法正确识别音频数据
- 服务器不返回 TTS 响应

### 14-handheld（正常）

```cpp
// main/chat/audio/audio_service.h
void PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm,
                            uint32_t timestamp = 0xFFFFFFFF);
//                                                                   ^^^^^^^^^^^^^^^^^^^
//                                                                   有 timestamp 参数

// main/chat/application.cc
audio_service.PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue,
                                    std::move(pcm), timestamp);
//                                                        ^^^^^^^^^
//                                                        传递 timestamp ✅
```

## 修复方案

### 1. 修改函数签名（添加 timestamp 参数）

**文件**: [main/audio/audio_service.h:109](../main/audio/audio_service.h#L109)

```diff
- void PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm);
+ void PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm,
+                             uint32_t timestamp = 0xFFFFFFFF);
```

### 2. 修改函数实现（支持 timestamp）

**文件**: [main/audio/audio_service.cc:408](../main/audio/audio_service.cc#L408)

```diff
  void AudioService::PushTaskToEncodeQueue(AudioTaskType type,
-                                          std::vector<int16_t>&& pcm) {
+                                          std::vector<int16_t>&& pcm,
+                                          uint32_t timestamp) {
      auto task = std::make_unique<AudioTask>();
      task->type = type;
      task->pcm = std::move(pcm);

      std::unique_lock<std::mutex> lock(audio_queue_mutex_);

      /* If the task is to send queue, we need to set the timestamp */
      if (type == kAudioTaskTypeEncodeToSendQueue) {
+         if (timestamp != 0xFFFFFFFF) {
+             // 使用传入的 timestamp（用于提醒等特殊场景）
+             task->timestamp = timestamp;
+         } else if (!timestamp_queue_.empty()) {
              // 使用队列中的 timestamp（正常对话场景）
              if (timestamp_queue_.size() <= MAX_TIMESTAMPS_IN_QUEUE) {
                  task->timestamp = timestamp_queue_.front();
              } else {
                  ESP_LOGW(TAG, "Timestamp queue (%u) is full, dropping timestamp",
                            timestamp_queue_.size());
              }
              timestamp_queue_.pop_front();
+         }
      }

      audio_queue_cv_.wait(lock, [this]() {
          return audio_encode_queue_.size() < MAX_ENCODE_TASKS_IN_QUEUE;
      });
      audio_encode_queue_.push_back(std::move(task));
      audio_queue_cv_.notify_all();
  }
```

**说明**：
- `timestamp != 0xFFFFFFFF`：使用传入的 timestamp（提醒场景）
- `timestamp == 0xFFFFFFFF`：使用队列中的 timestamp（正常对话）
- 默认值 `0xFFFFFFFF` 确保向后兼容

### 3. 更新调用（传递 timestamp）

**文件**: [main/application.cc:960](../main/application.cc#L960)

```diff
  // [Step 2] 注入编码队列上传至服务器
- audio_service.PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(pcm));
+ audio_service.PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue,
+                                      std::move(pcm), timestamp);
```

## 技术细节

### Timestamp 的作用

1. **音频同步**：timestamp 告诉服务器每帧音频的时间戳
2. **数据完整性**：服务器依赖 timestamp 来正确组装音频流
3. **唤醒识别**：服务器可能用 timestamp 来判断这是"唤醒词后的音频"

### 为什么提醒需要特殊处理

**正常对话**：
```
用户说话 → 麦克风采集 → timestamp_queue → PushTaskToEncodeQueue
                                      ↓
                                    自动从队列取 timestamp
```

**提醒场景**：
```
TTS 服务生成音频 → ProcessReminderTts → PushTaskToEncodeQueue(PCM, timestamp)
                                                      ↑
                                               手动传入 timestamp
                                               （从 TTS 服务获取）
```

## 预期效果（修复后）

### 完整提醒流程

```
I (212810) Application: Alert INFO: 去给萱萱收碗 [bell]
I (212810) AudioService: Playing sound: popup.ogg  ← 播放提示音 ✅
I (212810) Protocol: Sending reminder: 去给萱萱收碗
I (212820) Application: Received reminder from queue
I (212830) Application: ProcessReminderTts: 准备回传提醒音频
I (212880) Application: Streaming PCM data from TTS service...
I (222940) Application: Reminder audio upload finished, total 141346 bytes
I (223000) Application: << 时间到了，该去给萱萱收碗了，记得准时哦。  ← TTS ✅
```

### 用户听到

1. **"叮！"** - 本地提示音
2. **弹窗显示**："去给萱萱收碗" [铃铛图标]
3. **TTS 播放**："时间到了，该去给萱萱收碗了，记得准时哦。"

## 修复文件清单

1. ✅ [main/audio/audio_service.h:109](../main/audio/audio_service.h#L109) - 添加 timestamp 参数
2. ✅ [main/audio/audio_service.cc:408](../main/audio/audio_service.cc#L408) - 支持 timestamp 逻辑
3. ✅ [main/application.cc:560](../main/application.cc#L560) - 添加提示音（已修复）
4. ✅ [main/application.cc:960](../main/application.cc#L960) - 传递 timestamp（新修复）

## 验证步骤

编译并烧录后，测试提醒：

```
你："15秒后提醒我去给萱萱收碗"
设备："提醒已设置！15秒后我会提醒您去给萱萱收碗哦～ ⏰"

（15秒后）

设备："叮！" + 弹窗显示
设备："时间到了，该去给萱萱收碗了，记得准时哦。"
```

**应该听到**：
1. ✅ "叮" 提示音（`OGG_POPUP`）
2. ✅ TTS 语音（服务器返回）

## 总结

### 两个关键修复

1. **本地提示音**：`Alert(..., "bell", Lang::Sounds::OGG_POPUP)`
2. **Timestamp 参数**：`PushTaskToEncodeQueue(..., timestamp)`

### 对比 14-handheld

| 项目 | 提示音 | Timestamp | 服务器 TTS |
|------|--------|------------|-----------|
| 14-handheld | ✅ | ✅ | ✅ |
| eyes (修复前) | ❌ | ❌ | ❌ |
| eyes (修复后) | ✅ | ✅ | ✅ |

### 根本原因

**提醒音频没有 timestamp** → 服务器无法识别音频 → 不返回 TTS

现在修复了，服务器应该能正常返回 TTS 了！

---

**修复日期**：2025-01-07
**影响版本**：v0.3.2+
**参考项目**：14-handheld（工作正常的参考实现）
