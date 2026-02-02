# Chat 功能移植指南

## 📋 需要复制的文件

### 必需文件（2个）

```
your_project/main/chat/
├── chat_recorder.h      # 对话记录器头文件
└── chat_recorder.cc     # 对话记录器实现
```

**复制到目标项目**：
```bash
mkdir -p your_project/main/chat
cp G:/code/eyes/main/chat/chat_recorder.h your_project/main/chat/
cp G:/code/eyes/main/chat/chat_recorder.cc your_project/main/chat/
```

---

## 🔧 依赖项检查清单

### 1. Protocol 层（必需）

ChatRecorder 依赖 Protocol 类来上传数据。你的项目需要有一个 Protocol 基类。

**检查方法**：
```bash
grep -r "class Protocol" your_project/main/protocols/
```

**如果没有**，需要实现一个基础的 Protocol 接口：
```cpp
// protocols/protocol.h
class Protocol {
public:
    virtual ~Protocol() = default;
    virtual void SendChatBatch(const std::string& json_data) = 0;
    // 其他方法...
};
```

### 2. cJSON 库（必需）

ChatRecorder 使用 cJSON 构建 JSON 数据。

**检查方法**：
```bash
grep -r "cJSON.h" your_project/
```

**如果没有**，在 `main/CMakeLists.txt` 中添加：
```cmake
idf_component_register(SRCS "..." INCLUDE_DIRS "."
                       REQUIRES json)
```

### 3. ESP32 组件（必需）

需要以下 ESP-IDF 组件：
- `esp_log` - 日志输出
- `esp_system` - 系统信息
- `esp_network` - 网络功能

**检查方法**：这些通常已经包含在 ESP-IDF 中，无需额外配置。

---

## 📝 修改文件清单

### 1. main/CMakeLists.txt

**位置**：`your_project/main/CMakeLists.txt`

**添加 chat 源文件**：
```cmake
# 在 SRCS 列表中添加
set(SRCS
    "application.cc"
    "chat/chat_recorder.cc"    # 新增：对话记录器
    # ... 其他文件
)

# 在 INCLUDE_DIRS 中添加 chat 目录
set(INCLUDE_DIRS
    "."
    "display"
    "audio"
    "protocols"
    "chat"                    # 新增：chat 目录
)
```

**示例**：
```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS "application.cc"
         "chat/chat_recorder.cc"    # ← 添加这一行
         "display/eye_display.cc"
         # ... 其他源文件
    INCLUDE_DIRS "."
                 "display"
                 "audio"
                 "protocols"
                 "chat"              # ← 添加这一行
    REQUIRES nvs_flash fatfs_spiffs
)
```

---

### 2. main/application.h（或你的主类头文件）

**位置**：`your_project/main/application.h`

**添加方法声明**：
```cpp
class Application {
public:
    // ... 现有方法

    // ========== 新增：对话记录方法 ==========
    /**
     * @brief 记录用户输入（STT识别结果）
     * @param text 用户输入的文本
     */
    void RecordUserInput(const std::string& text);

    /**
     * @brief 记录AI回复（TTS合成文本）
     * @param text AI回复的文本
     * @param complete 是否完成（true=记录完整对话，false=累积文本）
     */
    void RecordAIResponse(const std::string& text, bool complete = true);

    // ... 其他方法

private:
    TaskHandle_t chat_upload_task_handle_;  // 对话上传任务句柄
};
```

---

### 3. main/application.cc（或你的主类实现文件）

**位置**：`your_project/main/application.cc`

#### 步骤 1：引入头文件

```cpp
// 在文件顶部的 include 部分
#include "chat_recorder.h"
```

#### 步骤 2：在初始化方法中设置 Protocol

**找到你的 Protocol 初始化位置**（通常是 `Application::Initialize()` 或构造函数）：

