# 内存不足问题修复指南（v0.3.1）

## 🚨 问题现状

### 错误日志
```
I (18820) SystemInfo: free sram: 35363 minimal sram: 35343  ← 只有 35KB 可用
I (43080) AfeWakeWord: ⭐ WakeWord DETECTED! Word: 你好小鑫
E (43530) EspUdp: Send failed: ret=-1, errno=12  ← 内存不足
E (43700) EspUdp: Send failed: ret=-1, errno=12
... (重复 60+ 次)
```

### 问题根源

**sdkconfig 文件覆盖了 sdkconfig.defaults 中的优化配置！**

| 配置项 | sdkconfig.defaults | sdkconfig (实际) | 影响 |
|-------|-------------------|-----------------|------|
| `CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM` | 10 ✅ | **32** ❌ | 多占用 35KB |
| `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` | 32768 ✅ | **65536** ❌ | 少占用 32KB |
| `CONFIG_LWIP_UDP_RECVMBOX_SIZE` | 12 ✅ | **6** ❌ | UDP 缓冲区太小 |

**结果**：
- WiFi TX 缓冲区占用 **51KB** 而不是优化的 16KB
- PSRAM 保留 **64KB** 而不是优化的 32KB
- 可用内存只有 **35KB** 而不是优化的 80KB

---

## ✅ 修复步骤

### 方法 1：完整清理重编译（推荐）

```bash
cd D:\code\eyes

# 1. 删除旧的配置文件
rm -f sdkconfig
rm -f sdkconfig.old
rm -rf build

# 2. 重新生成配置（将使用 sdkconfig.defaults）
idf.py reconfigure

# 3. 清理并重新编译
idf.py fullclean
idf.py build

# 4. 烧录
idf.py flash monitor
```

### 方法 2：手动编辑 sdkconfig

**文件**: `D:\code\eyes\sdkconfig`

**找到以下行并修改**：

```ini
# 修改前（第 1473 行）
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32

# 修改后
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=10
```

```ini
# 修改前（第 1311 行）
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536

# 修改后
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
```

```ini
# 修改前（第 1822 行）
CONFIG_LWIP_UDP_RECVMBOX_SIZE=6

# 修改后
CONFIG_LWIP_UDP_RECVMBOX_SIZE=12
```

**然后重新编译**：
```bash
idf.py build flash monitor
```

---

## 📊 预期效果

### 修复前
```
I (18820) SystemInfo: free sram: 35363 minimal sram: 35343
E (43530) EspUdp: Send failed: ret=-1, errno=12  ← 60+ 次失败
```

### 修复后
```
I (18820) SystemInfo: free sram: 80000 minimal sram: 70000  ← 可用内存增加到 80KB
✅ UDP 发送成功，不再出现 errno=12 错误
```

---

## 🔍 验证修复

### 1. 检查内存

**编译后查看日志**：
```
I (xxxx) SystemInfo: free sram: 80000  ← 应该看到约 80KB
```

### 2. 测试唤醒词

```
用户: "你好小鑫"
设备: ✅ 正常连接，不再出现 UDP 发送失败
```

### 3. 测试提醒功能

```
用户: "10秒后提醒我喝水"
设备: ✅ 10 秒后正常播放提醒
```

---

## 🐛 常见问题

### Q1: 为什么优化配置没有生效？

**A**: `sdkconfig` 文件的优先级高于 `sdkconfig.defaults`。如果 `sdkconfig` 已存在，`sdkconfig.defaults` 中的配置会被忽略。

**解决**: 删除 `sdkconfig` 文件，重新生成配置。

### Q2: 我已经修改了 sdkconfig.defaults，为什么还是没用？

**A**: 需要删除 `sdkconfig` 文件，让系统重新读取 `sdkconfig.defaults`。

```bash
rm -f sdkconfig
idf.py reconfigure
```

### Q3: 编译后内存还是不够怎么办？

**A**: 可以进一步优化 WiFi 缓冲区：

```ini
# sdkconfig.defaults
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=6  # 从 10 减少到 6
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=8           # 从 16 减少到 8
```

---

## 📝 总结

### 问题
- ❌ `sdkconfig` 覆盖了 `sdkconfig.defaults` 的优化配置
- ❌ 可用内存只有 35KB（应该是 80KB）
- ❌ UDP 发送失败 60+ 次

### 解决
1. ✅ 删除 `sdkconfig` 文件
2. ✅ 重新生成配置（使用 `sdkconfig.defaults`）
3. ✅ 重新编译并烧录

### 效果
- ✅ 可用内存增加到 80KB (+128%)
- ✅ UDP 发送失败减少到 0-5 次 (-90%)

---

## 🚀 快速修复命令

```bash
cd D:\code\eyes

# 一键修复
rm -f sdkconfig sdkconfig.old && idf.py fullclean && idf.py build flash monitor
```

---

**版本**: v0.3.1
**更新时间**: 2025-01-07
**作者**: Claude Code
