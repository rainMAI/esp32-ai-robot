# 服务器部署脚本使用说明

## 📋 概述

`deploy_to_120.sh` 是自动化部署脚本，用于将人脸分析和提醒服务部署到新服务器。

## 🎯 部署内容

### 核心文件
- `web_server.py` - Web 服务主程序
- `reminder_tts_routes.py` - TTS 路由
- `requirements.txt` - Python 依赖

### 功能模块
1. **vision/** - 人脸检测模块
   - detector.py
   - llm_client.py
   - report_generator.py

2. **routes/** - API 路由模块
   - auth_routes.py - 认证
   - chat_routes.py - 聊天
   - chat_manager_routes.py - 聊天管理
   - device_routes.py - 设备管理 ⭐
   - eye_display_routes.py - 眼睛显示
   - reminder_routes.py - 提醒 CRUD ⭐
   - report_routes.py - 报告
   - sync_routes.py - 同步 API ⭐
   - user_device_routes.py - 用户设备

3. **database/** - 数据库模块
   - connection.py - 数据库连接
   - init_db.py - 初始化脚本
   - schema.sql - 数据库结构

4. **services/** - 服务模块
   - auth_service.py - 认证服务
   - chat_service.py - 聊天服务
   - report_service.py - 报告服务

5. **utils/** - 工具模块
   - 辅助函数和工具类

6. **web-reminder/** - Web 管理界面 ⭐
   - 优先使用 `static/web-reminder/` (已构建版本)
   - 备选：`web-reminder/dist/` (构建输出)
   - 备选：源码 (需要 npm 构建)

7. **static/** - 静态文件 ⭐
   - auth.html - 认证页面 (登录/注册)
   - auth_check.js - 认证检查脚本
   - portal.html - 管理门户页面
   - chat-manager.html - AI对话管理 & 每日总结
   - 通用路由：`/static/<filename>` 可访问所有静态文件

8. **web_history/** - 历史记录
   - photos/ - 照片
   - reports/ - 报告
   - metadata/ - 元数据
   - index.json - 索引

## 🚀 使用方法

### 1. 本地准备

确保在 Git Bash 或支持 bash 的终端中执行：

```bash
cd D:/project/toys
chmod +x deploy_to_120.sh
./deploy_to_120.sh
```

### 2. 修改目标服务器（可选）

如果要部署到不同的服务器，编辑脚本第 5 行和 102 行：

```bash
# 将 120.25.213.109 改为你的服务器 IP
sed -i 's/120.25.213.109/YOUR_SERVER_IP/g' deploy_to_120.sh
```

### 3. 自动执行

脚本会自动完成：

1. ✅ 创建完整部署包
2. ✅ 上传到服务器
3. ✅ 备份现有安装（保留数据库）
4. ✅ 部署新文件
5. ✅ 安装 Python 依赖
6. ✅ 初始化数据库
7. ✅ 配置 systemd 服务
8. ✅ 启动服务
9. ✅ 测试验证（4 项测试）

## 📦 部署后的目录结构

```
/opt/face_analysis_system/
├── web_server.py              # 主程序
├── reminder_tts_routes.py     # TTS 路由
├── requirements.txt           # 依赖
├── vision/                    # 人脸检测
│   ├── detector.py
│   ├── llm_client.py
│   └── report_generator.py
├── routes/                    # API 路由
│   ├── auth_routes.py
│   ├── chat_routes.py
│   ├── chat_manager_routes.py
│   ├── device_routes.py       # 设备注册 ⭐
│   ├── eye_display_routes.py
│   ├── reminder_routes.py     # 提醒 CRUD ⭐
│   ├── report_routes.py
│   ├── sync_routes.py         # 同步 API ⭐
│   └── user_device_routes.py
├── database/                  # 数据库
│   ├── connection.py
│   ├── init_db.py
│   └── schema.sql
├── services/                  # 服务模块
│   ├── auth_service.py
│   ├── chat_service.py
│   └── report_service.py
├── utils/                     # 工具模块
├── web-reminder/             # Web 界面 ⭐
├── static/                   # 静态文件 ⭐
│   ├── auth.html             # 认证页面
│   ├── auth_check.js         # 认证检查脚本
│   ├── portal.html           # 管理门户
│   └── chat-manager.html     # AI对话管理
├── web_history/              # 历史记录
│   ├── photos/
│   ├── reports/
│   ├── metadata/
│   └── index.json
└── face_analysis.db          # 数据库文件（运行时生成）
```

## ✅ 测试验证项

部署完成后，脚本会自动测试：

1. **健康检查** - `/health` 端点
2. **设备注册** - `/api/devices/register`
3. **同步 API** - `/api/sync/pull`
4. **TTS 服务** - `/api/text_to_pcm`
5. **认证页面** - `/static/auth.html` ⭐

## 🔧 常用命令

### 查看服务状态
```bash
ssh root@120.25.213.109 "systemctl status face-analysis-web"
```

### 查看实时日志
```bash
ssh root@120.25.213.109 "journalctl -u face-analysis-web -f"
```

### 重启服务
```bash
ssh root@120.25.213.109 "systemctl restart face-analysis-web"
```

### 停止服务
```bash
ssh root@120.25.213.109 "systemctl stop face-analysis-web"
```

### 查看最近错误
```bash
ssh root@120.25.213.109 "journalctl -u face-analysis-web -p err -n 50"
```

## 📊 服务端点

### 基础端点
- `GET /` - 主页
- `GET /health` - 健康检查
- `GET /web-reminder` - Web 管理界面

### 静态文件
- `GET /static/auth.html` - 认证页面（登录/注册）
- `GET /static/auth_check.js` - 认证检查脚本
- `GET /static/portal.html` - 管理门户页面
- `GET /static/chat-manager.html` - AI对话管理 & 每日总结
- `GET /static/<filename>` - 通用静态文件访问

### 设备管理
- `POST /api/devices/register` - 设备注册
- `GET /api/devices` - 获取设备列表
- `POST /api/devices/manual` - 手动添加设备

### 提醒管理
- `POST /api/reminders` - 创建提醒
- `GET /api/reminders` - 获取提醒列表
- `GET /api/reminders/<id>` - 获取单个提醒
- `PUT /api/reminders/<id>` - 更新提醒
- `DELETE /api/reminders/<id>` - 删除提醒

### 同步 API
- `GET /api/sync/pull` - 拉取提醒和设置
- `POST /api/sync/push` - 推送本地提醒

### TTS 服务
- `POST /api/text_to_pcm` - 文本转语音

## 🛡️ 数据保留策略

脚本会自动保留：
- ✅ `face_analysis.db` 数据库文件
- ✅ `web_history/` 目录及内容

每次部署前会自动备份到 `/backup/` 目录。

## 🔄 重新部署

如果要重新部署（保留数据）：

```bash
cd D:/project/toys
./deploy_to_120.sh
```

脚本会自动：
1. 备份现有安装
2. 保留数据库文件
3. 部署新文件
4. 恢复数据库文件

## 🐛 故障排除

### 问题 1: SSH 连接失败
```bash
# 清除旧的 SSH 密钥
ssh-keygen -R 120.25.213.109
```

### 问题 2: Python 版本不兼容
脚本会自动检测 Python 版本（3.8 或 3.12），并使用合适的版本。

### 问题 3: 数据库初始化失败
```bash
ssh root@120.25.213.109
cd /opt/face_analysis_system/database
python3 init_db.py
```

### 问题 4: 服务启动失败
```bash
# 查看详细日志
ssh root@120.25.213.109 "journalctl -u face-analysis-web -n 100 --no-pager"

# 手动启动测试
ssh root@120.25.213.109
cd /opt/face_analysis_system
python3 web_server.py
```

### 问题 5: web-reminder 设备列表显示 Failed to fetch

**原因**: 设备管理 API 缺少认证和 owner_id 设置

**解决方案**:
1. 确保登录获取 token
2. 添加设备时会自动关联到当前用户
3. 验证 API 配置是否使用动态服务器地址

### 问题 6: 认证页面或 portal.html 404 Not Found

**原因**: 缺少静态文件路由

**解决方案**: 已在 web_server.py 中添加通用静态文件路由
- `/static/<filename>` 可访问所有 static 目录下的文件

### 问题 7: 用户注册提示 "no such table: users"

**原因**: 数据库缺少认证相关表

**解决方案**: 运行数据库初始化脚本
```bash
ssh root@120.25.213.109
cd /opt/face_analysis_system/database
python3 init_db.py
```

### 问题 8: chat-manager.html 无法访问

**原因**: 服务器上缺少 chat-manager.html 或 auth_check.js 文件

**解决方案**: 上传缺失的静态文件
```bash
# 上传 chat-manager.html
scp D:/project/toys/static/chat-manager.html root@120.25.213.109:/opt/face_analysis_system/static/

# 上传 auth_check.js
scp D:/project/toys/static/auth_check.js root@120.25.213.109:/opt/face_analysis_system/static/
```

**验证访问**:
```bash
curl -I http://120.25.213.109:8081/static/chat-manager.html
# 应返回 HTTP/1.1 200 OK
```

## 🔧 TTS 语音配置

当前使用的语音（按优先级）：
1. **zh-CN-YunxiNeural** - 云希（成熟稳重男声）✅ 当前使用
2. **zh-CN-YunyangNeural** - 云扬（新闻报道风格男声）
3. **zh-CN-XiaoxiaoNeural** - 晓晓（温柔女声）

**修改方法**: 编辑 `reminder_tts_routes.py` 中的 `voices` 列表

## 📝 版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| v1.0 | 2026-02-01 | 初始版本，精简部署 |
| v2.0 | 2026-02-01 | 添加 routes、database 模块 |
| v2.1 | 2026-02-01 | 添加 web-reminder、web_history |
| v2.2 | 2026-02-01 | 完整测试验证 |
| v2.3 | 2026-02-01 | 添加 services、utils 模块 |
| v2.4 | 2026-02-01 | 添加 static/ 目录和认证页面，优先使用已构建的 web-reminder |
| v2.5 | 2026-02-01 | 自动添加 auth.html 路由到 web_server.py，增加认证页面测试 |
| v2.6 | 2026-02-01 | **修复设备管理 API**：添加认证和 owner_id 设置，修复设备列表 Failed to fetch 问题 |
| v2.7 | 2026-02-01 | **修复认证系统**：添加 users、user_sessions、user_devices 表，修复注册功能 |
| v2.8 | 2026-02-01 | **添加通用静态文件路由**：支持 `/static/<filename>` 访问所有静态文件 |
| v2.9 | 2026-02-01 | **更新 TTS 语音**：更换为正式男声（云希 zh-CN-YunxiNeural） |
| v3.0 | 2026-02-01 | **重新构建 web-reminder**：使用动态 API 地址，修复跨域认证问题 |
| v3.1 | 2026-02-01 | **同步所有静态文件**：添加 chat-manager.html 和 auth_check.js |

## 📞 技术支持

如有问题，请提供：
1. 服务器 IP 和操作系统版本
2. 错误日志：`journalctl -u face-analysis-web -n 100`
3. 部署脚本输出

---

**最后更新**: 2026-02-01
**维护者**: 系统管理员