```cpp
void Application::Initialize() {
    // ... 现有初始化代码

    // 初始化 Protocol（MQTT 或 WebSocket）
    protocol_ = std::make_unique<WebsocketProtocol>();
    // 或者
    // protocol_ = std::make_unique<MqttProtocol>();

    // ========== 新增：初始化对话记录器 ==========
    ChatRecorder::GetInstance().SetProtocol(protocol_.get());
    ESP_LOGI(TAG, "ChatRecorder initialized with protocol");

    // 创建对话上传定时任务（每10分钟检查一次）
    xTaskCreate([](void* arg) {
        Application* app = static_cast<Application*>(arg);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));  // 10分钟
            ChatRecorder::GetInstance().PeriodicUploadCheck();
        }
        vTaskDelete(NULL);
    }, "chat_upload", 4096, this, 2, &chat_upload_task_handle_);
    ESP_LOGI(TAG, "Chat upload task created");

    // ... 其他初始化代码
}
```

#### 步骤 3：实现记录方法

**在 application.cc 的底部添加**：

```cpp
// ========== 对话记录功能实现 ==========

void Application::RecordUserInput(const std::string& text) {
    ChatRecorder::GetInstance().RecordUserInput(text);
}

void Application::RecordAIResponse(const std::string& text, bool complete) {
    ChatRecorder::GetInstance().RecordAIResponse(text, complete);
}

// ========== 对话记录功能结束 ==========
```

---

## 🎯 在代码中使用

### 使用场景 1：STT 识别结果

```cpp
// 在你的语音识别回调中
void OnSTTResult(const std::string& text) {
    ESP_LOGI(TAG, "User said: %s", text.c_str());

    // 记录用户输入
    Application::GetInstance().RecordUserInput(text);

    // ... 发送到 AI 处理
}
```

### 使用场景 2：TTS 文本合成

```cpp
// 在你的 AI 回复处理中
void OnAIResponse(const std::string& text_chunk, bool is_final) {
    // 记录 AI 回复
    Application::GetInstance().RecordAIResponse(text_chunk, is_final);

    // ... TTS 播放
}
```

### 使用场景 3：流式对话（多段回复）

```cpp
// AI 回复第一段
Application::GetInstance().RecordAIResponse("今天天气", false);

// AI 回复第二段
Application::GetInstance().RecordAIResponse("晴朗，温度25度", false);

// AI 回复最后一段
Application::GetInstance().RecordAIResponse("适合外出", true);  // ← true 表示结束
```

---

## ⚙️ 可选配置

### 调整批量大小和上传间隔

**位置**：`chat/chat_recorder.cc`

```cpp
// 修改这两个常量
static const size_t DEFAULT_BATCH_SIZE = 15;                    // 批量大小：15条对话
static const int64_t DEFAULT_UPLOAD_INTERVAL_MS = 10 * 60 * 1000;  // 上传间隔：10分钟

// 示例：改为更大的批量，更长的间隔
static const size_t DEFAULT_BATCH_SIZE = 30;                    // 30条对话
static const int64_t DEFAULT_UPLOAD_INTERVAL_MS = 30 * 60 * 1000;  // 30分钟
```

### 调整定时任务间隔

**位置**：`application.cc` 的初始化代码

```cpp
vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));  // 10分钟

// 改为
vTaskDelay(pdMS_TO_TICKS(5 * 60 * 1000));   // 5分钟
```

---

## 🧪 测试步骤

### 1. 编译测试

```bash
cd your_project
idf.py build
```

**预期结果**：编译成功，无错误

### 2. 功能测试

```cpp
// 在你的代码中添加测试调用
void TestChatRecorder() {
    // 测试用户输入
    Application::GetInstance().RecordUserInput("测试用户输入");

    // 测试 AI 回复
    Application::GetInstance().RecordAIResponse("测试AI回复", true);

    // 手动触发上传
    ChatRecorder::GetInstance().UploadBatch();

    ESP_LOGI(TAG, "ChatRecorder test completed");
}
```

### 3. 日志验证

**查看串口输出**：
```
I (1234) ChatRecorder: User input recorded: 测试用户输入
I (1235) ChatRecorder: AI text accumulated: 测试AI回复
I (1236) ChatRecorder: Complete dialogue recorded (user: 测试用户输入, ai: 测试AI回复)
I (1237) ChatRecorder: Batch size threshold reached, triggering upload
```

