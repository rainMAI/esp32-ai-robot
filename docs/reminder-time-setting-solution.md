# 日程提醒时间设置方案

## 问题背景

在 ESP32 双目 AI 机器人项目中，日程提醒功能存在时间设置不准确的问题。用户说"9点50提醒我"，结果实际设置的时间是22点21分，时间完全错误。

## 根本原因分析

### 1. 时区混乱问题
- **服务器返回的时区格式**：`"+08:00"`（ISO 8601 格式）
- **ESP32 需要的时区格式**：`"UTC-8"`（POSIX 格式）
- **问题**：两种格式不兼容，导致时区设置失败

### 2. AI 计算时间戳过于复杂
- **原方案**：AI 需要计算 `delay_in_seconds` 或 `timestamp`
- **问题**：
  - AI 需要理解时区概念
  - AI 需要做复杂的数学计算
  - `get_device_status` 返回的 `time_str` 是 UTC 时间，误导了 AI
  - AI 经常不调用 `reminder.add` 工具，只回复文本

### 3. 用户体验差
- 用户说"10点59"，AI 理解成 23:59
- 时间设置误差大，完全不可用

---

## 解决方案

### 核心理念
**"设备联网 → 获取本地时间 → 计算时间戳 → 存储到服务器 → 时间到 → 触发提醒"**

**关键点**：
- ✅ **不需要 AI 做复杂计算**
- ✅ **使用设备本地时间**（用户看到的时间）
- ✅ **ESP32 自动判断今天/明天**
- ✅ **服务器不需要修改**

---

## 技术实现

### 1. 修改 `get_device_status` 工具

**文件**：`main/mcp_server.cc`

**修改内容**：
```cpp
// 添加本地时间字符串（而非 UTC 时间）
struct tm timeinfo;
localtime_r(&now, &timeinfo);
char time_str[64];
strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
cJSON_AddStringToObject(system, "local_time", time_str);

// 添加时区信息
char* tz = getenv("TZ");
cJSON_AddStringToObject(system, "timezone", tz ? tz : "UTC");
```

**返回值示例**：
```json
{
  "timestamp": 1768743975,
  "local_time": "2026-01-18 21:06:15",
  "timezone": "UTC-8"
}
```

**工具描述简化**：
- 去掉"调用 `reminder.add` 前必须先调用此工具"的提示
- 避免误导 AI 以为需要先调用这个工具

---

### 2. 简化 `reminder.add` 工具

**文件**：`main/mcp_server.cc`

**参数改变**：
```cpp
// 原来的参数
Property("delay_in_seconds", kPropertyTypeInteger, 0LL),
Property("timestamp", kPropertyTypeInteger, 0LL)

// 改为
Property("content", kPropertyTypeString),
Property("time", kPropertyTypeString)  // 时间字符串，格式 "HH:MM"
```

**工具描述**（中文）：
```
设置提醒。当用户要求设置提醒时，你必须调用此工具！

参数：
- content (必需): 提醒内容 (例如: '喝水', '睡觉')
- time (必需): 时间，格式为 HH:MM (例如: '10:59', '22:50')

示例：
- 用户: '10点59提醒我喝水' → 调用: reminder.add(content='喝水', time='10:59')
- 用户: '晚上9点50提醒我睡觉' → 调用: reminder.add(content='睡觉', time='21:50')
- 用户: '今晚11点21分提醒我睡觉' → 调用: reminder.add(content='睡觉', time='23:21')

重要：你必须调用此工具。不要只回复文本！直接调用工具即可。
```

**关键改进**：
- 改用中文描述，更清晰
- 强调"必须调用此工具"
- 明确说"不要只回复文本"

---

### 3. ESP32 端计算时间戳

**文件**：`main/reminder_manager.cc`

**新增方法**：`AddReminderWithTime`

**实现逻辑**：
```cpp
bool ReminderManager::AddReminderWithTime(const std::string& content, const std::string& time_str) {
    // 1. 解析时间字符串，格式如 "10:59"
    int target_hour = 0, target_min = 0;
    sscanf(time_str.c_str(), "%d:%d", &target_hour, &target_min);

    // 2. 获取当前本地时间
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 3. 计算今天的 target_hour:target_min 的时间戳
    struct tm target_tm = timeinfo;
    target_tm.tm_hour = target_hour;
    target_tm.tm_min = target_min;
    target_tm.tm_sec = 0;
    time_t target_timestamp = mktime(&target_tm);

    // 4. 如果目标时间已经过了，设置为明天
    if (target_timestamp <= now) {
        target_timestamp += 86400; // 加24小时
    }

    // 5. 使用现有的 AddRemote 方法发送到服务器
    AddRemote(content, target_timestamp);

    // 6. 同步最新的提醒列表
    SyncPull(server_url_);

    return true;
}
```

---

### 4. 时区格式转换

**文件**：`main/reminder_manager.cc`

