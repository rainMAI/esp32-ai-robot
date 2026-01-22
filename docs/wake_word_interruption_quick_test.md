# 单麦克风实时打断 - 快速测试指南

## ✅ 功能已完全实现！

您的系统**已经支持**单麦克风实时打断对话功能，无需任何额外配置！

---

## 🚀 快速测试

### 步骤 1: 编译并烧录

```bash
cd D:\code\eyes
idf.py build flash monitor
```

### 步骤 2: 测试场景

```
场景：AI 讲故事时打断
─────────────────────────────────────
1. 对设备说："给我讲个故事"
   → AI 开始讲故事

2. 在 AI 讲故事过程中，清晰地说："你好小智"
   → AI 应该立即停止讲故事 ✅

3. 设备进入监听模式
   → 眼睛表情变化（可选）
   → 串口日志显示 "Abort speaking"

4. 继续说："讲个笑话吧"
   → AI 开始讲笑话
```

---

## 📊 预期结果

### 成功标志

| 现象 | 说明 |
|-----|------|
| ✅ **AI 立即停止说话** | 最明显的标志 |
| ✅ **设备开始监听** | LED 变化或眼睛表情变化 |
| ✅ **串口日志** | 显示 "Wake word detected" 和 "Abort speaking" |

### 串口日志示例

```
I (12345) Application: Device state: Idle → Speaking
I (12346) Application: 开始播放 TTS 音频
...
I (23456) Application: Wake word detected: 你好小智
I (23457) Application: Abort speaking
I (23458) Application: Device state: Speaking → Listening
I (23459) Application: 进入监听模式
```

---

## 🎯 使用技巧

### 最佳使用方式

1. **等待 AI 开始说话后再打断**
   - 不要在唤醒词刚说完时立即打断
   - 等待 1-2 秒后打断效果更好

2. **清晰地说出唤醒词**
   - "你好小智"（完整说出）
   - 不要吞字或含糊不清

3. **保持适当距离**
   - 最佳距离：30-100 cm
   - 太远（>2 米）可能识别困难

4. **避开噪音源**
   - 避免在电视、音乐播放时使用
   - 安静环境下识别率最高

---

## ⚙️ 可选调整

### 如果经常误触发

**现象**: 没说唤醒词也打断

**解决**: 提高检测阈值

```bash
# 编辑 sdkconfig
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=25

# 或降低麦克风增益
# 编辑 main/audio/audio_codec.h
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 35.0
```

### 如果检测不到唤醒词

**现象**: 说了"你好小智"也不打断

**解决**: 降低检测阈值或提高增益

```bash
# 编辑 sdkconfig
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=15

# 或提高麦克风增益
# 编辑 main/audio/audio_codec.h
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 45.0
```

---

## 📋 测试清单

### 基础测试 ✅

- [ ] AI 说话时说"你好小智"
- [ ] AI 立即停止说话
- [ ] 切换到监听模式
- [ ] 串口日志显示 "Abort speaking"

### 进阶测试

- [ ] 连续打断 2-3 次
- [ ] 不同距离测试（30cm, 1米, 2米）
- [ ] 稍有噪音环境测试
- [ ] 快速说出唤醒词测试

---

## 🎉 开始使用

**无需任何配置，直接使用！**

```bash
idf.py build flash monitor
```

**功能已完全实现，享受实时打断体验吧！** 🚀

---

## 📞 需要帮助？

如果遇到问题，请查看详细文档：
- [wake_word_interruption_setup.md](wake_word_interruption_setup.md) - 完整配置指南
- [wake_word_interruption_analysis.md](wake_word_interruption_analysis.md) - 技术分析

---

**版本**: v0.3.0
**更新时间**: 2025-01-05
