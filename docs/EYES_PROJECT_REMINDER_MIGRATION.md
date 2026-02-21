# Eyes 项目提醒功能移植指南

本文档详细说明如何将提醒功能从 `14-handheld` 项目移植到 `eyes` 项目（`D:\code\eyes`）。

---

## 项目分析

### 目标项目结构

```
D:\code\eyes\
├── main\
│   ├── application.h / .cc       # 主应用程序 ✅ 已存在
│   ├── protocols\
│   │   ├── protocol.h            # 协议头文件 ✅ 已存在
│   │   ├── protocol.cc           # 协议实现 ✅ 已存在
│   │   ├── websocket_protocol.cc
│   │   └── mqtt_protocol.cc
│   ├── mcp_server.h / .cc         # MCP 服务器 ✅ 已存在
│   ├── settings.h / .cc           # NVS 存储 ✅ 已存在
│   ├── audio\
│   │   └── audio_service.h        # 音频服务 ✅ 已存在
│   └── CMakeLists.txt             # 构建配置 ✅ 已存在
```

### 兼容性评估

| 组件 | 源项目 | 目标项目 | 兼容性 | 说明 |
|-----|-------|---------|--------|------|
| Application 类 | ✅ 存在 | ✅ 存在 | ✅ 兼容 | 结构类似 |
| Protocol 类 | ✅ 存在 | ✅ 存在 | ✅ 兼容 | 接口一致 |
| MCP Server | ✅ 存在 | ✅ 存在 | ✅ 兼容 | 基于 xiaozhi |
| Settings | ✅ 存在 | ✅ 存在 | ✅ 兼容 | NVS 封装 |
| Audio Service | ✅ 存在 | ✅ 存在 | ✅ 兼容 | 包含 Opus 编码 |
| 定时器 | esp_timer | esp_timer | ✅ 兼容 | 相同 |

**结论**: ✅ **高度兼容，可以直接移植！**

---

## 移植步骤

### 步骤 1: 复制提醒管理器文件

```bash
# 从源项目复制
cp D:\code\14-handheld\main\chat\reminder_manager.h D:\code\eyes\main\
cp D:\code\14-handheld\main\chat\reminder_manager.cc D:\code\eyes\main\
```

---

### 步骤 2: 修改 application.h

打开 `D:\code\eyes\main\application.h`，在 `Application` 类中添加以下内容：

#### 2.1 添加私有成员变量

在 `private` 部分（约第 80 行附近）添加：

```cpp
private:
    // ... 现有成员变量 ...

    bool has_server_time_ = false;
    bool aborted_ = false;
    int clock_ticks_ = 0;

    // ========== 新增：提醒 TTS 任务 ==========
    TaskHandle_t reminder_tts_task_handle_ = nullptr;
    QueueHandle_t reminder_queue_ = nullptr;
    static int64_t g_last_channel_open_time_;  // 用于跟踪通道打开时间

    // ... 其他成员变量 ...
};
```

#### 2.2 添加公共方法

在 `public` 部分（约第 43 行附近）添加：

```cpp
public:
    // ... 现有方法 ...

    // ========== 新增：提醒 TTS 接口 ==========
    void QueueReminderTts(const std::string& content);

    // ... 其他方法 ...
};
```

#### 2.3 添加私有方法

在 `private` 部分的方法声明区域（约第 150 行之后）添加：

```cpp
private:
    // ... 现有方法声明 ...

    // ========== 新增：提醒 TTS 实现 ==========
    void ReminderTtsTask();
    void ProcessReminderTts(const std::string& content);

    // ... 其他方法声明 ...
};
```

**完整修改预览**：

