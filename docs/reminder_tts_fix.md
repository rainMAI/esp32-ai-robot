# 提醒功能本地语音播放修复

## 问题描述

提醒到时间后，**没有本地提示音播放**，只有：
1. 本地弹窗显示
2. 上传音频到服务器
3. 服务器返回 TTS（有时）

### 日志分析

```
W (212810) Application: Alert INFO: 去给萱萱收碗 []  ← emotion 为空
I (212810) Protocol: Sending reminder: 去给萱萱收碗
I (212820) Application: Received reminder from queue: 去给萱萱收碗
I (212830) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，该去给萱萱收碗了，记得准时哦。]
I (212880) Application: Streaming PCM data from TTS service...
I (219320) AudioCodec: Set output enable to false  ← 音频输出被禁用
I (222940) Application: Reminder audio upload finished, total 141346 bytes
```

**问题**：
- ❌ 没有播放 `OGG_POPUP` 提示音
- ❌ 服务器返回的 TTS 可能被延迟或丢失

## 根本原因

对比 **14-handheld** 和 **eyes** 项目：

### 14-handheld（工作正常）

```cpp
// main/chat/application.cc:583
Alert(Lang::Strings::INFO, reminder.content.c_str(), "bell", Lang::Sounds::OGG_POPUP);
//                                                        ^^^^^           ^^^^^^^^^^^^
//                                                        图标             播放声音 ✅
```

### eyes 修复前（不工作）

```cpp
// main/application.cc:562
Alert("INFO", reminder.content.c_str(), "", "");
//                                         ^^  ^^
//                                         空   空  ❌
```

### eyes 修复后（应该工作）

```cpp
// main/application.cc:562
Alert("INFO", reminder.content.c_str(), "bell", Lang::Sounds::OGG_POPUP);
//                                          ^^^^^  ^^^^^^^^^^^^^^^^^^^^
//                                          图标    播放提示音 ✅
```

## 提醒流程说明

### 完整流程（14-handheld）

1. **提醒到期** → `ReminderManager::ProcessDueReminders()`
2. **本地反馈**：
   - 显示弹窗：`Alert(INFO, "去给萱萱收碗", "bell", OGG_POPUP)`
   - 播放提示音：`audio_service_.PlaySound(Lang::Sounds::OGG_POPUP)` ✅
3. **发送给服务器**：`protocol_->SendReminder(content)`
4. **队列处理**：`QueueReminderTts(content)` → `ProcessReminderTts(content)`
5. **ProcessReminderTts 作用**：
   - 生成提醒文本："时间到了，该去给萱萱收碗了..."
   - **上传音频到服务器**（用于服务器端识别用户说的内容）
   - **不是用于本地播放！**
6. **服务器返回**：服务器应该返回 TTS 音频播放

### 关键点

**`ProcessReminderTts` 的作用**：
- ✅ 上传提醒文本的 PCM 音频到服务器
- ❌ **不是**用于本地播放
- 目的：让服务器"听到"提醒内容，然后进行后续对话

**本地播放来源**：
- ✅ **`Alert()` 函数中的 `PlaySound(OGG_POPUP)`**
- ✅ 服务器返回的 TTS 音频（如果有）

## 修复内容

### 文件修改

**[main/application.cc:562](../main/application.cc#L562)**

```diff
  // 1. 本地弹窗提示 + 播放提示音
  if (!reminder.local_alert_shown) {
-     Alert("INFO", reminder.content.c_str(), "", "");
+     Alert("INFO", reminder.content.c_str(), "bell", Lang::Sounds::OGG_POPUP);
      reminder.local_alert_shown = true;
  }
```

### 修改说明

1. **添加图标**：`"bell"`（铃铛图标）
2. **添加提示音**：`Lang::Sounds::OGG_POPUP`
   - 这是内置的 "叮" 声音
   - 文件：`main/assets/embedded_files.cc` 中嵌入
   - 格式：OGG Vorbis

## 预期效果（修复后）

### 日志应该显示

```
I (212810) Application: Alert INFO: 去给萱萱收碗 [bell]
I (212810) AudioService: Playing sound: popup.ogg  ← 应该看到这个
I (212810) Protocol: Sending reminder: 去给萱萱收碗
...
I (212830) Application: ProcessReminderTts: 准备回传提醒音频: [时间到了，该去给萱萱收碗了，记得准时哦。]
I (212880) Application: Streaming PCM data from TTS service...
I (212920) AudioCodec: Set output enable to false  ← 播放提示音
I (212950) AudioCodec: Set output enable to true   ← 恢复
I (222940) Application: Reminder audio upload finished
```

### 用户应该听到

1. **"叮"** 的一声（`OGG_POPUP` 提示音）✅
2. 服务器返回的 TTS（如果有）：`"时间到了，该去给萱萱收碗了..."`

## 如果服务器不返回 TTS

如果服务器没有返回 TTS 音频，这是服务器端的问题，不是设备问题。

### 可能的原因

1. **服务器配置**：服务器的提醒 TTS 功能未启用
2. **服务器逻辑**：服务器认为提醒已经通过本地提示音播放过了
3. **网络问题**：服务器返回的 TTS 数据包丢失

### 解决方案

**方案 A：联系服务器开发者**
- 检查服务器是否返回 TTS
- 查看服务器日志

**方案 B：使用本地 TTS（备用）**
如果需要完全本地化的提醒 TTS，可以：
1. 使用 ESP-SR 的本地 TTS
2. 或者预录提醒音频
3. 或者在 `ProcessReminderTts` 结束后直接播放 PCM 数据

## 对比两个项目

### 14-handheld

```cpp
// main/chat/application.cc:583
Alert(Lang::Strings::INFO, reminder.content.c_str(),
      "bell", Lang::Sounds::P3_POPUP);  // 使用 P3_POPUP
```

### eyes（修复后）

```cpp
// main/application.cc:562
Alert("INFO", reminder.content.c_str(),
      "bell", Lang::Sounds::OGG_POPUP);  // 使用 OGG_POPUP
```

**差异**：
- 14-handheld: `Lang::Strings::INFO`（枚举）vs eyes: `"INFO"`（字符串）
- 14-handheld: `P3_POPUP` vs eyes: `OGG_POPUP`
- 两者功能相同，只是常量定义不同

## 验证步骤

编译并烧录后，设置一个 15 秒提醒：

```
你："15秒后提醒我去给萱萱收碗"
设备："提醒已设置！15秒后我会提醒您去给萱萱收碗哦～ ⏰"

（15秒后）

设备："叮！"  ← OGG_POPUP 提示音 ✅
设备：显示弹窗 "去给萱萱收碗" [铃铛图标]
设备：（服务器返回）"时间到了，该去给萱萱收碗了，记得准时哦。"
```

## 相关文件

- [main/application.cc:562](../main/application.cc#L562) - 提醒 Alert 调用
- [main/assets/lang_config.h:173](../main/assets/lang_config.h#L173) - OGG_POPUP 定义
- [main/audio/audio_service.cc](../main/audio/audio_service.cc) - PlaySound 实现
- [main/protocols/protocol.cc:85](../main/protocols/protocol.cc#L85) - SendReminder 实现

## 总结

✅ **已修复**：添加本地提示音播放
✅ **方法**：`Alert(..., "bell", Lang::Sounds::OGG_POPUP)`
✅ **预期效果**：提醒到时间时播放 "叮" 声
⚠️ **服务器 TTS**：取决于服务器实现，不是设备问题

---

**修复日期**：2025-01-07
**影响版本**：v0.3.2+
**参考项目**：14-handheld（工作正常的参考实现）
