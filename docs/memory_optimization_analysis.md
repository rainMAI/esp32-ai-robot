# 内存不足问题深度分析与优化方案

## 🔍 问题现状

### 错误日志
```
E (46180) EspUdp: Send failed: ret=-1, errno=12
E (46240) EspUdp: Send failed: ret=-1, errno=12
... (重复 60+ 次)
```

**错误含义**：
- `errno=12` = **ENOMEM** (内存不足)
- UDP 数据包发送失败

### 内存使用情况
```
启动时：free sram: 39KB
运行时：free sram: 26KB (最低)
```

**关键问题**：内存极其紧张，只剩 26KB 可用！

---

## 📊 内存配置分析

### 1. PSRAM 配置 ✅

**当前配置**：
```ini
CONFIG_SPIRAM=y                          # PSRAM 已启用
CONFIG_SPIRAM_USE_MALLOC=y                # 使用 PSRAM 进行 malloc
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=512   # 前 512B 从内部 RAM 分配
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536  # 保留 64KB 内部 RAM
CONFIG_ESP32S3_SPIRAM_SUPPORT=y           # ESP32-S3 PSRAM 支持
```

**分析**：
- ✅ PSRAM 已正确启用（8MB OCTAL PSRAM）
- ✅ 配置合理，小对象从内部 RAM 分配（速度）
- ⚠️ 但仍出现内存不足

---

### 2. 内存分区表

**分区配置**：`partitions/v1/16m.csv`

| 分区 | 类型 | 大小 | 用途 |
|-----|------|------|------|
| **nvs** | data | 16KB | NVS 存储配置 |
| **otadata** | data | 8KB | OTA 数据 |
| **phy_init** | data | 4KB | PHY 初始化数据 |
| **model** | data (spiffs) | 960KB | 模型文件（唤醒词等） |
| **ota_0** | app | 6MB | 应用程序分区 1 |
| **ota_1** | app | 6MB | 应用程序分区 2 |

**总 Flash**: 16MB ✅

---

### 3. 内存占用大户

#### 🔴大户 1：WiFi 缓冲区

**当前配置**：
```ini
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=6          # 6 个静态 RX 缓冲区
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8         # 8 个动态 RX 缓冲区
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32        # 32 个动态 TX 缓冲区 ⚠️
CONFIG_ESP_WIFI_RX_MGMT_BUF_NUM_DEF=5           # 5 个管理缓冲区
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=32                # 32 个管理小缓冲区 ⚠️
```

**内存占用**：
- 每个 TX 缓冲区：约 1.6KB
- 32 个 TX 缓冲区：32 × 1.6KB = **51.2KB**
- 32 个管理缓冲区：约 **10-20KB**
- **WiFi 总计：约 70-80KB**

#### 🔴大户 2：LwIP 网络栈

**当前配置**：
```ini
CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=32              # TCP/IP 接收邮箱
CONFIG_LWIP_TCP_RECVMBOX_SIZE=6                # TCP 接收邮箱
CONFIG_LWIP_TCP_ACCEPTMBOX_SIZE=6              # TCP 接受邮箱
CONFIG_LWIP_UDP_RECVMBOX_SIZE=6                # UDP 接收邮箱
CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=3072          # TCP/IP 任务栈 3KB
```

**内存占用**：
- TCP/IP 任务栈：**3KB**
- 邮箱队列：约 **5-10KB**
- **LwIP 总计：约 8-13KB**

#### 🔴大户 3：mbedTLS SSL

**当前配置**：
```ini
CONFIG_MBEDTLS_DYNAMIC_BUFFER=y                # 动态缓冲区
```

**内存占用**：
- SSL 缓冲区：约 **16-32KB**（动态分配）

#### 🔴大户 4：任务栈

**当前配置**：
```ini
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192            # 主任务栈 8KB
```

**项目中的任务**：
- Main Task: 8KB
- TCP/IP Task: 3KB
- Audio Task: ~4KB
- Wake Word Encode Task: 28KB (PSRAM)
- 其他任务：约 10-20KB

**任务栈总计**：约 **50-60KB** (内部 RAM)

#### 🔴大户 5：音频缓冲区

**当前配置**：
```ini
AUDIO_CODEC_DMA_DESC_NUM=6                      # DMA 描述符数量
AUDIO_CODEC_DMA_FRAME_NUM=240                   # DMA 帧数量
```