```diff
--- a/main/application.h
+++ b/main/application.h
@@ -40,6 +40,9 @@ public:
     void PlaySound(const std::string_view& sound);
     AudioService& GetAudioService() { return audio_service_; }

+    // ========== 新增：提醒 TTS 接口 ==========
+    void QueueReminderTts(const std::string& content);
+
 private:
     Application();
     ~Application();
@@ -79,6 +82,11 @@ private:
     bool has_server_time_ = false;
     bool aborted_ = false;
     int clock_ticks_ = 0;
+
+    // ========== 新增：提醒 TTS 任务 ==========
+    TaskHandle_t reminder_tts_task_handle_ = nullptr;
+    QueueHandle_t reminder_queue_ = nullptr;
+    static int64_t g_last_channel_open_time_;
+
     TaskHandle_t check_new_version_task_handle_ = nullptr;

     void MainEventLoop();
@@ -150,6 +158,10 @@ private:
     void ShowActivationCode(const std::string& code, const std::string& message);
     void OnClockTimer();
     void SetListeningMode(ListeningMode mode);
+    // ========== 新增：提醒 TTS 实现 ==========
+    void ReminderTtsTask();
+    void ProcessReminderTts(const std::string& content);
 };

 #endif // _APPLICATION_H_
```

---

### 步骤 3: 修改 application.cc

打开 `D:\code\eyes\main\application.cc`，进行以下修改：

#### 3.1 添加头文件

在文件顶部（约第 20 行附近）添加：

```cpp
#include "reminder_manager.h"  // ========== 新增 ==========
```

#### 3.2 在 `Start()` 方法中初始化队列和任务

找到 `Application::Start()` 方法，在音频服务初始化之后添加：

```cpp
void Application::Start() {
    // ... 现有代码 ...

    audio_service_.SetCallbacks(callbacks);

    // ========== 新增：创建提醒 TTS 任务和队列 ==========
    reminder_queue_ = xQueueCreate(5, sizeof(std::string));
    if (reminder_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create reminder queue");
    } else {
        // 12KB 栈：4KB缓冲区 + 2KB HTTP + 2KB 编码/发送 + 2KB 重采样器
        xTaskCreate([](void* arg) {
            Application* app = static_cast<Application*>(arg);
            app->ReminderTtsTask();
            vTaskDelete(NULL);
        }, "reminder_tts", 12288, this, 3, &reminder_tts_task_handle_);
    }

    // ... 其他代码 ...
}
```

#### 3.3 在 `OnClockTimer()` 中添加提醒检查

找到 `Application::OnClockTimer()` 方法，在现有逻辑之后添加：

```cpp
void Application::OnClockTimer() {
    clock_ticks_++;

    // ... 现有时间更新逻辑 ...

    // ========== 新增：检查到期提醒 ==========
    ReminderManager::GetInstance().ProcessDueReminders([this](const Reminder& reminder) {
        // 1. 本地弹窗提示
        Alert("INFO", reminder.content.c_str(), "", "");
        reminder.local_alert_shown = true;

        // 2. 检查音频通道
        if (protocol_ && protocol_->IsAudioChannelOpened()) {
            // 等待通道稳定（唤醒后至少 1 秒）
            if (esp_timer_get_time() - g_last_channel_open_time_ < 1000000) {
                return false;
            }

            // 如果正在说话，等待完成
            if (device_state_ == kDeviceStateSpeaking) {
                return false;
            }

            // 3. 发送提醒
            protocol_->SendReminder(reminder.content);

            // 4. 设置为 Listening 状态
            SetListeningMode(kListeningModeAutoStop);

            return true;  // 已处理，移除提醒
        }

        // 如果通道未打开，尝试打开
        if (device_state_ == kDeviceStateIdle) {
            SetDeviceState(kDeviceStateConnecting);
            protocol_->OpenAudioChannel();
            return false;
        }

        return false;
    });
}
```

**注意**：eyes 项目的 `Alert()` 方法签名可能不同，请根据实际情况调整参数。

#### 3.4 在文件末尾添加 TTS 实现方法

在 `application.cc` 文件的最后（在最后的 `}` 之前）添加：

