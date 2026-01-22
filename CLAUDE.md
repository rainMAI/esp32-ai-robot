# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此代码库中工作时提供指导。

## 项目概述

这是一个基于 ESP32-S3 的双目 AI 机器人项目（版本 1.8.9），从"虾哥"的 xiaozhi-esp32 项目分支而来。主要特性包括：
- **双目机器人显示系统**，使用两块 GC9A01 LCD 面板（每块 240x240）
- **语音交互**，支持唤醒词检测和大模型集成（Qwen、DeepSeek）
- **MCP（模型上下文协议）**用于物联网设备控制和 AI 工具集成
- **多协议通信**：MQTT、WebSocket 和自定义二进制协议

## 构建和开发命令

### 构建项目
```bash
# 构建固件
idf.py build

# 清理构建（在依赖更新后很有用）
idf.py fullclean
idf.py build

# 烧录到设备
idf.py flash

# 监控串口输出
idf.py monitor

# 烧录并监控
idf.py flash monitor
```

### 配置
```bash
# 打开配置菜单
idf.py menuconfig

# 主要配置选项：
# - 开发板选择（支持 40+ 种开发板）
# - 语言选择（支持 15+ 种语言）
# - 音频编解码器选择
# - 显示类型配置
```

## 架构概览

### 入口点和初始化

**main.cc**：应用程序入口点
- 创建默认事件循环
- 初始化 NVS Flash 用于 WiFi 配置
- 实例化并启动 `Application` 单例
- 进入主事件循环

**Application** (main/application.h)：中央协调器
- 单例模式，管理整个应用程序状态
- 设备状态机（启动中、WiFi 配置中、空闲、连接中、监听中、说话中等）
- 管理 AudioService、Protocol 通信和 OTA 更新
- 处理唤醒词检测和语音交互流程
- 基于 FreeRTOS 事件组的事件驱动架构

### 显示系统架构

显示系统采用**双层架构**：

#### 第 1 层：高级显示抽象
- **Display** (main/display/display.h)：所有显示类型的抽象基类
  - 基于 LVGL 的 UI，包含状态、通知、聊天消息
  - 主题支持和情绪显示
  - 省电模式
  - 由以下类实现：EyeDisplay、LcdDisplay、OledDisplay 和特定开发板变体

#### 第 2 层：眼睛显示系统（魔眼）

**eye_display.cc/h** 实现双目机器人面部：

**硬件配置：**
- 两块 GC9A01 SPI LCD 面板（每块 240x240 像素）
- SPI3_HOST 用于眼睛 2，SPI2_HOST 用于眼睛 1
- 20MHz SPI 时钟
- 双缓冲，每次渲染 10 行（LINES_PER_BATCH=10）

**核心组件：**

1. **眼睛素材** (eyes_data.h)：
   - `sclera`：眼白纹理（375x375 RGB565）
   - `iris`：虹膜/瞳孔纹理（256x64 极坐标映射）
   - `upper`/`lower`：眼睑纹理
   - 多种主题变体（默认、xingkong/星空）

2. **眼睛渲染管线**：
   ```
   drawEye(e, iScale, scleraX, scleraY, uT, lT)
   └──> 渲染巩膜（眼白）
   └──> 渲染虹膜（缩放和定位）
   └──> 渲染眼睑（上下眼睑带透明度）
   ```

3. **动画系统**：
   - **缓动**：预计算的缓入/缓出曲线（ease[] 数组，256 个条目）
   - **虹膜缩放**：IRIS_MIN (180) 到 IRIS_MAX (280)
   - **眼睛跟踪**：根据 `eyeNewX`、`eyeNewY` 更新虹膜位置
   - **眨眼**：状态机，包含 NOBLINK/ENBLINK/DEBLINK 状态
   - **分割动画**：在一段时间内插值虹膜缩放值

4. **线程安全**：
   - `lcd_mutex`：所有 LCD 操作的全局互斥锁
   - `task_update_eye_handler`：用于持续眼睛更新的 FreeRTOS 任务
   - 安全位图绘制：`esp_lcd_safe_draw_bitmap()`