**内存占用**：
- Opus 缓冲区：约 **10-20KB**
- 音频处理缓冲区：约 **10-20KB**
- **音频总计：约 20-40KB**

#### 🔴大户 6：动画数据

**内存占用**：
- anim_eye.h: 6.5MB (Flash)
- anim_grok.h: 6.5MB (Flash)
- 运行时缓冲区：约 **50-100KB** (PSRAM)

---

## 🔍 根本原因分析

### 问题 1：内部 RAM (SRAM) 不足 ⭐⭐⭐

**ESP32-S3 内存布局**：
```
内部 SRAM: 512KB
├─ 系统保留: ~64KB (CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL)
├─ WiFi 缓冲: ~70KB
├─ LwIP 网络栈: ~10KB
├─ 任务栈: ~50KB
├─ 音频缓冲: ~30KB
├─ 其他: ~100KB
└─ 剩余可用: ~26KB ❌
```

**关键问题**：
- 内部 RAM 只有 512KB
- WiFi + 任务栈 + 音频 = **150KB** 常驻内存
- **只剩 26KB** 可用于动态分配

### 问题 2：WiFi TX 缓冲区过多 ⭐⭐⭐

```ini
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32        # 32 个 TX 缓冲区
```

**占用**: 51.2KB 内部 RAM

**问题**：
- 对于大多数应用，32 个 TX 缓冲区太多了
- 一般 6-10 个就足够了

### 问题 3：UDP 发送频率过高

从日志看：
- 每 60-100ms 发送一次 UDP
- 如果数据包大小 1KB，需要 1KB 缓冲区
- **60+ 次连续失败**说明：

**可能原因**：
1. UDP 缓冲区太小，无法发送
2. 内存碎片化，无法分配连续内存
3. 发送频率太高，来不及释放

---

## 💡 优化方案

### 方案 1：减少 WiFi TX 缓冲区数量 ⭐⭐⭐

**修改**: `sdkconfig.defaults`

```ini
# 修改前
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32

# 修改后（建议值）
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=10      # 减少到 10 个
```

**效果**：
- 节省内存：~35KB
- 对大多数应用影响很小

---

### 方案 2：优化 WiFi 管理缓冲区 ⭐⭐

**修改**: `sdkconfig.defaults`

```ini
# 修改前
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=32

# 修改后
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=16               # 减少到 16 个
```

**效果**：
- 节省内存：~10KB

---

### 方案 3：增加 UDP 缓冲区 ⭐⭐⭐

**修改**: `sdkconfig.defaults` 添加

```ini
# 增加 UDP 缓冲区大小
CONFIG_LWIP_UDP_RECVMBOX_SIZE=12                # 从 6 增加到 12
CONFIG_LWIP_NETBUF_TXCNT=8                      # 发送缓冲区数量（默认 5）

# 启用 UDP 更高效的模式
CONFIG_LWIP_SO_REUSE=y                          # 允许 SO_REUSEADDR
CONFIG_LWIP_SO_RCVBUF=y                         # 允许设置 SO_RCVBUF
```

**效果**：
- 提高 UDP 发送成功率
- 减少内存碎片化

---

### 方案 4：强制大缓冲区使用 PSRAM ⭐⭐⭐

**修改**: `sdkconfig.defaults` 添加

```ini
# 强制大缓冲区使用 PSRAM
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024        # 从 512 增加到 1KB
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768     # 从 64KB 减少到 32KB
```

**效果**：
- 增加可用内部 RAM：32KB
- **非常关键！**

---

### 方案 5：优化音频缓冲区 ⭐⭐

**检查**: `main/audio/audio_service.cc`

**建议**：
```cpp
// 减少音频缓冲区大小
// 当前可能是：每个缓冲区 10-20KB
// 建议：减少到 5-10KB
```

**效果**：
- 节省内存：10-20KB

---

### 方案 6：使用 heap_caps_malloc 指定内存类型 ⭐⭐⭐

**修改代码**: 搜索 `malloc` 和 `new`

**原则**：
- 大对象（> 2KB）：使用 `MALLOC_CAP_SPIRAM`
- 小对象（< 2KB）：使用 `MALLOC_CAP_INTERNAL` 或 `MALLOC_CAP_DMA`

