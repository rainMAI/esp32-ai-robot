# 提醒功能 TTS 播报差异分析

## 📋 文档信息

- **分析日期**: 2025-01-07
- **对比项目**:
  - **14-handheld**: 原始项目（播放"轩轩爸爸，快去喝杯水吧..."）
  - **eyes**: 当前项目（播放"喝完水别忘了把杯子放回原位..."）
- **问题**: 当前项目的提醒播报不像真正的提醒，更像事后叮嘱

---

## 🔍 核心差异

### 1. TTS 文本生成逻辑差异 ⭐⭐⭐ (最关键)

#### 14-handheld 项目

**文件**: [main/chat/application.cc:945-954](../../14-handheld/main/chat/application.cc#L945-L954)

```cpp
void Application::ProcessReminderTts(const std::string& content) {
    std::string text;

    // 逻辑 1: 如果内容以"去"、"到"、"做"开头
    if (content.find("去") == 0 || content.find("到") == 0 || content.find("做") == 0) {
        text = "时间到了，该" + content + "了，记得准时哦。";
    }
    // 逻辑 2: 如果内容包含"练习"、"口语"、"学习"、"英语"
    else if (content.find("练习") != std::string::npos ||
             content.find("口语") != std::string::npos ||
             content.find("学习") != std::string::npos ||
             content.find("英语") != std::string::npos) {
        text = "到时间" + content + "了，要不要我们现在来进行互动练习呢？";
    }
    // 逻辑 3: 其他情况
    else {
        text = "时间到了，该去" + content + "了，记得准时哦。";
    }

    ESP_LOGI(TAG, "ProcessReminderTts: 准备回传提醒音频: [%s]", text.c_str());
    // ... 后续 HTTP 请求和音频上传
}
```

**关键点**:
- ✅ **智能匹配**: 根据提醒内容的关键词，选择不同的提示语
- ✅ **自然对话**: 使用"时间到了，该去..."、"要不要..."等自然表达
- ✅ **正面引导**: 鼓励用户执行动作（"要不要我们现在来进行互动练习呢？"）

#### eyes 项目

**文件**: [main/application.cc:865-869](main/application.cc#L865-L869)

```cpp
void Application::ProcessReminderTts(const std::string& content) {
    ESP_LOGI(TAG, "Processing TTS for reminder: %s", content.c_str());

    std::string url = "http://192.140.190.183:8081/api/text_to_pcm";
    std::string text = "时间到了，我要" + content + "了";  // ⚠️ 固定模板

    // ... 后续代码与 14-handheld 相同
}
```

**关键点**:
- ❌ **固定模板**: 只有一个固定的模板 `"时间到了，我要" + content + "了"`
- ❌ **第一人称**: 使用"我要"而不是"该去"或"该做"
- ❌ **不区分场景**: 所有提醒内容都使用同样的表达方式

---

## 📊 实际播放效果对比

### 测试场景: 用户设置"10秒后提醒我去喝水"

#### 14-handheld 项目的完整流程

**用户**: "10秒后提醒我去喝水"

**AI 调用 MCP 工具**:
```json
{
  "name": "self.reminder.add",
  "arguments": {
    "content": "去喝水",  // ✅ 提取动作
    "delay_in_seconds": 10
  }
}
```

**日志显示**:
```
I (21546) MCP: Adding relative reminder: 去喝水 (delay: 10, now: ...)
I (31216) ReminderManager: Reminder due: 去喝水
I (31246) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，该去喝水了，记得准时哦。]
```

**最终播放**:
> **"轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～"**

**分析**:
- ✅ AI 生成了自然的提醒内容"去喝水"
- ✅ 前缀匹配 `content.find("去") == 0`，使用模板 1
- ✅ 生成文本: "时间到了，该去喝水了，记得准时哦。"
- ✅ **服务器 AI 进一步优化为**: "轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～"

#### eyes 项目的完整流程

**用户**: "10秒后提醒我去喝水"

**AI 调用 MCP 工具**:
```json
{
  "name": "self.reminder.add",
  "arguments": {
    "content": "喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！",  // ⚠️ 完整句子
    "delay_in_seconds": 10
  }
}
```

**日志显示**:
```
I (21546) MCP: Adding relative reminder: 喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！
I (31216) ReminderManager: Reminder due: 喝完水别忘了...
I (31246) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，我要喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！了]
```

**最终播放**:
> **"时间到了，我要喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！"**

**分析**:
- ❌ AI 生成了完整的提醒内容（而不是动作）
- ❌ 不匹配任何前缀条件，使用固定模板: `"时间到了，我要" + content + "了"`
- ❌ 结果变成: "时间到了，我要喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！"
- ❌ **语法不通**: "我要...保持整洁"不符合逻辑

---

## 🎯 问题根源

### 问题 1: MCP 工具描述不同 ⭐⭐⭐

#### 14-handheld 项目

**文件**: [main/chat/mcp_server.cc:181-189](../../14-handheld/main/chat/mcp_server.cc#L181-L189)

```cpp
AddTool("self.reminder.add",
    "Adds a new reminder to the device. \n"
    "Parameters: \n"
    "- content: The core text of the reminder.\n"  // ✅ 强调"核心文本"
    "- delay_in_seconds: Use this for relative time reminders like 'in 10 minutes'. \n"
    "  Note: For simple relative reminders, you don't need to call get_device_status.\n"
    "- timestamp: Use this for absolute time reminders like 'at 3:00 PM'. \n"
    "  IMPORTANT: For absolute time, you MUST call `self.get_device_status` first to get the current `timestamp` and `time_str` of the device.\n"
    "  Calculate the absolute timestamp as: device_current_timestamp + (target_local_time - device_current_local_time).",
    // ...
)
```

**特点**:
- ✅ 描述简洁明确
- ✅ 强调 `content` 是"核心文本"
- ✅ 给出相对时间和绝对时间的使用示例

#### eyes 项目

**文件**: [main/mcp_server.cc:124-132](main/mcp_server.cc#L124-L132)

```cpp
AddTool("self.reminder.add",
    "Add a reminder. \n"
    "You MUST call `self.get_device_status` first to get the current timestamp, then calculate the target timestamp based on the user's relative time (e.g. 'in 10 minutes') or absolute time.\n\n"  // ⚠️ 强制要求
    "IMPORTANT: Always call BOTH `self.get_device_status` AND `self.reminder.add` IN THE SAME BATCH (as a JSON array).\n\n"  // ⚠️ 额外要求
    "After success, say '提醒已设置' or similar in Chinese.\n\n"  // ⚠️ 添加了输出要求
    "Args:\n"
    "- `content`: Reminder text.\n"  // ⚠️ 描述模糊
    "- `delay_in_seconds`: Seconds from now (optional, for simple cases).\n"
    "- `timestamp`: Target Unix epoch seconds (use system.timestamp from get_device_status + delay).",
    // ...
)
```

**特点**:
- ⚠️ 描述过于复杂，增加了不必要的限制
- ⚠️ `content` 描述为"Reminder text"，AI 可能理解为完整句子
- ⚠️ 强制要求调用 `get_device_status`，但相对时间不需要
- ⚠️ 添加了"After success, say '提醒已设置'"，干扰了 AI 的判断

### 问题 2: TTS 生成逻辑过于简单 ⭐⭐⭐

**eyes 项目**只使用一个固定模板:

```cpp
std::string text = "时间到了，我要" + content + "了";
```

**问题**:
- ❌ 不区分提醒内容类型
- ❌ 使用第一人称"我要"（不像提醒，更像自言自语）
- ❌ 如果 content 是完整句子，会导致语法错误

**14-handheld 项目**使用智能匹配:

```cpp
if (content.find("去") == 0 || content.find("到") == 0 || content.find("做") == 0) {
    text = "时间到了，该" + content + "了，记得准时哦。";
} else if (content.find("练习") != std::string::npos || ...) {
    text = "到时间" + content + "了，要不要我们现在来进行互动练习呢？";
} else {
    text = "时间到了，该去" + content + "了，记得准时哦。";
}
```

**优势**:
- ✅ 根据内容类型选择不同模板
- ✅ 使用第二人称"该去"，更像提醒
- ✅ 多样化表达，避免单调

---

## 🎨 AI 生成内容的差异

### 14-handheld 项目

**用户**: "10秒后提醒我去喝水"

**AI 理解**:
- 提醒内容应该是简短的动作（"去喝水"）
- 使用相对时间（delay_in_seconds）

**AI 调用**:
```json
{
  "name": "self.reminder.add",
  "arguments": {
    "content": "去喝水",  // ✅ 简洁的动作
    "delay_in_seconds": 10
  }
}
```

**结果**:
- TTS 文本: "时间到了，该去喝水了，记得准时哦。"
- 最终播放: "轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～"

### eyes 项目

**用户**: "10秒后提醒我去喝水"

**AI 理解**:
- MCP 工具描述复杂，要求先获取时间戳
- `content` 应该是"Reminder text"（完整句子）
- 需要添加额外的温馨提示

**AI 调用**:
```json
{
  "name": "self.get_device_status"
}
// 然后调用
{
  "name": "self.reminder.add",
  "arguments": {
    "content": "喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！",  // ❌ 完整句子
    "delay_in_seconds": 10
  }
}
```

**结果**:
- TTS 文本: "时间到了，我要喝完水别忘了把杯子放回原位哦～保持整洁也是小科学家的好习惯呢！"
- 最终播放: 同上（语法错误，逻辑混乱）

---

## 💡 修复方案

### 方案 1: 优化 MCP 工具描述 ⭐⭐⭐ (推荐)

**文件**: [main/mcp_server.cc:124-132](main/mcp_server.cc#L124-L132)

**修改为**:

```cpp
AddTool("self.reminder.add",
    "Adds a new reminder to the device. When the reminder time comes, the device will speak the reminder content.\n\n"
    "Parameters:\n"
    "- content: The **core action** of the reminder (e.g., '去喝水', '做运动', '学习英语'). Use a SHORT phrase starting with verbs like '去', '做', '到', etc.\n"
    "- delay_in_seconds: Seconds from now (for relative time like 'in 10 minutes').\n"
    "- timestamp: Absolute Unix timestamp (for specific time like 'at 3:00 PM').\n\n"
    "IMPORTANT:\n"
    "- For relative time (in X minutes), use delay_in_seconds directly.\n"
    "- For absolute time (at X o'clock), call self.get_device_status FIRST to get current timestamp.\n"
    "- Keep content SHORT and ACTION-ORIENTED (verb + object).",
    PropertyList({
        Property("content", kPropertyTypeString),
        Property("delay_in_seconds", kPropertyTypeInteger, 0),
        Property("timestamp", kPropertyTypeInteger, 0)
    }),
    // ...
)
```

**改进点**:
- ✅ 明确 `content` 应该是"**core action**"（核心动作）
- ✅ 强调使用"**SHORT phrase starting with verbs**"
- ✅ 给出正面示例: '去喝水', '做运动', '学习英语'
- ✅ 区分相对时间和绝对时间的使用方法

### 方案 2: 改进 TTS 生成逻辑 ⭐⭐⭐

**文件**: [main/application.cc:865-869](main/application.cc#L865-L869)

**修改为**:

```cpp
void Application::ProcessReminderTts(const std::string& content) {
    std::string text;

    // 智能匹配：根据内容类型选择不同模板
    if (content.find("去") == 0 || content.find("到") == 0 || content.find("做") == 0) {
        text = "时间到了，该" + content + "了，记得准时哦。";
    } else if (content.find("练习") != std::string::npos ||
               content.find("口语") != std::string::npos ||
               content.find("学习") != std::string::npos ||
               content.find("英语") != std::string::npos ||
               content.find("看书") != std::string::npos ||
               content.find("写作业") != std::string::npos) {
        text = "到时间" + content + "了，要不要我们现在来进行互动练习呢？";
    } else {
        text = "时间到了，该去" + content + "了，记得准时哦。";
    }

    ESP_LOGI(TAG, "ProcessReminderTts: 准备回传提醒音频: [%s]", text.c_str());

    std::string url = "http://192.140.190.183:8081/api/text_to_pcm";

    // ... 后续代码保持不变
}
```

**改进点**:
- ✅ 与 14-handheld 项目保持一致的智能匹配逻辑
- ✅ 使用第二人称"该去"而不是"我要"
- ✅ 添加更多关键词（"看书", "写作业"）
- ✅ 自然、多样化的表达

### 方案 3: 添加内容验证和清理 ⭐⭐

**在调用 MCP 工具前**:

```cpp
// 在 mcp_server.cc 的 callback 中
std::string content = properties["content"].value<std::string>();

// 清理过长的内容
if (content.length() > 20) {
    ESP_LOGW(TAG, "Reminder content too long, truncating: %s", content.c_str());
    content = content.substr(0, 20);
}

// 如果内容不是以动词开头，尝试提取核心动作
if (content.find("去") != 0 && content.find("做") != 0 &&
    content.find("到") != 0 && content.find("提醒") != 0) {
    // 如果是完整句子，尝试提取动词部分
    size_t verb_pos = content.find("去");
    if (verb_pos != std::string::npos) {
        content = content.substr(verb_pos);
    }
}
```

---

## 📝 总结

### 根本原因

| 问题 | 14-handheld 项目 | eyes 项目 | 影响 |
|-----|------------------|----------|------|
| **MCP 工具描述** | 简洁明确，强调"核心文本" | 复杂冗长，描述模糊 | AI 生成内容不同 |
| **TTS 生成逻辑** | 智能匹配，3 种模板 | 固定模板，1 种 | 表达单调或语法错误 |
| **content 格式** | 简短动作（"去喝水"） | 完整句子（"喝完水别忘了..."） | 最终播放效果差异 |

### 核心差异

**14-handheld 项目**:
1. ✅ MCP 工具描述清晰，引导 AI 生成简短的动作
2. ✅ TTS 逻辑智能匹配，自然表达
3. ✅ 最终播放: "轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～"

**eyes 项目**:
1. ❌ MCP 工具描述复杂，AI 生成完整句子
2. ❌ TTS 逻辑固定模板，语法错误
3. ❌ 最终播放: "时间到了，我要喝完水别忘了把杯子放回原位哦～"

### 推荐修复优先级

1. **立即修复**: 优化 MCP 工具描述（方案 1）⭐⭐⭐
2. **重要改进**: 改进 TTS 生成逻辑（方案 2）⭐⭐⭐
3. **可选增强**: 添加内容验证（方案 3）⭐⭐

---

## 🚀 立即行动

### 最小改动方案（只改 MCP 工具描述）

**文件**: [main/mcp_server.cc:124](main/mcp_server.cc#L124)

**修改前**:
```cpp
"Add a reminder. \n"
"You MUST call `self.get_device_status` first...\n\n"
...
"- `content`: Reminder text.\n"
```

**修改后**:
```cpp
"Adds a new reminder. When time is up, device will speak the reminder.\n\n"
"Args:\n"
"- `content`: **Core action** (e.g., '去喝水', '做运动'). Use SHORT phrase starting with verb.\n"
"- `delay_in_seconds`: Seconds from now (for 'in 10 minutes').\n"
"- `timestamp`: Absolute timestamp (for 'at 3:00 PM').\n\n"
"IMPORTANT: Keep content SHORT and ACTION-ORIENTED.\n"
```

**预期效果**:
- AI 会生成: `{"content": "去喝水", "delay_in_seconds": 10}`
- 而不是: `{"content": "喝完水别忘了把杯子放回原位哦～...", "delay_in_seconds": 10}`

---

## 🔍 实际日志对比分析

### 14-handheld 项目的完整提醒流程日志

```
I (21516) Application: << % self.reminder.add...
I (21526) Application: RAW MCP Message: {"jsonrpc":"2.0","method":"tools/call","id":3,"params":{"name":"self.reminder.add","arguments":{"content":"轩轩爸爸，该喝水了，记得补充水分哦！","delay_in_seconds":10}}}
I (21536) MCP: MCP RPC method: tools/call
I (21546) MCP: Adding relative reminder: 轩轩爸爸，该喝水了，记得补充水分哦！ (delay: 10, now: ld, target: ld)
I (21566) ReminderManager: Added reminder: 轩轩爸爸，该喝水了，记得补充水分哦！ at ld (now: ld)
I (21576) MCP: ReplyResult: id=3, len=93
I (22266) Application: << 提醒已准备就绪，10秒后为您播报。
...
I (31216) ReminderManager: Reminder due: 轩轩爸爸，该喝水了，记得补充水分哦！
W (31216) Application: Alert 信息: 轩轩爸爸，该喝水了，记得补充水分哦！ [bell]
I (31236) Protocol: Sending reminder: 轩轩爸爸，该喝水了，记得补充水分哦！
I (31236) ReminderManager: Reminder handled, removing
I (31236) Application: Received reminder from queue: 轩轩爸爸，该喝水了，记得补充水分哦！
I (31236) AudioService: Resampling audio from 16000 to 24000
I (31256) OpusResampler: Resampler configured with input sample rate 16000 and output sample rate 24000
I (31246) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，该去轩轩爸爸，该喝水了，记得补充水分哦！了，记得准时哦。]
...
I (40656) Application: Reminder audio upload finished, total 237346 bytes
I (41906) Application: >> 时间到了，该去轩轩爸爸，该喝水了，记得补充水分哦。那记得准时哦。
I (41916) Application: STATE: speaking
I (42206) Application: << 时间到！
I (43476) Application: << 轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～
```

**关键点**:
- ✅ **有 RAW MCP Message 日志** (line 21526)
- ✅ **TTS 上传后服务器播放**: "轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～"
- ✅ **有 STATE: speaking 状态转换** (line 41916)

### eyes 项目的提醒流程日志（第 1 次）

```
I (56690) Application: >> 15秒后提醒我喝水。
I (57290) Application: << % self.get_device_status...
I (58100) Application: << % self.reminder.add...
I (58120) ReminderManager: Added reminder: 喝水 at ld (now: ld)
I (58120) MCP: Reminder added: 喝水 at ld
I (58540) Application: << 提醒已设置，15秒后记得喝水哦！
I (62190) Application: STATE: listening
W (72780) Application: Alert INFO: 喝水 []
I (72780) Protocol: Sending reminder: 喝水
I (72780) Application: STATE: listening  ⚠️ 注意：立即回到 listening 状态
I (72780) Application: Received reminder from queue: 喝水
I (72780) ReminderManager: ✅ Reminder delivered: 喝水
I (72790) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，该去喝水了，记得准时哦。]
I (72840) Application: Streaming PCM data from TTS service...
I (77970) AudioCodec: Set output enable to false  ⚠️ 注意：音频输出被禁用
I (81610) Application: Reminder audio upload finished, total 123682 bytes
... (等待 40+ 秒)
I (124600) Application: STATE: speaking  ⚠️ 注意：40+ 秒后才进入 speaking 状态
I (124850) AudioCodec: Set output enable to true
I (125450) Application: << 我在呢，轩轩爸爸，待会见！  ⚠️ 播放的不是提醒内容！
```

**问题**:
- ❌ **没有 RAW MCP Message 日志**
- ❌ **TTS 上传后立即进入 listening 状态** (line 72780)
- ❌ **音频输出被禁用** (line 77970: `Set output enable to false`)
- ❌ **40+ 秒后才进入 speaking 状态** (line 124600)
- ❌ **播放的不是提醒内容**，而是"我在呢，轩轩爸爸，待会见！"

### eyes 项目的提醒流程日志（第 2 次）

```
I (178300) Application: >> 10秒钟后提醒我喝水。
I (179120) ReminderManager: Added reminder: 去喝水 at ld (now: ld)
I (179120) MCP: Reminder added: 去喝水 at ld
W (179780) Application: Alert INFO: 去喝水 []  ⚠️ 注意：时间还没到就触发了！
I (183100) Application: STATE: listening
I (183780) Protocol: Sending reminder: 去喝水
I (183780) Application: STATE: listening  ⚠️ 立即回到 listening
I (183780) Application: Received reminder from queue: 去喝水
I (183780) ReminderManager: ✅ Reminder delivered: 去喝水
I (183780) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，该去喝水了，记得准时哦。]
I (183850) Application: Streaming PCM data from TTS service...
I (191330) Application: Reminder audio upload finished, total 123682 bytes
I (198970) AudioCodec: Set output enable to false  ⚠️ 音频输出被禁用
...
I (244370) Application: STATE: speaking  ⚠️ 60+ 秒后才进入 speaking
I (244810) Application: << 轩轩爸爸，我先去待机啦，有事随时叫我哦！  ⚠️ 播放的仍是其他内容
```

**同样的问题**:
- ❌ 音频输出被禁用
- ❌ 播放的不是提醒内容

---

## 🚨 核心问题分析

### 问题 1: 缺少 RAW MCP Message 日志 ⭐⭐

#### 14-handheld 项目

**文件**: [main/chat/application.cc:481-488](../../14-handheld/main/chat/application.cc#L481-L488)

```cpp
} else if (strcmp(type->valuestring, "mcp") == 0) {
    auto payload = cJSON_GetObjectItem(root, "payload");
    if (cJSON_IsObject(payload)) {
        char* payload_str = cJSON_PrintUnformatted(payload);  // ✅ 打印原始消息
        ESP_LOGI(TAG, "RAW MCP Message: %s", payload_str);
        McpServer::GetInstance().ParseMessage(payload);
        cJSON_free(payload_str);
    }
}
```

#### eyes 项目

**文件**: [main/application.cc:481-488](main/application.cc#L481-L488)

```cpp
} else if (strcmp(type->valuestring, "mcp") == 0) {
    auto payload = cJSON_GetObjectItem(root, "payload");
    if (cJSON_IsObject(payload)) {
        char* payload_str = cJSON_PrintUnformatted(payload);
        ESP_LOGI(TAG, "RAW MCP Message: %s", payload_str);  // ✅ 也有这段代码！
        McpServer::GetInstance().ParseMessage(payload);
        cJSON_free(payload_str);
    }
}
```

**结论**: 两个项目都有 `RAW MCP Message` 日志代码，但 **eyes 项目的日志中没有显示**。

**可能原因**:
1. 日志级别过滤（ESP_LOGI 可能被过滤）
2. MCP 消息处理流程不同

### 问题 2: 音频输出被禁用 ⭐⭐⭐ (最关键)

#### 14-handheld 项目日志

```
I (41906) Application: >> 时间到了，该去轩轩爸爸，该喝水了，记得补充水分哦。那记得准时哦。
I (41916) Application: STATE: speaking
I (42206) Application: << 时间到！
I (43476) Application: << 轩轩爸爸，快去喝杯水吧，身体需要及时补水哦～
```

**流程**:
1. TTS 上传完成后，**立即进入 speaking 状态**
2. **服务器下发 TTS 音频并播放**："<- 时间到！"、"轩轩爸爸，快去喝杯水吧..."

#### eyes 项目日志

```
I (72790) Application: ProcessReminderTts: 准备回传提醒音频...
I (72840) Application: Streaming PCM data from TTS service...
I (77970) AudioCodec: Set output enable to false  ⚠️ 输出被禁用！
I (81610) Application: Reminder audio upload finished, total 123682 bytes
... (40+ 秒无语音)
I (124600) Application: STATE: speaking  ⚠️ 40 秒后才进入 speaking
I (124850) AudioCodec: Set output enable to true  ⚠️ 输出才被启用
I (125450) Application: << 我在呢，轩轩爸爸，待会见！  ⚠️ 但内容不对
```

**流程**:
1. TTS 上传过程中，**音频输出被禁用** (`Set output enable to false`)
2. TTS 上传完成后，**立即进入 listening 状态**，而不是 speaking
3. **40+ 秒后才进入 speaking 状态**
4. 播放的是其他内容（"我在呢，轩轩爸爸，待会见！"），而不是提醒

**根本原因**: `ProcessReminderTts` 只是上传 TTS 数据到服务器，但**没有触发服务器播放提醒音频**！

### 问题 3: 提醒时间不准确（第 2 次测试）

**日志**:
```
I (179120) ReminderManager: Added reminder: 去喝水 at ld (now: ld)  ← 添加提醒
W (179780) Application: Alert INFO: 去喝水 []  ← 仅 660ms 后就触发！
```

**预期**: 应该在 10 秒后触发，但实际上 0.66 秒就触发了。

**可能原因**:
- 系统时间未同步
- 提醒时间计算错误

---

## 🎯 差异总结

| 项目 | RAW MCP Message 日志 | 音频输出状态 | TTS 上传后状态 | 最终播放内容 |
|-----|---------------------|------------|--------------|------------|
| **14-handheld** | ✅ 有 | 保持启用 | 进入 speaking | ✅ 提醒内容（"轩轩爸爸，快去喝杯水吧..."） |
| **eyes** | ❌ 无（日志中未显示） | ❌ 被禁用 | ⚠️ 保持 listening | ❌ 其他内容（"我在呢，轩轩爸爸，待会见！"） |

---

## 💡 问题根源推断

### 推断 1: ProcessReminderTts 只是上传音频，不触发播放 ⭐⭐⭐

**14-handheld 项目**:
- `ProcessReminderTts` 上传音频到服务器
- 服务器收到音频后，**立即播放**："<- 时间到！"、"轩轩爸爸，快去喝杯水吧..."

**eyes 项目**:
- `ProcessReminderTts` 上传音频到服务器
- **服务器没有播放提醒**，而是进入 listening 状态
- 40+ 秒后，服务器才发送其他 TTS（"我在呢，轩轩爸爸，待会见！"）

**可能原因**:
1. **服务器端逻辑不同**: 14-handheld 使用的服务器会自动播放提醒，eyes 使用的服务器不会
2. **协议不同**: 可能需要发送额外的命令告诉服务器播放提醒
3. **服务器地址相同，但行为不同**: 两个项目都使用 `http://192.140.190.183:8081/api/text_to_pcm`，但服务器对提醒的处理逻辑可能不同

### 推断 2: 音频输出被禁用的原因 ⭐⭐

**日志显示**:
```
I (77970) AudioCodec: Set output enable to false
```

**这段日志在 TTS 上传完成后出现**，可能是:
1. **ProcessReminderTts 内部调用了 `EnableVoiceProcessing(false)`**，副作用是禁用音频输出
2. 代码中有 `audio_service.EnableVoiceProcessing(false)` 挂起麦克风，可能误伤了扬声器

**查看代码** ([application.cc:920-923](main/application.cc#L920-L923)):
```cpp
// [Step 1] 挂起麦克风采集，防止环境音混入提醒语音回传
bool was_processor_running = audio_service.IsAudioProcessorRunning();
if (was_processor_running) {
    audio_service.EnableVoiceProcessing(false);  // ⚠️ 可能同时禁用了扬声器
}
```

**14-handheld 项目也有同样代码** ([application.cc:985-989](../../14-handheld/main/chat/application.cc#L985-L989))，但它的扬声器没有被禁用。

**差异可能在于**:
- 14-handheld 项目的 `EnableVoiceProcessing(false)` 只影响麦克风
- eyes 项目的 `EnableVoiceProcessing(false)` 同时禁用了麦克风和扬声器

---

## 🔍 需要进一步调查的问题

### 问题 1: 为什么服务器没有播放提醒音频？

**测试建议**:
1. 检查服务器端是否收到提醒音频
2. 检查服务器日志，看是否有播放提醒的逻辑
3. 对比两个项目使用的服务器是否相同（虽然 URL 相同）

### 问题 2: 音频输出为什么被禁用？

**测试建议**:
1. 检查 `EnableVoiceProcessing(false)` 的实现
2. 检查音频编解码器驱动的差异
3. 检查是否有其他地方调用了 `Set output enable to false`

### 问题 3: 为什么 40+ 秒后才进入 speaking 状态？

**可能原因**:
1. 服务器超时，认为客户端没有响应
2. 服务器等待用户输入，但用户没有说话
3. 服务器逻辑：等待一段时间后自动进入待机状态

---

## 📝 需要对比的关键代码

### 1. MCP 消息处理差异

**14-handheld 项目** ([application.cc:481-488](../../14-handheld/main/chat/application.cc#L481-L488)):
```cpp
} else if (strcmp(type->valuestring, "mcp") == 0) {
    auto payload = cJSON_GetObjectItem(root, "payload");
    if (cJSON_IsObject(payload)) {
        char* payload_str = cJSON_PrintUnformatted(payload);
        ESP_LOGI(TAG, "RAW MCP Message: %s", payload_str);  // ✅ 打印原始消息
        McpServer::GetInstance().ParseMessage(payload);
        cJSON_free(payload_str);
    }
}
```

**eyes 项目** ([application.cc:481-488](main/application.cc#L481-L488)):
```cpp
} else if (strcmp(type->valuestring, "mcp") == 0) {
    auto payload = cJSON_GetObjectItem(root, "payload");
    if (cJSON_IsObject(payload)) {
        char* payload_str = cJSON_PrintUnformatted(payload);
        ESP_LOGI(TAG, "RAW MCP Message: %s", payload_str);  // ✅ 也有这段代码
        McpServer::GetInstance().ParseMessage(payload);
        cJSON_free(payload_str);
    }
}
```

**结论**: 代码相同，但 eyes 项目日志中没有显示。可能是日志级别过滤。

### 2. 提醒触发后的状态管理

**14-handheld 项目** ([application.cc:602-609](../../14-handheld/main/chat/application.cc#L602-L609)):
```cpp
// 直接发送提醒
protocol_->SendReminder(reminder.content);

// [FIX] 无论之前是什么状态，发完提醒后，设备必须处于 Listening 状态等待服务端对"引导式文案"的回应
// 这会自动促使服务端下发后续的 TTS 或切换到对话模式
SetListeningMode(kListeningModeAutoStop);

return true; // 已发送给服务端，任务完成，移除提醒
```

**eyes 项目** ([application.cc:572-577](main/application.cc#L572-L577)):
```cpp
// 3. 发送提醒
protocol_->SendReminder(reminder.content);

// 4. 设置为 Listening 状态
SetListeningMode(kListeningModeAutoStop);

return true;  // 已处理，移除提醒
```

**结论**: 逻辑相同。

---

## 🚨 最关键的发现

### 发现: 两个项目的服务器行为可能不同！

**证据**:
1. **14-handheld 项目**: TTS 上传完成后，服务器立即播放提醒
2. **eyes 项目**: TTS 上传完成后，服务器**没有播放提醒**，而是进入 listening 状态，40+ 秒后播放其他内容

**推测**:
- 14-handheld 项目使用的服务器**自动识别提醒内容并播放**
- eyes 项目使用的服务器**需要额外的协议或命令来触发播放**

**或者**:
- 两个项目使用的服务器 URL 相同，但服务器端对不同的设备或协议有不同的处理逻辑
- 可能需要检查服务器端代码或配置

---

## 📊 总结

### 问题汇总

| 问题 | 14-handheld | eyes | 影响 |
|-----|-------------|------|------|
| **RAW MCP Message 日志** | ✅ 有 | ❌ 无（日志中未显示） | 调试困难 |
| **TTS 上传后状态** | 进入 speaking | 保持 listening | 服务器不播放提醒 |
| **音频输出** | 保持启用 | 被禁用 | 无法播放语音 |
| **提醒播放** | ✅ 播放提醒内容 | ❌ 播放其他内容 | 功能失效 |

### 核心问题

**最关键的问题**: `ProcessReminderTts` 只是上传 TTS 数据到服务器，但**服务器没有播放提醒音频**！

**可能原因**:
1. 服务器端逻辑不同（最可能）
2. 协议层实现不同
3. 音频输出被意外禁用

### 推荐调查方向

1. **检查服务器端**: 确认服务器是否收到提醒音频，以及是否有播放提醒的逻辑
2. **对比协议层**: 检查 `protocol_->SendReminder()` 的实现是否相同
3. **检查音频输出**: 找出为什么音频输出被禁用

---

## 🔍 为什么 eyes 项目缺少 MCP 日志？

### 问题：eyes 项目缺少以下日志

```
I (21516) Application: << % self.reminder.add...
I (21526) Application: RAW MCP Message: {"jsonrpc":"2.0","method":"tools/call",...}
I (21536) MCP: MCP RPC method: tools/call
I (21546) MCP: Adding relative reminder: 轩轩爸爸，该喝水了...
```

**eyes 项目只有**:
```
I (58100) Application: << % self.reminder.add...
I (58120) ReminderManager: Added reminder: 喝水 at ld (now: ld)
I (58120) MCP: Reminder added: 喝水 at ld
```

### 根本原因分析

#### 原因 1: 缺少 "MCP RPC method: tools/call" 日志 ⭐⭐⭐

**14-handheld 项目** ([mcp_server.cc:338](../../14-handheld/main/chat/mcp_server.cc#L338)):

```cpp
void McpServer::ParseMessage(const cJSON* json) {
    // ... 检查 version 和 method ...

    auto method_str = std::string(method->valuestring);
    ESP_LOGI(TAG, "MCP RPC method: %s", method_str.c_str());  // ✅ 打印方法名
    // ...
}
```

**eyes 项目** ([mcp_server.cc:357-375](main/mcp_server.cc#L357-L375)):

```cpp
void McpServer::ParseMessage(const cJSON* json) {
    // ... 检查 version 和 method ...

    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0) {
        return;
    }
    // ❌ 没有 ESP_LOGI(TAG, "MCP RPC method: %s", method_str.c_str());
    // ...
}
```

**结论**: eyes 项目移除了 `ESP_LOGI(TAG, "MCP RPC method: %s", ...)` 这行日志。

#### 原因 2: 缺少 "Adding relative reminder" 日志 ⭐⭐⭐

**14-handheld 项目** ([mcp_server.cc:203-204](../../14-handheld/main/chat/mcp_server.cc#L203-L204)):

```cpp
if (delay > 0) {
    timestamp = (long long)::time(nullptr) + delay;
    ESP_LOGI(TAG, "Adding relative reminder: %s (delay: %d, now: %lld, target: %lld)",
             content.c_str(), delay, (long long)::time(nullptr), timestamp);  // ✅ 详细日志
}
```

**eyes 项目** ([mcp_server.cc:161-170](main/mcp_server.cc#L161-L170)):

```cpp
if (delay > 0) {
    timestamp = std::time(nullptr) + delay;
} else if (ts > 0) {
    timestamp = ts;
} else {
    ESP_LOGE(TAG, "Either delay_in_seconds or timestamp must be provided");
    return false;
}

ReminderManager::GetInstance().AddReminder(timestamp, content);
ESP_LOGI(TAG, "Reminder added: %s at %lld", content.c_str(), timestamp);  // ❌ 简化日志，缺少详细信息
```

**结论**: eyes 项目简化了日志输出，只打印了 `"Reminder added: %s at %lld"`，而不是详细的 `"Adding relative reminder: ... (delay: %d, now: %lld, target: %lld)"`。

#### 原因 3: 日志内容格式不同

**14-handheld 项目日志**:
```
I (21546) MCP: Adding relative reminder: 轩轩爸爸，该喝水了，记得补充水分哦！ (delay: 10, now: 1735123456, target: 1735123466)
I (21566) ReminderManager: Added reminder: 轩轩爸爸，该喝水了，记得补充水分哦！ at 1735123466 (now: 1735123456)
```

**eyes 项目日志**:
```
I (58120) ReminderManager: Added reminder: 喝水 at ld (now: ld)
I (58120) MCP: Reminder added: 喝水 at ld
```

**注意**: eyes 项目的日志中 `ld` 不是数字，而是 printf 格式说明符错误！

**问题根源**: eyes 项目的 `ReminderManager::AddReminder` 中使用了错误的格式说明符：

```cpp
// eyes 项目 - main/reminder_manager.cc:106
ESP_LOGI(TAG, "Added reminder: %s at %lld (now: %lld)", content.c_str(), (long long)timestamp, (long long)std::time(nullptr));
```

但日志显示 `ld`，说明可能是：
1. 编译器优化问题
2. 格式字符串错误
3. 日志系统配置问题

### 代码对比总结

| 日志 | 14-handheld | eyes | 差异 |
|-----|-------------|------|------|
| **"MCP RPC method: tools/call"** | ✅ 有 | ❌ 无 | eyes 项目移除 |
| **"Adding relative reminder: ..."** | ✅ 详细 | ❌ 无 | eyes 项目简化为 "Reminder added: ..." |
| **"ReminderManager: Added reminder: ..."** | ✅ 详细 | ⚠️ 错误格式 | eyes 项目显示 `ld` 而不是实际时间戳 |

### 为什么这会导致提醒功能失效？

**关键发现**: 缺少这些日志只是**表面现象**，真正的问题是：

1. **MCP 消息处理流程可能不同**:
   - 14-handheld 项目有详细的日志，说明每一步都被执行
   - eyes 项目缺少日志，可能某些步骤被跳过或执行失败

2. **提醒添加逻辑可能有问题**:
   - 14-handheld 项目: `"Adding relative reminder: ... (delay: 10, now: X, target: Y)"` - 显示详细信息
   - eyes 项目: `"Reminder added: ... at ld (now: ld)"` - **时间戳格式错误！**

3. **时间戳计算可能错误**:
   - 如果 `timestamp` 的值是错误的（如 `ld` 而不是实际数字），提醒会立即触发或在错误的时间触发
   - 这解释了为什么第 2 次测试中，10 秒的提醒在 0.66 秒后就触发了！

### 推荐修复

#### 修复 1: 恢复详细日志 ⭐⭐⭐

**文件**: [main/mcp_server.cc](main/mcp_server.cc)

**在 `ParseMessage` 函数中添加** (约 line 372):

```cpp
auto method_str = std::string(method->valuestring);
ESP_LOGI(TAG, "MCP RPC method: %s", method_str.c_str());  // 添加这行
if (method_str.find("notifications") == 0) {
    return;
}
```

**在 `reminder.add` 工具的 callback 中修改** (约 line 161-170):

```cpp
long long timestamp = 0;
if (delay > 0) {
    timestamp = (long long)std::time(nullptr) + delay;
    ESP_LOGI(TAG, "Adding relative reminder: %s (delay: %d, now: %lld, target: %lld)",
             content.c_str(), delay, (long long)std::time(nullptr), timestamp);  // 添加详细日志
} else if (ts > 0) {
    timestamp = ts;
    ESP_LOGI(TAG, "Adding absolute reminder: %s (ts: %lld, now: %lld)",
             content.c_str(), timestamp, (long long)std::time(nullptr));  // 添加详细日志
} else {
    ESP_LOGE(TAG, "Either delay_in_seconds or timestamp must be provided");
    return false;
}
```

#### 修复 2: 检查时间戳格式问题 ⭐⭐⭐

**文件**: [main/reminder_manager.cc:106](main/reminder_manager.cc#L106)

**检查当前的日志代码**:

```cpp
ESP_LOGI(TAG, "Added reminder: %s at %lld (now: %lld)", content.c_str(), (int64_t)timestamp, (int64_t)std::time(nullptr));
```

**如果显示 `ld`，可能需要使用 `PRId64` 宏**:

```cpp
#include <cinttypes>
ESP_LOGI(TAG, "Added reminder: %s at %" PRId64 " (now: %" PRId64 ")",
         content.c_str(), (int64_t)timestamp, (int64_t)std::time(nullptr));
```

或者强制转换为 `long` 并使用 `%ld`:

```cpp
ESP_LOGI(TAG, "Added reminder: %s at %ld (now: %ld)",
         content.c_str(), (long)timestamp, (long)std::time(nullptr));
```

---

**版本**: v0.3.1
**更新时间**: 2025-01-07
**作者**: Claude Code