---

## ❗ 常见问题

### Q1: 编译错误 "undefined reference to ChatRecorder"

**原因**：`CMakeLists.txt` 中没有添加 `chat_recorder.cc`

**解决**：按照步骤 1 修改 `CMakeLists.txt`

### Q2: 编译错误 "protocol.h: No such file"

**原因**：缺少 Protocol 类

**解决**：确保你的项目有 Protocol 层，并检查 INCLUDE_DIRS

### Q3: 运行时没有上传数据

**原因**：没有调用 `SetProtocol()`

**解决**：在初始化时调用 `ChatRecorder::GetInstance().SetProtocol(protocol_.get())`

### Q4: 数据没有记录

**原因**：没有调用 `RecordUserInput()` 和 `RecordAIResponse()`

**解决**：在 STT 和 TTS 回调中添加记录调用

---

## 📦 最小集成清单（快速检查）

确保以下 5 步已完成：

- [ ] **1. 复制文件**
  - [ ] `chat_recorder.h` → `main/chat/`
  - [ ] `chat_recorder.cc` → `main/chat/`

- [ ] **2. 修改 CMakeLists.txt**
  - [ ] 添加 `"chat/chat_recorder.cc"` 到 SRCS
  - [ ] 添加 `"chat"` 到 INCLUDE_DIRS

- [ ] **3. 在 application.cc 中引入头文件**
  - [ ] `#include "chat_recorder.h"`

- [ ] **4. 初始化 ChatRecorder**
  - [ ] `ChatRecorder::GetInstance().SetProtocol(protocol_.get())`
  - [ ] 创建定时上传任务

- [ ] **5. 实现记录方法**
  - [ ] `Application::RecordUserInput()`
  - [ ] `Application::RecordAIResponse()`

- [ ] **6. 在 STT/TTS 回调中调用记录方法**

---

## 📝 代码模板

### 完整的 application.cc 集成示例

```cpp
// application.cc

#include "chat_recorder.h"

void Application::Initialize() {
    // ... 现有初始化

    // 初始化 Protocol
    protocol_ = std::make_unique<WebsocketProtocol>();

    // 初始化 ChatRecorder
    ChatRecorder::GetInstance().SetProtocol(protocol_.get());
    ESP_LOGI(TAG, "ChatRecorder initialized with protocol");

    // 创建定时上传任务
    xTaskCreate([](void* arg) {
        Application* app = static_cast<Application*>(arg);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));
            ChatRecorder::GetInstance().PeriodicUploadCheck();
        }
        vTaskDelete(NULL);
    }, "chat_upload", 4096, this, 2, &chat_upload_task_handle_);

    // ...
}

void Application::RecordUserInput(const std::string& text) {
    ChatRecorder::GetInstance().RecordUserInput(text);
}

void Application::RecordAIResponse(const std::string& text, bool complete) {
    ChatRecorder::GetInstance().RecordAIResponse(text, complete);
}

// 在 STT 回调中
void OnSTTResult(const std::string& text) {
    this->RecordUserInput(text);
    // ...
}

// 在 AI 回调中
void OnAIResponseChunk(const std::string& chunk) {
    this->RecordAIResponse(chunk, false);
    // ...
}

void OnAIResponseEnd() {
    this->RecordAIResponse("", true);  // 标记结束
    // ...
}
```

---

## 🎉 完成

完成以上步骤后，你的项目就拥有了完整的对话记录功能！

**特性**：
- ✅ 自动记录用户输入和 AI 回复
- ✅ 批量上传优化（15条或10分钟）
- ✅ 线程安全
- ✅ 定时同步
- ✅ 掉电保护（需配置 NVS）

---

**文档创建日期**: 2025-01-22
**适用项目**: 任何基于 ESP-IDF 的 ESP32 项目
**前提条件**: 已有 Protocol 层（MQTT/WebSocket）