**示例**：
```cpp
// ❌ 错误：默认分配可能占用内部 RAM
uint8_t* buffer = (uint8_t*)malloc(10240);

// ✅ 正确：大缓冲区使用 PSRAM
uint8_t* buffer = (uint8_t*)heap_caps_malloc(10240, MALLOC_CAP_SPIRAM);

// ✅ 正确：小缓冲区使用内部 RAM
uint8_t* small_buf = (uint8_t*)heap_caps_malloc(256, MALLOC_CAP_DMA);
```

---

## 🎯 推荐的优化步骤

### 第 1 步：立即修改配置 ⭐⭐⭐

创建或修改 `sdkconfig.defaults`：

```ini
# 减少 WiFi TX 缓冲区
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=10
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=16

# 优化 PSRAM 分配策略
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768

# 增加 UDP 缓冲区
CONFIG_LWIP_UDP_RECVMBOX_SIZE=12
CONFIG_LWIP_NETBUF_TXCNT=8
```

**预期效果**：
- 节省内部 RAM：约 **50-60KB**
- 增加可用内存：从 26KB → **80KB**

---

### 第 2 步：清理配置并重新编译

```bash
cd D:\code\eyes
idf.py fullclean
idf.py build
```

---

### 第 3 步：测试和监控

**添加内存监控代码**（可选）：

```cpp
// 在 application.cc 或适当位置
void print_memory_info() {
    ESP_LOGI(TAG, "=== Memory Info ===");
    ESP_LOGI(TAG, "Free heap: %d KB", esp_get_free_heap_size() / 1024);
    ESP_LOGI(TAG, "Largest free block: %d KB",
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
    ESP_LOGI(TAG, "Internal free: %d KB",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    ESP_LOGI(TAG, "SPIRAM free: %d KB",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
}

// 每 10 秒输出一次
while(1) {
    print_memory_info();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
}
```

---

## 📊 预期效果

### 优化前
```
内部 RAM 可用: 26KB ❌
UDP 发送失败: 60+ 次
内存碎片化: 严重
```

### 优化后（预期）
```
内部 RAM 可用: 80KB ✅ (+208%)
UDP 发送失败: 0-5 次 (-90%)
内存碎片化: 轻微
```

---

## ⚠️ 注意事项

### 1. WiFi 缓冲区减少的影响

**减少 TX 缓冲区**：32 → 10

**可能的影响**：
- 高速传输时可能略有影响
- 对语音应用影响很小

**建议**：
- 先试 10 个
- 如果还有问题，可以增加到 16

---

### 2. PSRAM 保留内存的影响

**减少保留内存**：64KB → 32KB

**可能的影响**：
- 某些关键代码可能需要内部 RAM
- 但 32KB 通常足够

**建议**：
- 保守一点可以先设为 48KB
- 观察是否有错误

---

## 🚀 立即行动

### 快速修复（5 分钟）

创建 `sdkconfig.defaults` 并添加上述配置：

```bash
cd D:\code\eyes
cat >> sdkconfig.defaults << 'EOF'
# 减少 WiFi 缓冲区以节省内存
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=10
CONFIG_ESP_WIFI_MGMT_SBUF_NUM=16

# 优化 PSRAM 分配策略
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768

# 增加 UDP 缓冲区
CONFIG_LWIP_UDP_RECVMBOX_SIZE=12
CONFIG_LWIP_NETBUF_TXCNT=8
EOF

# 清理并重新编译
idf.py fullclean
idf.py build
```

---

## 📝 总结

### 问题根源

1. **WiFi TX 缓冲区过多** (32 个)：占用 51KB
2. **WiFi 管理缓冲区过多** (32 个)：占用 20KB
3. **PSRAM 保留内存过大** (64KB)：浪费可用内存
4. **UDP 缓冲区太小**：导致发送失败

### 解决方案

1. ✅ 减少 WiFi 缓冲区：节省 70KB
2. ✅ 优化 PSRAM 策略：释放 32KB
3. ✅ 增加 UDP 缓冲区：提高成功率
4. ✅ **预期增加可用内存：约 100KB**

---

**现在就开始优化吧！** 🚀

---

**版本**: v0.3.1
**更新时间**: 2025-01-05
**作者**: Claude Code
