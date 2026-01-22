# 14-handheld 修复同步汇总报告 (带 Diff 详情)

为了方便后续回溯与测试，本报告详细记录了从核心项目同步到 `14-handheld` 项目的具体代码变更。

---

## 1. 🛠️ 日志格式修复 (Log Formatting)

**修复说明**：解决了 ESP-IDF 下 `%lld` 无法识别导致打印 `ld` 的问题。

### 文件：`main/chat/mcp_server.cc`
```diff
-                ESP_LOGI(TAG, "Adding relative reminder: %s (delay: %d, now: %lld, target: %lld)", 
-                         content.c_str(), delay, (long long)::time(nullptr), timestamp);
+                ESP_LOGI(TAG, "Adding relative reminder: %s (delay: %d, now: %ld, target: %ld)", 
+                         content.c_str(), delay, (long)::time(nullptr), (long)timestamp);
             } else if (ts > 0) {
                 timestamp = ts;
-                ESP_LOGI(TAG, "Adding absolute reminder: %s (ts: %lld, now: %lld)", 
-                         content.c_str(), timestamp, (long long)::time(nullptr));
+                ESP_LOGI(TAG, "Adding absolute reminder: %s (ts: %ld, now: %ld)", 
+                         content.c_str(), (long)timestamp, (long)::time(nullptr));
```

### 文件：`main/chat/reminder_manager.cc`
```diff
-    ESP_LOGI(TAG, "Added reminder: %s at %lld (now: %lld)", content.c_str(), (long long)timestamp, (long long)std::time(nullptr));
+    ESP_LOGI(TAG, "Added reminder: %s at %ld (now: %ld)", content.c_str(), (long)timestamp, (long)std::time(nullptr));
```

---

## 2. 🔊 音频回传重构 (Reminder TTS Backhaul)

**修复说明**：引入帧对齐缓冲区和硬件时钟同步，消除杂音并纠正 slow-motion（0.75x）语速问题。

### 文件：`main/chat/application.cc` (ProcessReminderTts)

````carousel
```diff
+    // [修改亮点 1: 文案美化与清洗]
-    if (content.find("去") == 0 || content.find("到") == 0 || content.find("做") == 0) {
-        text = "时间到了，提醒" + content + "了，记得准时哦。";
-    } else if (content.find("练习") != std::string::npos || content.find("口语") != std::string::npos || 
-               content.find("学习") != std::string::npos || content.find("英语") != std::string::npos) {
-        text = "到时间" + content + "了，要不要我们现在来进行互动练习呢？";
-    } else {
-        text = "时间到了，提醒" + content + "了，记得准时哦。";
-    }
+    std::string clean_content = content;
+    if (clean_content.find("去") == 0) {
+        clean_content = clean_content.substr(3);
+    }
+    if (clean_content.find("练习") != std::string::npos || ...) {
+        text = "时间" + clean_content + "到了，要不要我们现在来进行互动练习呢？";
+    } else {
+        text = "时间到了，提醒该" + clean_content + "了，记得准时哦。";
+    }
```
<!-- slide -->
```diff
+    // [修改亮点 2: 帧对齐与时钟同步算法]
-    const size_t FRAME_SIZE = 960;
-    const size_t FRAME_BYTES = FRAME_SIZE * 2;
-    auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[FRAME_BYTES]);
-    while (true) {
-        int bytes_read = http->Read((char*)buffer.get(), FRAME_BYTES);
-        // ... 直接发送 bytes_read (可能不满一帧，导致碎包噪音)
-        audio_service.PushTaskToEncodeQueue(..., std::move(pcm), timestamp);
-        vTaskDelay(pdMS_TO_TICKS(60)); // 固定延时，未考虑逻辑开销
-    }
+    std::vector<int16_t> pcm_buffer; // 蓄水池缓存
+    int64_t start_time = esp_timer_get_time();
+    while (true) {
+        int bytes_read = http->Read(...);
+        pcm_buffer.insert(pcm_buffer.end(), samples_ptr, ...);
+        while (pcm_buffer.size() >= PACKET_SAMPLES) {
+            // 满 60ms 才发送
+            audio_service.PushTaskToEncodeQueue(...);
+            // 时钟同步：计算差值延时
+            if (expected_elapsed > actual_elapsed) {
+                vTaskDelay(pdMS_TO_TICKS((expected_elapsed - actual_elapsed) / 1000));
+            }
+        }
+    }
+    protocol_->SendStopListening(); // 主动告知服务器结束，触发秒回
```
````

---

## 3. 同步状态表

| 文件路径 | 状态 | 核心逻辑 |
| :--- | :--- | :--- |
| `main/chat/application.cc` | ✅ 已同步 | 帧对齐缓存 + `esp_timer` 采样平衡 |
| `main/chat/mcp_server.cc` | ✅ 已同步 | 日志类型强转 `(long)` |
| `main/chat/reminder_manager.cc` | ✅ 已同步 | 日志类型强转 `(long)` |

> [!IMPORTANT]
> 本次同步严格排除了生命周期管理和任务栈优化的修改，以维持 `11-handheld` 的架构独立性。

---
**报告更新日期**：2026-01-08
