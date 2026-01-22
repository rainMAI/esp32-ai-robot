# 眼睛主题切换完整指南

## 🎨 可用主题总览

### 原始大尺寸主题 (375x375)
| 主题名 | 英文标识 | 描述 | Iris来源 |
|--------|----------|------|----------|
| 星空主题 | `xingkong` | 梦幻紫蓝色星空效果(默认) | iris_xingkong |
| 水墨主题 | `shuimu` | 优雅的中国水墨画风格 | iris_xingkong |
| 科技主题 | `keji` | 未来感青色科技风格 | iris_xingkong |
| 龙眼主题 | `dragon` | 神秘的龙眼风格 | DragonTheme::dragon_iris |

### Graphics转换主题 (375x375)
| 主题名 | 英文标识 | 原始尺寸 | 描述 | Iris来源 |
|--------|----------|----------|------|----------|
| 猫眼 | `cat` | 180x180 | 可爱的猫咪眼睛 | iris_xingkong |
| 默认眼 | `default` | 200x200 | 经典默认风格 | DefaultTheme::default_iris |
| 鹿眼 | `doe` | 180x180 | 温柔的鹿眼风格 | DoeTheme::doe_iris |
| 山羊眼 | `goat` | 128x128 | 横向瞳孔山羊眼 | GoatTheme::goat_iris |
| Nauga眼 | `nauga` | 180x180 | Nauga风格眼睛 | iris_xingkong |
| 蝾螈眼 | `newt` | 200x200 | 两栖动物风格 | NewtTheme::newt_iris |
| 无眼白 | `noSclera` | 160x160 | 独特的无眼白风格 | NoscleraTheme::noSclera_iris |
| 猫头鹰眼 | `owl` | 180x180 | 智慧的猫头鹰眼 | iris_xingkong |
| 终结者眼 | `terminator` | 200x200 | 机器人终结者风格 | TerminatorTheme::terminator_iris |

## 📋 使用方法

### 方式1: 语音对话 (推荐)

直接对小智说话,例如:

**原始主题:**
- "换成星空眼睛"
- "用水墨风格的眼睛"
- "给我换个科技感的眼睛"
- "切换到龙眼"

**Graphics主题:**
- "换成猫眼"
- "用鹿眼风格"
- "给我换个山羊眼睛"
- "切换到终结者眼"
- "换成猫头鹰的眼睛"

**英文示例:**
- "Change to cat eyes"
- "Use terminator style"
- "Switch to dragon eyes"

