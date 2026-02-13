# OTA 功能禁用说明

## 修改内容

已禁用 OTA 自动更新功能，避免启动时尝试连接 `api.tenclass.net` 失败导致的错误。

## 修改文件

### 1. [main/application.cc](../main/application.cc)

#### 禁用 OTA 版本检查（第 388 行）

```cpp
// OTA 功能已禁用 - 使用单 app 分区，不支持 OTA 更新
// Check for new firmware version or get the MQTT broker address
Ota ota;
// CheckNewVersion(ota);  // 已禁用
```

#### 默认使用 WebSocket 协议（第 396-409 行）

```cpp
// OTA 检查已禁用，默认使用 WebSocket 协议
// 如果需要使用 MQTT，取消注释下面一行：
// protocol_ = std::make_unique<MqttProtocol>();
protocol_ = std::make_unique<WebsocketProtocol>();
```

## 影响范围

### ❌ 不再支持的功能

1. **固件自动更新** - 系统启动时不再检查新版本
2. **动态协议配置** - 无法通过 OTA 服务器获取协议配置（MQTT/WebSocket）
3. **设备激活** - 无法通过 OTA 服务器激活设备

### ✅ 正常工作的功能

1. **语音交互** - 唤醒词、语音识别、TTS 播放
2. **WebSocket/MQTT 通信** - 使用硬编码的协议类型
3. **设备控制** - MCP 协议、物联网控制
4. **本地功能** - 提醒、显示、音频等

## 原因说明

### 为什么禁用 OTA？

1. **分区表变更** - 从双 OTA 分区改为单 factory 分区
2. **节省空间** - 单 app 分区提供 15MB 空间（vs 双 OTA 每个 7MB）
3. **避免网络错误** - 不再依赖 `api.tenclass.net` 服务

### 相关修改

- [partitions/v1/16m.csv](../partitions/v1/16m.csv) - 单 app 分区表
- [sdkconfig.defaults](../sdkconfig.defaults) - 禁用 OTA 相关配置

## 如果需要恢复 OTA

如果你以后需要恢复 OTA 功能，需要：

1. **恢复分区表** - 使用双 OTA 分区布局
2. **取消注释** - 在 `main/application.cc` 中恢复以下代码：
   ```cpp
   // 恢复版本检查
   CheckNewVersion(ota);

   // 恢复动态协议选择
   if (ota.HasMqttConfig()) {
       protocol_ = std::make_unique<MqttProtocol>();
   } else if (ota.HasWebsocketConfig()) {
       protocol_ = std::make_unique<WebsocketProtocol>();
   } else {
       protocol_ = std::make_unique<MqttProtocol>();
   }
   ```

3. **重新配置** - 设置 OTA URL 和服务器地址

## 协议配置

### 当前使用：WebSocket

连接到默认的 WebSocket 服务器进行通信。

### 如果需要使用 MQTT

在 [main/application.cc](../main/application.cc#L398) 中取消注释：
```cpp
protocol_ = std::make_unique<MqttProtocol>();
```
并注释掉：
```cpp
// protocol_ = std::make_unique<WebsocketProtocol>();
```

## 错误日志

### 禁用前（错误示例）

```
E (47740) esp-tls: [sock=54] select() timeout
E (47740) esp-tls: Failed to open new connection
E (47740) EspSsl: Failed to connect to api.tenclass.net:443
E (47740) HttpClient: TCP connection failed
E (47740) Ota: Failed to open HTTP connection
W (47750) Application: Alert 错误: 检查新版本失败，将在 20 秒后重试
W (47750) Application: Check new version failed, retry in 20 seconds (2/10)
```

### 禁用后

不再有上述错误日志，系统直接启动并连接到 WebSocket 服务器。

## 验证

启动后应该看到：

```
I (xxxx) Application: Network connected
I (xxxx) Application: Loading protocol
I (xxxx) WebsocketProtocol: Connecting to ws://...
```

而**不应该**看到：
```
E (xxxx) Ota: Failed to open HTTP connection
W (xxxx) Application: Check new version failed
```

## 固件更新方式

由于禁用了 OTA，固件更新需要：

1. **编译新固件**：`idf.py build`
2. **串口烧录**：`idf.py flash`
3. **或者使用 USB 烧录工具**

## 总结

✅ **已禁用**：OTA 自动更新检查
✅ **已解决**：`api.tenclass.net` 连接失败错误
✅ **默认协议**：WebSocket（可改为 MQTT）
✅ **正常工作**：所有本地功能和通信功能