```cpp
// ========== 新增：提醒 TTS 实现 ==========

void Application::QueueReminderTts(const std::string& content) {
    if (reminder_queue_ != nullptr) {
        xQueueSend(reminder_queue_, &content, portMAX_DELAY);
    }
}

void Application::ReminderTtsTask() {
    ESP_LOGI(TAG, "Reminder TTS task started");

    std::string content;
    while (true) {
        if (xQueueReceive(reminder_queue_, &content, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received reminder from queue: %s", content.c_str());
            ProcessReminderTts(content);
        }
    }
}

void Application::ProcessReminderTts(const std::string& content) {
    ESP_LOGI(TAG, "Processing TTS for reminder: %s", content.c_str());

    // TODO: 修改为你的 TTS 服务器地址
    std::string url = "http://YOUR_SERVER_IP:8081/api/text_to_opus";
    std::string text = "时间到了，我要" + content + "了";

    // 获取网络接口
    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Failed to get network interface");
        return;
    }

    // 创建 HTTP 请求
    auto http = network->CreateHttp(1);
    if (!http) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return;
    }

    // 构造 JSON 请求体
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "text", text.c_str());
    char* json_str = cJSON_PrintUnformatted(root);

    // 设置请求
    http->SetHeader("Content-Type", "application/json");
    http->SetContent(json_str);

    // 发送请求
    if (!http->Open("POST", url.c_str())) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        cJSON_free(json_str);
        cJSON_Delete(root);
        return;
    }

    cJSON_free(json_str);
    cJSON_Delete(root);

    // 流式读取 PCM 数据
    const size_t FRAME_SIZE = 960;
    const size_t FRAME_BYTES = FRAME_SIZE * 2;
    uint8_t frame_buffer_bytes[FRAME_BYTES];
    int16_t full_frame_pcm[FRAME_SIZE];

    auto& audio_service = GetAudioService();

    uint32_t timestamp = 0;
    size_t frame_index = 0;
    size_t total_bytes = 0;

    ESP_LOGI(TAG, "Starting to stream PCM data from TTS service...");

    while (true) {
        int bytes_read = http->Read((char*)frame_buffer_bytes, FRAME_BYTES);

        if (bytes_read < 0) {
            ESP_LOGE(TAG, "Failed to read frame data");
            break;
        }

        if (bytes_read == 0) {
            ESP_LOGI(TAG, "Reached end of stream, total %u bytes read", total_bytes);
            break;
        }

        total_bytes += bytes_read;

        if (frame_index == 0) {
            ESP_LOGI(TAG, "Successfully started streaming, first frame: %u bytes", bytes_read);
        }

        size_t samples_in_frame = bytes_read / 2;
        const int16_t* frame_pcm = reinterpret_cast<const int16_t*>(frame_buffer_bytes);

        // 复制并填充零
        std::copy(frame_pcm, frame_pcm + samples_in_frame, full_frame_pcm);
        for (size_t i = samples_in_frame; i < FRAME_SIZE; i++) {
            full_frame_pcm[i] = 0;
        }

        // Opus 编码
        std::vector<int16_t> full_frame(full_frame_pcm, full_frame_pcm + FRAME_SIZE);

        auto packet = std::make_unique<AudioStreamPacket>();
        packet->sample_rate = 16000;
        packet->frame_duration = 60;
        packet->timestamp = timestamp;

        if (!audio_service.GetOpusEncoder()->Encode(std::move(full_frame), packet->payload)) {
            ESP_LOGE(TAG, "Failed to encode frame %u", frame_index);
            break;
        }

        if ((frame_index + 1) % 10 == 0) {
            ESP_LOGI(TAG, "Sending frame %u (%u bytes Opus)...", frame_index + 1, packet->payload.size());
        }

        if (!protocol_->SendAudio(std::move(packet))) {
            ESP_LOGE(TAG, "Failed to send frame %u", frame_index);
            break;
        }

        timestamp += 60;
        frame_index++;

        // 每 5 帧让出 CPU
        if (frame_index % 5 == 0) {
            vTaskDelay(1);
        }
    }

    http->Close();

    ESP_LOGI(TAG, "Reminder audio sent: %u frames, %u bytes", frame_index, total_bytes);
}

// ========== 提醒功能结束 ==========
```