### 方式2: 通过 MCP 协议直接调用

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.eye.set_theme",
    "arguments": {
      "theme": "cat"
    }
  },
  "id": 1
}
```

## 🔧 技术实现

### 代码结构

1. **eye_themes.h/cc**: 主题配置管理
   - 定义了13个主题的枚举值 (0-12)
   - 包含所有主题的配置数据

2. **主题数据文件**:
   - `eyes_data.h`: 原始大尺寸主题数据
   - `dragon_theme_data.h/cc`: 龙眼独立数据
   - `cat_theme_data.h/cc`: 猫眼独立数据
   - `default_theme_data.h/cc`: 默认眼独立数据
   - `doe_theme_data.h/cc`: 鹿眼独立数据
   - `goat_theme_data.h/cc`: 山羊眼独立数据
   - `nauga_theme_data.h/cc`: Nauga眼独立数据
   - `newt_theme_data.h/cc`: 蝾螈眼独立数据
   - `noSclera_theme_data.h/cc`: 无眼白数据
   - `owl_theme_data.h/cc`: 猫头鹰眼数据
   - `terminator_theme_data.h/cc`: 终结者眼数据

3. **mcp_server.cc**: MCP协议服务器
   - 注册了 `self.eye.set_theme` 工具
   - 支持13个主题的切换

### 特性

- ✅ **统一尺寸**: 所有主题统一为375x375
- ✅ **命名空间隔离**: 每个主题使用独立命名空间,避免符号冲突
- ✅ **线程安全**: 使用互斥锁确保切换安全
- ✅ **立即生效**: 切换后立即刷新显示
- ✅ **Iris多样性**: 6个主题使用自己独特的iris,其他使用星空虹膜

## 📊 主题枚举值

```cpp
typedef enum {
    EYE_THEME_XINGKONG = 0,   // 星空主题
    EYE_THEME_SHUIMU = 1,     // 水墨主题
    EYE_THEME_KEJI = 2,       // 科技主题
    EYE_THEME_DRAGON = 3,     // 龙眼主题
    EYE_THEME_CAT = 4,        // 猫眼主题
    EYE_THEME_DEFAULT = 5,    // 默认主题
    EYE_THEME_DOE = 6,        // 鹿眼主题
    EYE_THEME_GOAT = 7,       // 山羊眼主题
    EYE_THEME_NAUGA = 8,      // Nauga眼主题
    EYE_THEME_NEWT = 9,       // 蝾螈眼主题
    EYE_THEME_NOSCLERA = 10,  // 无眼白主题
    EYE_THEME_OWL = 11,       // 猫头鹰眼主题
    EYE_THEME_TERMINATOR = 12 // 终结者眼主题
} EyeTheme;
```

## 💾 存储需求

所有13个主题数据已集成到固件中:
- **固件大小**: 约8MB
- **分区要求**: 需要使用32MB分区表 (partitions/v1/32m.csv)
- **OTA分区**: 12MB (足够容纳固件)

## 🎯 API 返回值

成功时:
```json
{
  "success": true,
  "message": "Eye theme changed to cat",
  "theme": "cat"
}
```

失败时:
```json
{
  "success": false,
  "message": "Unknown theme: xxx. Available themes: xingkong, shuimu, keji, dragon, cat, default, doe, goat, nauga, newt, noSclera, owl, terminator"
}
```

## 🚀 编译和部署

### 清理并重新编译

```bash
idf.py fullclean
idf.py build
idf.py flash
```

### 确保使用正确的分区表

检查 `sdkconfig` 中的配置:
```
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v1/32m.csv"
```

## 📝 主题设计思路

### Iris数据策略

1. **使用自己Iris的主题** (6个):
   - `default`, `doe`, `goat`, `newt`, `noSclera`, `terminator`
   - 这些主题的原始文件包含完整的iris数据
   - 已转换为统一的256x64尺寸

2. **使用星空虹膜的主题** (7个):
   - `xingkong`, `shuimu`, `keji`, `dragon`, `cat`, `nauga`, `owl`
   - 原始文件iris数据为1x1占位或不存在
   - 统一使用梦幻的星空虹膜

### 尺寸统一策略

- **Sclera**: 全部统一为375x375
- **Iris**: 全部统一为256x64
- 使用最近邻插值算法进行缩放

## 🎨 主题切换示例对话

**用户**: 换成猫眼睛
**小智**: 好的,我已经为您切换到猫眼主题!

**用户**: 用龙眼风格
**小智**: 好的,龙之眼已激活!神秘而威严~

**用户**: 给我换个终结者风格的眼睛
**小智**: 没问题!终结者主题已应用,看起来是不是很酷?

**用户**: 换成山羊眼睛
**小智**: 好的,山羊眼已切换,横向瞳孔很独特呢!

## 🔮 总结

现在小智拥有**13种不同的眼睛主题**,包括:
- 4个原始大尺寸主题 (星空、水墨、科技、龙眼)
- 9个Graphics转换主题 (猫、默认、鹿、山羊、Nauga、蝾螈、无眼白、猫头鹰、终结者)

所有主题都已:
✅ 统一为375x375尺寸
✅ 6个使用自己独特的iris数据
✅ 使用命名空间避免冲突
✅ 完全集成到MCP协议
✅ 支持语音和API两种切换方式

享受你的个性化小智吧! ✨
