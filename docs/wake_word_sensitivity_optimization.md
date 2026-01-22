# 唤醒词灵敏度优化方案

## 🎯 问题分析

当前配置：
- ✅ 麦克风增益：40 dB（已经较高）
- ✅ 环境安静
- ✅ 普通话标准
- ❌ 但唤醒词检测还是不够灵敏

## 📊 从日志分析

```
I (13480) AFE: AFE Pipeline: [input] ->  -> |VAD(WebRTC)| -> |WakeNet(wn9_nihaoxiaoxin_tts,)| -> [output]
I (13480) AfeWakeWord: Audio detection task started, feed size: 512 fetch size: 512
```

**关键参数**：
- Feed size: 512 samples (16kHz 下约 32ms)
- WakeNet 模型: `wn9_nihaoxiaoxin_tts`
- 检测模式: 默认（未明确设置阈值）

## 💡 优化方案

### 方案 1：提高麦克风增益到 45-50 dB（推荐）⭐

虽然您说增益已经是 40dB，但可以**再提高一些**。

**修改文件**：[main/audio/audio_codec.h](main/audio/audio_codec.h)

```cpp
// 当前值
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 40.0

// 优化建议
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 45.0  // 提高到 45 dB
// 或者
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 50.0  // 提高到 50 dB（最高推荐值）
```

**注意事项**：
- 45 dB：通常不会有回声问题
- 50 dB：如果出现回声/啸叫，降回 45 dB
- > 50 dB：不推荐（可能导致失真或啸叫）

---

### 方案 2：降低 AFE 唤醒词检测阈值

AFE WakeNet 有一个内置的检测阈值，可以通过代码调整。

**修改文件**：[main/audio/wake_words/afe_wake_word.cc](main/audio/wake_words/afe_wake_word.cc)

在初始化部分添加阈值设置：

```cpp
// 在 afe_config_init 之后添加
afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);

// ⭐ 添加这行：设置检测阈值（0.0-1.0，越低越灵敏）
afe_config->wakenet_model_name = wakenet_model_;  // 原有代码
afe_config->det_threshold = 0.5;  // ⭐ 新增：默认约 0.6-0.7，降低到 0.5 更灵敏
// 或者更灵敏
afe_config->det_threshold = 0.4;  // ⭐ 非常灵敏（可能误触发）
```

**阈值建议**：
| 阈值 | 灵敏度 | 误触发风险 |
|-----|--------|-----------|
| 0.7 | 低 | 很低 |
| 0.6 | 中低 | 低 |
| 0.5 | 中等 | 中等（推荐）✅ |
| 0.4 | 高 | 高 |
| 0.3 | 很高 | 很高 |

---

### 方案 3：启用 AGC（自动增益控制）

AGC 可以自动调整音频增益，在说话声音小时自动提升。

**修改文件**：[main/audio/processors/afe_audio_processor.cc](main/audio/processors/afe_audio_processor.cc)

```cpp
// 第 52 行
afe_config->agc_init = false;  // 当前值

// 修改为
afe_config->agc_init = true;   // ⭐ 启用 AGC
```

**优点**：
- 自动适应说话音量
- 小声说话时自动提升增益
- 大声说话时自动降低增益

**缺点**：
- 可能增加轻微延迟
- 可能引入一些噪音放大

---

### 方案 4：优化 VAD 模式

VAD（语音活动检测）模式影响检测灵敏度。

**当前配置**：
```cpp
afe_config->vad_mode = VAD_MODE_0;  // 最灵敏
```

**已经是最灵敏模式**，无需修改。

---

### 方案 5：添加调试日志（诊断用）

添加详细的唤醒词检测日志，帮助诊断问题。

**修改文件**：[main/audio/wake_words/afe_wake_word.cc](main/audio/wake_words/afe_wake_word.cc)

在 `AudioDetectionTask` 函数中添加日志：

```cpp
void AfeWakeWord::AudioDetectionTask() {
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);
    ESP_LOGI(TAG, "Audio detection task started, feed size: %d fetch size: %d",
        feed_size, fetch_size);

    // ⭐ 添加检测阈值日志
    ESP_LOGI(TAG, "WakeNet detection threshold: %.2f", afe_config->det_threshold);

    while (true) {
        xEventGroupWaitBits(event_group_, DETECTION_RUNNING_EVENT, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = afe_iface_->fetch_with_delay(afe_data_, portMAX_DELAY);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            continue;;
        }

        // ⭐ 添加检测状态日志（每5秒输出一次）
        static int last_log_time = 0;
        int current_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        if (current_time - last_log_time > 5) {
            ESP_LOGI(TAG, "WakeWord detection running, confidence: %.2f",
                     res->wakeup_prob);  // 置信度
            last_log_time = current_time;
        }

        // ... 其余代码
    }
}
```