**关键修改点**：
- 第 12 行：修改 `url` 为你的 TTS 服务器地址
- 检查 `Board::GetInstance().GetNetwork()` 是否存在，可能需要调整

---

### 步骤 4: 修改 protocol.h

打开 `D:\code\eyes\main\protocols\protocol.h`，在 `Protocol` 类中添加：

```cpp
public:
    // ... 现有方法 ...

    // ========== 新增：发送提醒 ==========
    virtual void SendReminder(const std::string& content) {
        // 默认实现：空（子类可以覆盖）
    }

    // ... 其他方法 ...
};
```

**完整位置**（约在第 73 行附近）：

```diff
--- a/main/protocols/protocol.h
+++ b/main/protocols/protocol.h
@@ -70,6 +70,9 @@ public:
     virtual void SendStartListening(ListeningMode mode);
     virtual void SendStopListening();
     virtual void SendAbortSpeaking(AbortReason reason);
     virtual void SendMcpMessage(const std::string& message);
+
+    // ========== 新增：发送提醒 ==========
+    virtual void SendReminder(const std::string& content) {}
```

---

### 步骤 5: 修改 protocol.cc

打开 `D:\code\eyes\main\protocols\protocol.cc`，在文件末尾添加：

```cpp
#include "application.h"  // 确保已包含

#define TAG "Protocol"

void Protocol::SendReminder(const std::string& content) {
    ESP_LOGI(TAG, "Sending reminder: %s", content.c_str());
    // 通过队列将提醒发送给 TTS 处理任务
    Application::GetInstance().QueueReminderTts(content);
}
```

**注意**：如果在 `websocket_protocol.cc` 或 `mqtt_protocol.cc` 中有特定实现，请添加到对应文件中。

---

### 步骤 6: 修改 mcp_server.cc

打开 `D:\code\eyes\main\mcp_server.cc`，找到添加用户工具的方法（通常是 `AddUserOnlyTools()` 或类似方法），添加以下三个工具：

