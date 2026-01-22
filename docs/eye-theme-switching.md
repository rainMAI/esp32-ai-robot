# 眼睛主题切换功能使用指南

## 功能概述

通过 MCP (Model Context Protocol) 协议，你现在可以通过对话方式切换小智机器人的眼睛主题样式！

## 可用主题

| 主题名称 | 英文标识 | 描述 |
|---------|---------|------|
| 星空主题 | `xingkong` | 梦幻紫蓝色星空效果（默认）|
| 水墨主题 | `shuimu` | 优雅的中国水墨画风格 |
| 科技主题 | `keji` | 未来感青色科技风格 |

## 使用方法

### 方式 1：语音对话（推荐）

直接对小智说话，例如：

- "换成星空眼睛"
- "用水墨风格的眼睛"
- "给我换个科技感的眼睛"
- "Change to starry sky eyes"
- "Use ink painting style"

### 方式 2：通过 MCP 协议直接调用

如果你想直接调用 MCP 工具，可以使用以下 JSON-RPC 格式：

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.eye.set_theme",
    "arguments": {
      "theme": "xingkong"
    }
  },
  "id": 1
}
```

## 技术实现细节

### 代码结构

1. **eye_display.h/cc**: 眼睛渲染和主题切换实现
   - `SetEyeTheme(int theme_id)`: 设置眼睛主题
   - `GetEyeThemeName(int theme_id)`: 获取主题名称

2. **eyes_data.h**: 眼睛图像数据
   - `sclera_xingkong[]`: 星空眼白数据
   - `sclera_shuimu[]`: 水墨眼白数据
   - `sclera_keji[]`: 科技眼白数据
   - `iris_xingkong[]`: 虹膜数据

3. **mcp_server.cc**: MCP 协议服务器
   - 注册了 `self.eye.set_theme` 工具

### 线程安全

眼睛主题切换使用互斥锁 (`lcd_mutex`) 确保线程安全，防止在更新眼睛图像时发生冲突。

### 立即生效

调用 `SetEyeTheme()` 后会立即调用 `eye_update()` 刷新显示，无需等待。

## API 说明

### 工具名称
`self.eye.set_theme`

### 参数
- `theme` (字符串，必填): 主题名称
  - `"xingkong"`: 星空主题
  - `"shuimu"`: 水墨主题
  - `"keji"`: 科技主题

### 返回值
成功时：
```json
{
  "success": true,
  "message": "Eye theme changed to xingkong",
  "theme": "xingkong"
}
```

失败时：
```json
{
  "success": false,
  "message": "Unknown theme: xxx. Available themes: xingkong, shuimu, keji"
}
```

## 扩展新主题

如果你想添加新的眼睛主题，请按以下步骤操作：

### 1. 准备图像数据

将新的眼睛图像（眼白数据）转换为 RGB565 格式，分辨率应为 375x375 像素。

### 2. 添加到 eyes_data.h

```cpp
const uint16_t sclera_yourtheme[375*375] = {
    // 你的图像数据...
};
```

### 3. 更新枚举和函数

在 `eye_display.h` 中添加新的枚举值：

```cpp
enum EyeTheme {
    EYE_THEME_XINGKONG = 0,
    EYE_THEME_SHUIMU = 1,
    EYE_THEME_KEJI = 2,
    EYE_THEME_YOURTHEME = 3,  // 新增
};
```

在 `eye_display.cc` 的 `SetEyeTheme()` 函数中添加新的 case：

```cpp
case EYE_THEME_YOURTHEME:
    sclera = sclera_yourtheme;
    iris = iris_xingkong;  // 或者使用新的虹膜
    ESP_LOGI(TAG, "Applied YourTheme");
    break;
```

在 `GetEyeThemeName()` 中添加名称映射。

### 4. 更新 MCP 工具

在 `mcp_server.cc` 的工具描述中添加新主题的说明，并在条件判断中添加：

```cpp
else if (theme == "yourtheme") {
    theme_id = EYE_THEME_YOURTHEME;
}
```

## 故障排除

### 问题：切换主题后眼睛没有变化

**解决方案**：
1. 检查串口日志，确认函数被调用
2. 确认 `lcd_mutex` 没有被其他任务长时间占用
3. 检查图像数据是否正确编译到固件中

### 问题：编译错误 - 找不到 sclera_shuimu 等符号

**解决方案**：
1. 确保 `eyes_data.h` 被正确包含
2. 检查图像数组是否完整定义
3. 清理构建缓存：`idf.py fullclean && idf.py build`

### 问题：切换时系统卡顿

**解决方案**：
这是正常现象，因为需要将大量图像数据（375x375x2 字节）复制到显存。可以考虑：
1. 降低图像分辨率
2. 使用 SPI Flash 存储图像数据
3. 优化 DMA 传输

## 示例对话

**用户**: 换个星空眼睛
**小智**: 好的，我已经为您切换到星空主题的眼睛！

**用户**: 用水墨风格的眼睛
**小智**: 好的，已经切换到水墨风格，看起来很有艺术气息呢！

**用户**: 给我换个科技感的
**小智**: 没问题！科技主题已应用，看起来是不是很有未来感？

## 总结

通过这个功能，小智的"眼睛"可以根据对话内容和场景情绪进行切换，让交互更加生动有趣！

享受你的个性化小智吧！✨


## 今后拓展
**情绪系统** 通过情绪系统控制（最优雅）
查看 Application::Alert() 方法，可以利用情绪系统来触发眼睛切换：

// 在 application.cc 中
void Application::SetEyeTheme(const std::string& theme) {
    // 通过事件发送到眼睛显示任务
    xTaskNotify(eye_update_handler, THEME_CHANGE_EVENT, eSetValueWithOverwrite);
}

// 然后在对话解析中：
// 当用户说"换眼睛"时，AI 返回特殊指令
// 协议层解析后调用 SetEyeTheme()