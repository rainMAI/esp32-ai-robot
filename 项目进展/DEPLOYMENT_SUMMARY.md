# 服务器部署总结

## 部署日期
2026-02-01

## 服务器信息
- **地址**: 120.25.213.109:8081
- **系统**: CentOS Linux
- **Python**: 3.12
- **服务**: face-analysis-web

## 部署内容概览

### 核心服务
- `web_server.py` - 主服务器
- `reminder_tts_routes.py` - TTS 路由

### 数据库
- `face_analysis.db` - SQLite 数据库
- 包含表：devices, reminders, users, user_sessions, user_devices, chat_messages, ai_reports 等

### 功能模块
- **vision/** - 人脸检测
- **routes/** - API 路由（12个文件）
- **database/** - 数据库管理
- **services/** - 认证、聊天、报告服务
- **utils/** - 工具函数

### Web 界面
- **/web-reminder/** - 提醒管理界面（Vue 构建）
- **/static/auth.html** - 认证页面
- **/static/auth_check.js** - 认证检查脚本
- **/static/portal.html** - 管理门户
- **/static/chat-manager.html** - AI对话管理 & 每日总结

## 修复的问题

### 1. 设备列表 Failed to fetch ✅
**问题**: `/api/devices/manual` 缺少认证和 owner_id 设置
**解决**:
- 添加 token 验证
- 提取 user_id 并设置到设备
- 支持设备认领（已存在但无所有者）

### 2. 用户注册失败 ✅
**问题**: 数据库缺少认证相关表
**解决**:
- 创建 users、user_sessions、user_devices 表
- 运行数据库初始化脚本
- 添加外键约束

### 3. 静态文件 404 ✅
**问题**: auth.html、portal.html 无法访问
**解决**:
- 添加通用静态文件路由 `/static/<filename>`
- 自动服务 static/ 目录下所有文件

### 4. TTS 语音更换 ✅
**更改**: 晓晓（温柔女声）→ 云希（成熟稳重男声）
**语音**: zh-CN-YunxiNeural
**风格**: 正式、专业

### 5. Web 界面 API 地址 ✅
**问题**: 硬编码旧服务器地址
**解决**:
- 修改 src/api/base.js 使用动态地址
- 重新构建 Vue 应用
- 部署新构建版本

### 6. chat-manager.html 404 ✅
**问题**: AI对话管理页面无法访问
**解决**:
- 上传 chat-manager.html 到服务器
- 上传 auth_check.js 到服务器
- 通过通用静态文件路由 `/static/<filename>` 提供访问

## ESP32 项目配置