```cpp
// 在 mcp_server.cc 中添加

#include "reminder_manager.h"  // 在文件顶部添加

void McpServer::AddUserOnlyTools() {
    // ... 现有工具 ...

    // ========== 新增：提醒工具 ==========

    // 1. 添加提醒
    AddTool("self.reminder.add",
        "Set a reminder with delay in seconds or absolute timestamp.\n\n"
        "MANDATORY: When user asks to set a reminder (e.g., 'remind me in 20 seconds', '20秒后提醒我'), you MUST:\n"
        "1. First call self.get_device_status to get current timestamp\n"
        "2. Then IMMEDIATELY call self.reminder.add in the SAME RESPONSE with:\n"
        "   - content: the reminder text (what to remind)\n"
        "   - delay_in_seconds: time delay in seconds (e.g., 20 for 'in 20 seconds', 600 for 'in 10 minutes')\n"
        "   - timestamp: absolute Unix timestamp (alternative to delay_in_seconds)\n\n"
        "DO NOT wait for user confirmation. DO NOT say 'I will set it'. Just CALL THE TOOL.\n"
        "Examples:\n"
        "- '20秒后提醒我喝水' → content='喝水', delay_in_seconds=20\n"
        "- 'remind me in 5 minutes' → content='reminder', delay_in_seconds=300",
        [this](const cJSON* params) -> std::string {
            // 解析参数
            const cJSON* content_item = cJSON_GetObjectItem(params, "content");
            const cJSON* delay_item = cJSON_GetObjectItem(params, "delay_in_seconds");
            const cJSON* timestamp_item = cJSON_GetObjectItem(params, "timestamp");

            if (!content_item || !cJSON_IsString(content_item)) {
                return "Error: Missing 'content' parameter";
            }

            std::string content = content_item->valuestring;

            // 计算触发时间
            long long trigger_timestamp = 0;
            if (delay_item && cJSON_IsNumber(delay_item)) {
                // 使用延时
                int delay_seconds = (int)delay_item->valuedouble;
                // TODO: 获取当前时间戳的方法可能不同
                long long current_time = get_current_timestamp();
                trigger_timestamp = current_time + delay_seconds;
            } else if (timestamp_item && cJSON_IsNumber(timestamp_item)) {
                // 使用绝对时间戳
                trigger_timestamp = (long long)timestamp_item->valuedouble;
            } else {
                return "Error: Missing 'delay_in_seconds' or 'timestamp' parameter";
            }

            // 添加提醒
            ReminderManager::GetInstance().AddReminder(trigger_timestamp, content);

            // 返回成功消息
            char response[256];
            snprintf(response, sizeof(response), "Reminder added: %s at %lld",
                    content.c_str(), trigger_timestamp);
            return std::string(response);
        }
    );

    // 2. 查看提醒列表
    AddTool("self.reminder.list",
        "List all active reminders.",
        [this](const cJSON* params) -> std::string {
            auto reminders = ReminderManager::GetInstance().GetReminders();

            if (reminders.empty()) {
                return "No active reminders.";
            }

            cJSON* root = cJSON_CreateArray();
            for (const auto& reminder : reminders) {
                cJSON* item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "id", reminder.id.c_str());
                cJSON_AddStringToObject(item, "content", reminder.content.c_str());
                cJSON_AddNumberToObject(item, "timestamp", (double)reminder.timestamp);
                cJSON_AddItemToArray(root, item);
            }

            char* json_str = cJSON_PrintUnformatted(root);
            std::string result(json_str);
            cJSON_free(json_str);
            cJSON_Delete(root);

            return result;
        }
    );

    // 3. 删除提醒
    AddTool("self.reminder.remove",
        "Remove a reminder by ID.",
        [this](const cJSON* params) -> std::string {
            const cJSON* id_item = cJSON_GetObjectItem(params, "id");
            if (!id_item || !cJSON_IsString(id_item)) {
                return "Error: Missing 'id' parameter";
            }

            std::string id = id_item->valuestring;
            bool success = ReminderManager::GetInstance().RemoveReminder(id);

            if (success) {
                return "Reminder removed successfully.";
            } else {
                return "Failed to remove reminder: ID not found.";
            }
        }
    );

    // ... 其他工具 ...
}
```

**关键点**：
- 第 26 行：`get_current_timestamp()` 函数可能需要根据 eyes 项目实现
- 检查 `AddTool()` 方法签名是否匹配

---

### 步骤 7: 修改 CMakeLists.txt

打开 `D:\code\eyes\main\CMakeLists.txt`，在 `SOURCES` 列表中添加 `reminder_manager.cc`：

```cmake
set(SOURCES "audio/audio_codec.cc"
            "audio/audio_service.cc"
            # ... 其他源文件 ...
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "mcp_server.cc"
            "reminder_manager.cc"    # ========== 新增 ==========
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "device_state_event.cc"
            "main.cc"
            )
```

**具体位置**：在第 36 行 `mcp_server.cc` 之后添加。

---

### 步骤 8: 初始化 ReminderManager

在 `Application::Start()` 方法的开始部分添加初始化：

```cpp
void Application::Start() {
    // ... 现有代码 ...

    // ========== 新增：初始化提醒管理器 ==========
    ReminderManager::GetInstance().Initialize();

    // ... 其他代码 ...
}
```

**具体位置**：在 `audio_service_.Initialize(codec)` 之前或之后均可。