5. **控制标志**：
   - `is_blink`：启用/禁用自动眨眼
   - `is_track`：启用眼睛跟踪（设置为 true 时进入"半眼"模式）
   - `eyeNewX`、`eyeNewY`：目标眼睛位置（0-1023 范围）

### 其他主要组件

**音频系统** (main/audio/)：
- AudioService：中央音频管理
- AudioCodecs：ES8311、ES8374、ES8388 等
- WakeWordDetection：AFE、ESP 和自定义引擎
- AudioProcessors：AEC（声学回声消除）支持

**协议层** (main/protocols/)：
- Protocol：抽象通信接口
- MqttProtocol、WebSocketProtocol：实时双向通信
- BinaryProtocolV2/V3：高效数据传输

**物联网集成** (main/iot/)：
- ThingSystem：抽象物联网设备管理
- MCPServer：模型上下文协议服务器，用于 AI 工具集成

**开发板支持** (main/boards/)：
- 40+ 开发板配置，包含特定硬件实现
- 每个开发板定义：显示类型、音频编解码器、GPIO 引脚映射

## 代码风格

本项目使用 **Google C++ 代码风格**。贡献代码时：
- 遵循 Google C++ 风格指南约定
- 使用适当的命名空间和类命名
- 包含适当的错误处理（使用 ESP_ERROR_CHECK）
- 尽可能使用 RAII 进行资源管理

## 常见模式

### 添加新开发板
1. 在 `main/boards/` 中创建开发板配置
2. 定义显示类型（LCD/OLED/眼睛）
3. 指定音频编解码器和处理器
4. 添加 GPIO 引脚映射
5. 在 `main/CMakeLists.txt` 中更新构建配置

### 修改眼睛行为
eye_display.cc 中的关键变量：
- **虹膜大小**：修改 `IRIS_MIN`/`IRIS_MAX` 宏
- **眨眼时机**：在眨眼状态机中调整 `timeToNextBlink`
- **眼睛跟踪**：更新 `eyeNewX`、`eyeNewY` 并设置 `is_track = true`
- **主题**：在 `sclera_default`、`sclera_xingkong`、`iris_default`、`iris_xingkong` 之间切换

### 线程安全的显示操作
在 LCD 操作前始终获取 `lcd_mutex`：
```cpp
if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
    // 执行 LCD 操作
    esp_lcd_safe_draw_bitmap(...);
    xSemaphoreGive(lcd_mutex);
}
```

## 已知问题和解决方案

### 组件管理器依赖

当 `idf_component.yml` 依赖更新时，组件管理器可能重新下载组件，可能撤销本地修复。常见问题：

1. **esp_emote_gfx 中的格式化字符串错误**：
   - 受影响文件：`gfx_render.c`、`gfx_refr.c`、`gfx_label.c`
   - 修复方法：添加 `#include <inttypes.h>` 并使用 PRI* 宏（PRIu32、PRId32）
   - 位置：`managed_components/espressif2022__esp_emote_gfx/src/core/`

2. **缺少 string.h**：
   - 文件：`gfx_render.c`
   - 修复方法：添加 `#include <string.h>` 以使用 memset

如果依赖更新后编译失败：
```bash
idf.py fullclean
idf.py build
```

## 文档参考

- [自定义开发板指南](main/boards/README.md) - 自定义开发板开发指南
- [MCP 协议物联网控制用法说明](docs/mcp-usage.md) - MCP 协议用于物联网控制
- [MCP 协议交互流程](docs/mcp-protocol.md) - MCP 实现细节
- [一份详细的 WebSocket 通信协议文档](docs/websocket.md) - WebSocket 协议
- [MQTT + UDP 混合通信协议文档](docs/mqtt-udp.md) - MQTT+UDP 混合协议

## 外部参考

- 原始项目：https://github.com/78/xiaozhi-esp32
- 官方服务器：https://xiaozhi.me