### 已更新文件
- [main/application.cc:361](d:/project/eyes/main/application.cc#L361) - 提醒服务器地址
- [main/application.cc:988](d:/project/eyes/main/application.cc#L988) - TTS API 地址

### 配置值
```cpp
// 提醒服务器
SetServerUrl("http://120.25.213.109:8081");

// TTS API
std::string url = "http://120.25.213.109:8081/api/text_to_pcm";
```

## 测试账号

### 用户认证
- **用户名**: admin
- **密码**: admin123
- **Token 有效期**: 30 天

### 测试设备
- **MAC**: 11:22:33:44:55:66
- **名称**: 客厅设备
- **所有者**: admin (user_id: 1)

## 服务端点

### 基础端点
- `GET /` - 主页
- `GET /health` - 健康检查
- `GET /web-reminder` - Web 管理界面

### Web 界面访问
- **提醒管理**: http://120.25.213.109:8081/web-reminder
- **认证页面**: http://120.25.213.109:8081/static/auth.html
- **管理门户**: http://120.25.213.109:8081/static/portal.html
- **对话管理**: http://120.25.213.109:8081/static/chat-manager.html

### 静态文件
- `GET /static/auth.html` - 认证页面
- `GET /static/auth_check.js` - 认证检查脚本
- `GET /static/portal.html` - 管理门户
- `GET /static/chat-manager.html` - AI对话管理 & 每日总结
- `GET /static/<filename>` - 通用静态文件访问

### 设备管理
- `POST /api/devices/register` - 设备注册
- `POST /api/devices/manual` - 手动添加设备（需认证）
- `GET /api/devices` - 获取设备列表（需认证）

### 提醒管理
- `POST /api/reminders` - 创建提醒
- `GET /api/reminders` - 获取提醒列表
- `PUT /api/reminders/<id>` - 更新提醒
- `DELETE /api/reminders/<id>` - 删除提醒

### 同步 API
- `GET /api/sync/pull` - 拉取提醒和设置
- `POST /api/sync/push` - 推送本地提醒

### TTS 服务
- `POST /api/text_to_pcm` - 文本转语音（edge-tts 云希语音）

## 部署脚本

### 使用方法
```bash
cd D:/project/toys
chmod +x deploy_to_120.sh
./deploy_to_120.sh
```

### 自动化功能
1. 创建部署包（包含所有模块）
2. 上传到服务器
3. 备份现有安装
4. 部署新文件
5. 安装 Python 依赖
6. 初始化数据库
7. 配置 systemd 服务
8. 启动服务
9. 运行 6 项测试验证

### 测试验证
1. ✅ 健康检查
2. ✅ 设备注册
3. ✅ 同步 API
4. ✅ TTS 服务
5. ✅ 认证页面
6. ✅ 对话管理页面

## 维护命令

### 服务管理
```bash
# 查看状态
ssh root@120.25.213.109 "systemctl status face-analysis-web"

# 重启服务
ssh root@120.25.213.109 "systemctl restart face-analysis-web"

# 查看日志
ssh root@120.25.213.109 "journalctl -u face-analysis-web -f"

# 查看最近错误
ssh root@120.25.213.109 "journalctl -u face-analysis-web -p err -n 50"
```

### 数据库
```bash
# 连接服务器
ssh root@120.25.213.213.109

# 进入数据库目录
cd /opt/face_analysis_system/database

# 重新初始化
python3 init_db.py
```

## TTS 语音配置

### 当前使用
- **zh-CN-YunxiNeural** - 云希（成熟稳重男声）✅

### 备选语音
- zh-CN-YunyangNeural - 云扬（新闻报道风格）
- zh-CN-XiaoxiaoNeural - 晓晓（温柔女声）

### 修改方法
编辑 [reminder_tts_routes.py](D:/project/toys/reminder_tts_routes.py) 中的 `voices` 列表

## 数据保留策略

部署时自动保留：
- ✅ `face_analysis.db` 数据库文件
- ✅ `web_history/` 目录及内容

备份位置：`/backup/face_analysis_system_<timestamp>/`

## 已知问题

### 眼睛显示服务启动失败
**影响**: 不影响核心功能，仅眼睛显示不可用
**原因**: 服务器上无 COM3 串口
**状态**: 可忽略（ESP32 设备不需要此服务）

### edge-tts 语音限制
**说明**: 某些语音（如 XiaoyanNeural）可能在当前 edge-tts 版本中不可用
**解决**: 脚本会自动重试其他语音

## 文件修改记录

### 服务器端
- `/opt/face_analysis_system/routes/device_routes.py` - 添加设备认证
- `/opt/face_analysis_system/web_server.py` - 添加静态文件路由
- `/opt/face_analysis_system/static/web-reminder/` - 重新构建版本
- `/opt/face_analysis_system/static/chat-manager.html` - AI对话管理页面
- `/opt/face_analysis_system/static/auth_check.js` - 认证检查脚本

### 本地端
- `D:/project/toys/routes/device_routes.py` - 设备认证
- `D:/project/toys/reminder_tts_routes.py` - TTS 语音
- `D:/project/toys/web-reminder/src/api/base.js` - 动态 API 地址
- `D:/project/toys/web_server.py` - 静态文件路由
- `D:/project/toys/database/schema.sql` - 认证表结构

### ESP32 项目
- `main/application.cc` - 更新服务器地址

## 部署验证

### 服务状态
```bash
curl http://120.25.213.109:8081/health
# {"detection_available":true,"service":"web-face-analysis-server","status":"ok","version":"1.0"}
```

### 认证测试
```bash
# 登录
curl -X POST http://120.25.213.109:8081/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}'

# 返回 token
```

### 设备列表测试
```bash
TOKEN="your_token_here"
curl -H "Authorization: Bearer $TOKEN" \
  http://120.25.213.109:8081/api/devices/
```

## 后续优化建议

1. **监控**: 添加服务监控和告警
2. **备份**: 定期备份数据库和 web_history
3. **日志**: 实施日志轮转策略
4. **安全**: 添加 HTTPS 支持
5. **性能**: 考虑使用生产级 WSGI 服务器（如 Gunicorn）

## 联系方式

如有问题请参考：
- 部署文档：[DEPLOY_README.md](D:/project/toys/DEPLOY_README.md)
- 部署脚本：[deploy_to_120.sh](D:/project/toys/deploy_to_120.sh)
- 技术支持：查看日志 `journalctl -u face-analysis-web -n 100`

---

**部署完成时间**: 2026-02-01
**最后更新**: 2026-02-01 (v3.1)
**部署状态**: ✅ 成功