---

## 配置修改

### 1. TTS 服务器地址

修改 `application.cc` 中的 `ProcessReminderTts()` 方法：

```cpp
std::string url = "http://YOUR_SERVER_IP:8081/api/text_to_opus";
```

替换为你的 TTS 服务器地址。

### 2. 初始化静态变量

在 `application.cc` 的全局区域添加：

```cpp
// 在 application.cc 顶部
int64_t Application::g_last_channel_open_time_ = 0;
```

或在 `OnAudioChannelOpened()` 回调中更新：

```cpp
void Application::OnAudioChannelOpened(...) {
    g_last_channel_open_time_ = esp_timer_get_time();
    // ... 其他代码 ...
}
```

---

## 编译和测试

### 1. 编译

```bash
cd D:\code\eyes
idf.py build
```

**预期结果**：编译成功，无错误。

**常见错误**：

| 错误 | 原因 | 解决方案 |
|-----|------|---------|
| `reminder_manager.h: No such file` | 文件未复制 | 执行步骤 1 |
| `ReminderManager was not declared` | 未包含头文件 | 检查 `#include "reminder_manager.h"` |
| `QueueReminderTts is not a member` | 未声明方法 | 检查 application.h 修改 |
| `undefined reference to get_current_timestamp` | 函数不存在 | 实现或替换为项目中的时间函数 |

### 2. 功能测试

#### 测试 1: 设置提醒

```
用户: "20秒后提醒我测试"
```

**预期日志**：
```
I (12345) McpServer: Handling tool call: self.reminder.add
I (12345) ReminderManager: Added reminder: 测试
I (12345) ReminderManager: Loaded 1 reminders
```

20 秒后：
```
I (12345) Application: Sending reminder: 测试
I (12345) Application: Processing TTS for reminder: 测试
I (12345) Application: Starting to stream PCM data...
I (12345) Application: Successfully started streaming, first frame: 1920 bytes
I (12345) Application: Reminder audio sent: 50 frames, 96000 bytes
```

#### 测试 2: 查看提醒

```
用户: "查看所有提醒"
```

**预期结果**：AI 显示提醒列表。

#### 测试 3: 删除提醒

```
用户: "删除所有提醒"
```

**预期结果**：提醒列表清空。

---

## Eyes 项目特殊调整

### 1. Board 类调整

如果 eyes 项目没有 `Board::GetInstance().GetNetwork()`，需要查找对应方法：

```cpp
// 可能的替代方案
auto network = /* eyes 项目的网络获取方法 */;
```

**查找方法**：
```bash
cd D:\code\eyes
grep -r "GetNetwork" main/
grep -r "CreateHttp" main/
```

### 2. Alert 方法调整

eyes 项目的 `Alert()` 方法签名可能不同：

```cpp
// 原始调用
Alert("INFO", reminder.content.c_str(), "", "");

// 可能需要调整为
Alert(reminder.content.c_str(), "INFO");
// 或
ShowNotification(reminder.content.c_str());
```

**查找方法**：
```bash
grep -r "void.*Alert" main/
```

### 3. 时间戳获取

如果 `get_current_timestamp()` 不存在，使用以下实现之一：

```cpp
// 方案 1: 使用 esp_timer
long long get_current_timestamp() {
    return esp_timer_get_time() / 1000;
}

// 方案 2: 使用 time 函数
long long get_current_timestamp() {
    return std::time(nullptr);
}
```

---

## 快速移植检查清单

- [ ] 复制 `reminder_manager.h` 到 `main/`
- [ ] 复制 `reminder_manager.cc` 到 `main/`
- [ ] 修改 `application.h`：
  - [ ] 添加 `reminder_tts_task_handle_` 成员
  - [ ] 添加 `reminder_queue_` 成员
  - [ ] 添加 `g_last_channel_open_time_` 静态成员
  - [ ] 添加 `QueueReminderTts()` 声明
  - [ ] 添加 `ReminderTtsTask()` 声明
  - [ ] 添加 `ProcessReminderTts()` 声明