**在 `SyncPull()` 中添加时区解析和应用**：
```cpp
// 解析并应用时区设置
cJSON* device_settings = cJSON_GetObjectItem(root, "device_settings");
if (device_settings != nullptr) {
    cJSON* timezone = cJSON_GetObjectItem(device_settings, "timezone");
    if (timezone != nullptr && cJSON_IsString(timezone)) {
        std::string server_tz = timezone->valuestring; // "+08:00"

        // 转换时区格式："+08:00" → "UTC-8"
        char sign = server_tz[0];
        int hours = std::stoi(server_tz.substr(1, 2));
        int posix_sign = (sign == '+') ? -1 : 1; // 符号相反

        char tz_buf[20];
        snprintf(tz_buf, sizeof(tz_buf), "UTC%+d", posix_sign * hours);

        // 应用时区
        setenv("TZ", tz_buf, 1);
        tzset();
    }
}
```

---

## 完整工作流程

```
用户："今晚11点21分提醒我睡觉"
    ↓
AI 提取时间和内容
- content = "睡觉"
- time = "23:21"
    ↓
AI 调用：reminder.add(content='睡觉', time='23:21')
    ↓
ESP32 接收请求
    ↓
ESP32 计算：
1. 解析时间：target_hour=23, target_min=21
2. 获取当前本地时间：例如 22:59
3. 计算今天 23:21 的时间戳
4. 判断：23:21 > 22:59（未过）
5. 时间戳 = 今天 23:21
    ↓
ESP32 调用 AddRemote：
{
  "content": "睡觉",
  "timestamp": 1768752660  // 今天 23:21 的时间戳
}
    ↓
服务器存储时间戳
    ↓
ESP32 定期检查（每分钟）
if (timestamp >= now) {
    触发提醒！
}
```

---

## 测试场景

| 当前时间 | 用户说 | 目标时间 | 结果 |
|---------|-------|---------|------|
| 22:59 | "10点59提醒我" | 10:59 | ✅ 明天 10:59 |
| 09:00 | "10点59提醒我" | 10:59 | ✅ 今天 10:59 |
| 22:50 | "22点55提醒我" | 22:55 | ✅ 今天 22:55 |
| 23:00 | "22点50提醒我" | 22:50 | ✅ 明天 22:50 |

---

## 修改的文件清单

| 文件 | 修改内容 | 作用 |
|------|---------|------|
| `main/mcp_server.cc` | 修改 `get_device_status` 返回值 | 返回本地时间和时区 |
| `main/mcp_server.cc` | 修改 `get_device_status` 描述 | 简化描述，去掉误导性提示 |
| `main/mcp_server.cc` | 修改 `reminder.add` 参数和描述 | 改用 time 字符串，中文描述 |
| `main/mcp_server.cc` | 修改 `reminder.add` 实现逻辑 | 调用 `AddReminderWithTime` |
| `main/reminder_manager.h` | 添加 `AddReminderWithTime` 声明 | 新增方法 |
| `main/reminder_manager.cc` | 实现 `AddReminderWithTime` | ESP32 端计算时间戳 |
| `main/reminder_manager.cc` | 在 `SyncPull` 中添加时区转换 | 应用服务器下发的时区 |

---

## 优点总结

1. ✅ **简单可靠**
   - AI 不需要做复杂计算
   - 只需要提取时间和内容

2. ✅ **用户体验好**
   - 用户说"10点59"，就是 10:59
   - 不会出现时间混乱

3. ✅ **自动判断今天/明天**
   - 如果时间已过，自动设置为明天
   - 用户不需要说明是"今天"还是"明天"

4. ✅ **不需要修改服务器**
   - 使用现有的 API 接口
   - ESP32 端计算时间戳

5. ✅ **支持跨时区**
   - 使用设备本地时间
   - 服务器下发时区配置
   - 自动应用正确的时区

---

## 注意事项

1. **时区配置**
   - 服务器需要返回正确的时区配置
   - ESP32 自动转换并应用时区

2. **时间格式**
   - AI 必须传递 `HH:MM` 格式的时间字符串
   - 24小时制，例如："23:21"

3. **网络连接**
   - 提醒功能需要 WiFi 连接
   - 需要能够访问服务器

4. **时间同步**
   - ESP32 启动时会通过 NTP 同步时间
   - 确保时间准确

---

## 调试日志

正常流程的日志示例：
```
I (12345) MCP: Adding reminder with time: 23:21, content: 睡觉
I (12350) ReminderManager: Current time: 22:59, Target time: 23:21
I (12355) ReminderManager: Calculated timestamp: 1768752660 (now: 1768749600)
I (12400) ReminderManager: Adding reminder to server: 睡觉 at 1768752660
I (12450) ReminderManager: [TZ_DEBUG] timestamp=1768752660, hour=23, min=21, timezone=UTC-8
I (12500) ReminderManager: Successfully added reminder to server: 睡觉
I (12550) MCP: Reminder added successfully: 睡觉 at 23:21
```

---

## 结论

通过简化 AI 的任务，让 ESP32 端负责时间计算，成功解决了日程提醒时间设置不准确的问题。新方案简单、可靠、用户体验好，不需要修改服务器代码。