---

## 🔧 推荐的优化步骤

### 第 1 步：提高麦克风增益到 45 dB

**立即尝试，最简单**

```cpp
// main/audio/audio_codec.h:16
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 45.0
```

**测试**：
```bash
idf.py build flash monitor
```

**如果效果好**：✅ 完成！
**如果出现回声/啸叫**：降低到 42-43 dB

---

### 第 2 步：如果还不够，降低检测阈值

**修改 afe_audio_processor.cc**

```cpp
// main/audio/processors/afe_audio_processor.cc:34-36
afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
afe_config->vad_mode = VAD_MODE_0;
afe_config->det_threshold = 0.5;  // ⭐ 新增：降低检测阈值
```

---

### 第 3 步：启用 AGC（可选）

```cpp
// main/audio/processors/afe_audio_processor.cc:52
afe_config->agc_init = true;  // 从 false 改为 true
```

---

## 📊 预期效果

### 优化前
- 需要说 2-3 次"你好小鑫"才能检测到
- 距离 > 50cm 就检测不到
- 小声说话检测困难

### 优化后（预期）
- 说 1 次就能检测到 ✅
- 距离 1-1.5 米也能检测 ✅
- 小声说话也能检测 ✅

---

## ⚠️ 潜在风险

### 风险 1：误触发增加

降低阈值后，可能会出现误触发。

**解决**：
- 如果误触发频繁，提高阈值到 0.55-0.6
- 或者降低麦克风增益到 42-43 dB

### 风险 2：回声/啸叫

提高增益到 45-50 dB 后可能出现回声。

**解决**：
- 降低增益到 42-43 dB
- 启用 AGC 自动调整
- 物理上增加麦克风与扬声器距离

---

## 🎯 最佳实践建议

### 组合方案（推荐）

```
方案 A：保守优化
├─ 麦克风增益：45 dB
├─ 检测阈值：0.5
└─ AGC：禁用

方案 B：激进优化（最灵敏）⭐
├─ 麦克风增益：50 dB
├─ 检测阈值：0.4
└─ AGC：启用

方案 C：平衡优化（推荐）
├─ 麦克风增益：45 dB
├─ 检测阈值：0.45
└─ AGC：启用
```

---

## 🧪 测试方法

### 测试 1：距离测试

```
1. 距离 30cm：清晰说"你好小鑫"
   → 应该 100% 检测到

2. 距离 1m：正常音量说"你好小鑫"
   → 应该 90% 以上检测到

3. 距离 1.5m：正常音量说"你好小鑫"
   → 应该 70% 以上检测到
```

### 测试 2：音量测试

```
1. 大声说话：应该 100% 检测
2. 正常说话：应该 90% 检测
3. 小声说话：应该 70% 检测
```

### 测试 3：误触发测试

```
1. 不说话，保持安静 1 分钟
   → 不应该有误触发

2. 说类似词语："你好小新"、"你好小星"
   → 不应该误触发

3. 播放音乐时说话
   → 可能检测率下降（正常）
```

---

## 📝 总结

### 立即可用的优化

| 优化项 | 难度 | 效果 | 推荐度 |
|-------|------|------|--------|
| **提高增益到 45 dB** | ⭐ 简单 | ⭐⭐⭐⭐ | ✅ 强烈推荐 |
| **降低阈值到 0.5** | ⭐⭐ 中等 | ⭐⭐⭐⭐ | ✅ 推荐 |
| **启用 AGC** | ⭐ 简单 | ⭐⭐⭐ | ✅ 推荐 |

### 建议的优化顺序

1. **先尝试提高增益到 45 dB**（最简单，最有效）
2. **如果还不够，添加阈值设置到 0.5**
3. **最后考虑启用 AGC**

### 期望效果

优化后，唤醒词检测率应该从当前的约 50% 提升到 **80-90%**。

---

**版本**: v0.3.0
**更新时间**: 2025-01-05