- [ ] 修改 `application.cc`：
  - [ ] 包含 `reminder_manager.h`
  - [ ] 初始化静态成员变量
  - [ ] 在 `Start()` 中初始化 ReminderManager
  - [ ] 在 `Start()` 中创建队列和任务
  - [ ] 在 `OnClockTimer()` 中检查提醒
  - [ ] 实现 `QueueReminderTts()`
  - [ ] 实现 `ReminderTtsTask()`
  - [ ] 实现 `ProcessReminderTts()`
- [ ] 修改 `protocol.h`：
  - [ ] 添加 `SendReminder()` 虚方法
- [ ] 修改 `protocol.cc`：
  - [ ] 实现 `SendReminder()`
- [ ] 修改 `mcp_server.cc`：
  - [ ] 包含 `reminder_manager.h`
  - [ ] 添加 `self.reminder.add` 工具
  - [ ] 添加 `self.reminder.list` 工具
  - [ ] 添加 `self.reminder.remove` 工具
- [ ] 修改 `CMakeLists.txt`：
  - [ ] 添加 `reminder_manager.cc` 到 SOURCES
- [ ] 配置：
  - [ ] 修改 TTS 服务器地址
  - [ ] 调整 Board/Alert/时间戳方法

---

## 故障排查

### 问题 1: 编译错误 - 提醒管理器未定义

```
error: 'ReminderManager' has not been declared
```

**解决**：
```bash
# 检查是否包含了头文件
grep "#include.*reminder_manager.h" main/application.cc
```

### 问题 2: 链接错误

```
undefined reference to `Application::g_last_channel_open_time_`
```

**解决**：在 `application.cc` 顶部添加：
```cpp
int64_t Application::g_last_channel_open_time_ = 0;
```

### 问题 3: 任务创建失败

```
E (12345) Application: Failed to create reminder queue
```

**原因**：内存不足。

**解决**：减少队列深度或栈大小：
```cpp
xQueueCreate(3, sizeof(std::string));  // 从 5 改为 3
xTaskCreate(..., 8192, ...);            // 从 12288 改为 8192
```

---

## 附录：完整文件差异

### application.h 差异

```diff
--- a/main/application.h
+++ b/main/application.h
@@ -43,6 +43,9 @@ public:
     void PlaySound(const std::string_view& sound);
     AudioService& GetAudioService() { return audio_service_; }

+    void QueueReminderTts(const std::string& content);
+
 private:
     Application();
     ~Application();
@@ -82,6 +85,10 @@ private:
     bool has_server_time_ = false;
     bool aborted_ = false;
     int clock_ticks_ = 0;
+    TaskHandle_t reminder_tts_task_handle_ = nullptr;
+    QueueHandle_t reminder_queue_ = nullptr;
+    static int64_t g_last_channel_open_time_;
+
     TaskHandle_t check_new_version_task_handle_ = nullptr;

@@ -158,6 +165,9 @@ private:
     void OnClockTimer();
     void SetListeningMode(ListeningMode mode);
+    void ReminderTtsTask();
+    void ProcessReminderTts(const std::string& content);
 };

 #endif // _APPLICATION_H_
```

### CMakeLists.txt 差异

```diff
--- a/main/CMakeLists.txt
+++ b/main/CMakeLists.txt
@@ -34,6 +34,7 @@ set(SOURCES
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "iot/thing.cc"
+           "reminder_manager.cc"
            "iot/thing_manager.cc"
            "mcp_server.cc"
```

---

**文档版本**: v1.0
**最后更新**: 2025-01-05
**目标项目**: eyes (D:\code\eyes)
**源项目**: 14-handheld (D:\code\14-handheld)
**作者**: Claude Code Assistant
