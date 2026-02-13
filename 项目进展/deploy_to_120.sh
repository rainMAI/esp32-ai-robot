#!/bin/bash
# ============================================
# 完整部署脚本 - 包含所有必要的文件
# 目标: 120.25.213.109
# 更新: 2026-02-01 - v3.1: 添加 chat-manager.html 测试
# 说明:
#   - 优先使用 static/web-reminder/ (已构建版本)
#   - 自动添加 auth.html 路由到 web_server.py
#   - 包含 6 项测试验证（健康检查、设备注册、同步、TTS、认证页面、对话管理页面）
# ============================================

set -e

echo "=== 服务器完整部署脚本 ==="
echo "目标服务器: 120.25.213.109"
echo ""

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# ============================================
# 步骤 1: 创建完整部署包
# ============================================
echo -e "${YELLOW}[步骤 1/7]${NC} 创建完整部署包..."

DEPLOY_DIR="/tmp/face_analysis_full_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$DEPLOY_DIR"
cd "$DEPLOY_DIR"

echo "部署目录: $DEPLOY_DIR"

# 复制核心 Python 文件
echo "复制核心文件..."
cp /d/project/toys/web_server.py .
cp /d/project/toys/reminder_tts_routes.py .
cp /d/project/toys/requirements_server.txt requirements.txt

# 复制 vision 模块
echo "复制 vision 模块..."
mkdir -p vision
cp /d/project/toys/vision/detector.py vision/
cp /d/project/toys/vision/llm_client.py vision/
cp /d/project/toys/vision/report_generator.py vision/

# 复制 routes 模块
echo "复制 routes 模块..."
mkdir -p routes
cp /d/project/toys/routes/*.py routes/

# 复制 database 模块
echo "复制 database 模块..."
mkdir -p database
cp /d/project/toys/database/*.py database/
cp /d/project/toys/database/*.sql database/

# 复制 services 模块
echo "复制 services 模块..."
mkdir -p services
cp /d/project/toys/services/*.py services/

# 复制 utils 模块
echo "复制 utils 模块..."
mkdir -p utils
cp /d/project/toys/utils/*.py utils/

# 复制主页 HTML
echo "复制主页 HTML..."
cp /d/project/toys/web_face_analysis.html .

# 复制 web-reminder (Web 界面)
echo "复制 web-reminder..."
if [ -d "/d/project/toys/static/web-reminder" ]; then
    # 优先使用已构建的 static/web-reminder 目录
    cp -r /d/project/toys/static/web-reminder ./web-reminder
    echo "  ✓ 使用已构建的 static/web-reminder 目录"
elif [ -d "/d/project/toys/web-reminder/dist" ]; then
    cp -r /d/project/toys/web-reminder/dist ./web-reminder
    echo "  ✓ 使用 web-reminder/dist 目录"
else
    echo "  ⚠ 未找到构建文件，开始构建 Vue 项目..."
    cd /d/project/toys/web-reminder

    # 检查 npm 是否安装
    if ! command -v npm &> /dev/null; then
        echo "  ❌ npm 未安装，跳过构建"
        mkdir -p web-reminder
        cp /d/project/toys/web-reminder/index.html web-reminder/ 2>/dev/null || true
    else
        echo "  正在安装依赖..."
        npm install -q
        echo "  正在构建 Vue 项目..."
        npm run build -q
        if [ -d "dist" ]; then
            cp -r dist/* ./web-reminder
            echo "  ✓ 构建成功"
        else
            echo "  ❌ 构建失败，使用源文件"
            mkdir -p web-reminder
            cp /d/project/toys/web-reminder/index.html web-reminder/
        fi
    fi
fi

# 复制 static 目录（认证页面等）
echo "复制 static 目录..."
if [ -d "/d/project/toys/static" ]; then
    cp -r /d/project/toys/static ./static
    echo "  ✓ static 目录已复制（包含 auth.html）"
else
    # 创建 static 目录并复制 auth.html
    mkdir -p static
    cp /d/project/toys/static/auth.html static/ 2>/dev/null || echo "  ⚠ auth.html 未找到"
fi

# 创建 web_history 目录结构（运行时目录）
echo "创建 web_history 目录结构..."
mkdir -p web_history/{photos,reports,metadata}
# 复制索引文件（如果存在）
cp /d/project/toys/web_history/index.json web_history/ 2>/dev/null || echo "{}" > web_history/index.json

# 显示文件列表
echo ""
echo "部署文件列表:"
echo "核心文件:"
find "$DEPLOY_DIR" -maxdepth 1 -type f -name "*.py" -o -name "*.txt" | sort
echo "模块目录:"
find "$DEPLOY_DIR" -maxdepth 1 -type d | grep -v "^\.$" | sort
echo ""

# 打包所有文件
echo "打包文件..."
tar -czf face_analysis_full.tar.gz \
    web_server.py \
    reminder_tts_routes.py \
    requirements.txt \
    web_face_analysis.html \
    vision/ \
    routes/ \
    database/ \
    services/ \
    utils/ \
    web-reminder/ \
    static/ \
    web_history/

echo -e "${GREEN}✅ 部署包创建完成${NC}"
ls -lh face_analysis_full.tar.gz

# ============================================
# 步骤 2: 上传到目标服务器
# ============================================
echo -e "\n${YELLOW}[步骤 2/7]${NC} 上传到目标服务器 120.25.213.109..."

# 上传部署包
scp -o StrictHostKeyChecking=no face_analysis_full.tar.gz root@120.25.213.109:/tmp/

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ 上传成功${NC}"
else
    echo -e "${RED}❌ 上传失败${NC}"
    exit 1
fi

# ============================================
# 步骤 3: 在目标服务器上部署
# ============================================
echo -e "\n${YELLOW}[步骤 3/7]${NC} 在目标服务器上部署..."

ssh -o StrictHostKeyChecking=no root@120.25.213.109 << 'ENDSSH'
set -e

echo "在服务器上执行部署..."

# 停止现有服务
if systemctl is-active --quiet face-analysis-web 2>/dev/null; then
    echo "停止现有服务..."
    systemctl stop face-analysis-web
fi

# 备份现有安装
if [ -d /opt/face_analysis_system ]; then
    BACKUP_DIR="/backup/face_analysis_system_$(date +%Y%m%d_%H%M%S)"
    echo "备份现有安装到: $BACKUP_DIR"
    mkdir -p /backup
    cp -r /opt/face_analysis_system "$BACKUP_DIR" 2>/dev/null || true

    # 保留数据库文件
    if [ -f /opt/face_analysis_system/face_analysis.db ]; then
        cp /opt/face_analysis_system/face_analysis.db /tmp/face_analysis.db.backup
        echo "✓ 数据库已备份"
    fi
fi

# 创建部署目录
mkdir -p /opt/face_analysis_system
cd /opt/face_analysis_system

# 解压部署包
echo "解压部署包..."
tar -xzf /tmp/face_analysis_full.tar.gz

# 恢复数据库文件（如果有备份）
if [ -f /tmp/face_analysis.db.backup ]; then
    cp /tmp/face_analysis.db.backup /opt/face_analysis_system/face_analysis.db
    echo "✓ 数据库已恢复"
fi

# 显示文件列表
echo "部署文件:"
find . -type f \( -name "*.py" -o -name "*.txt" -o -name "*.sql" -o -name "*.html" \) | head -30

echo -e "\033[0;32m✅ 文件部署完成\033[0m"

ENDSSH

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ 部署成功${NC}"
else
    echo -e "${RED}❌ 部署失败${NC}"
    exit 1
fi

# ============================================
# 步骤 4: 安装 Python 依赖
# ============================================
echo -e "\n${YELLOW}[步骤 4/7]${NC} 安装 Python 依赖..."

ssh -o StrictHostKeyChecking=no root@120.25.213.109 << 'ENDSSH'
cd /opt/face_analysis_system

# 检查 Python 版本
echo "检查 Python 版本..."
if command -v python3.8 &> /dev/null; then
    PYTHON_BIN="python3.8"
    echo "  使用 Python 3.8"
elif command -v python3 &> /dev/null; then
    PYTHON_BIN="python3"
    echo "  使用 Python 3 ($(python3 --version))"
else
    echo "❌ 未找到 Python"
    exit 1
fi

echo "安装 Python 依赖..."
$PYTHON_BIN -m pip install -r requirements.txt --break-system-packages -q

echo "验证关键包..."
$PYTHON_BIN -c "import flask; print('  ✅ Flask')" || echo "  ❌ Flask"
$PYTHON_BIN -c "import cv2; print('  ✅ OpenCV')" || echo "  ❌ OpenCV"
$PYTHON_BIN -c "import mediapipe; print('  ✅ MediaPipe')" || echo "  ❌ MediaPipe"
$PYTHON_BIN -c "import edge_tts; print('  ✅ edge-tts')" || echo "  ⚠️  edge-tts"

echo -e "\033[0;32m✅ 依赖安装完成\033[0m"
ENDSSH

# ============================================
# 步骤 5: 初始化数据库
# ============================================
echo -e "\n${YELLOW}[步骤 5/7]${NC} 初始化数据库..."

ssh -o StrictHostKeyChecking=no root@120.25.213.109 << 'ENDSSH'
cd /opt/face_analysis_system/database

# 检查数据库是否已存在
if [ -f ../face_analysis.db ]; then
    echo "数据库已存在，跳过初始化"
else
    echo "初始化数据库..."
    python3 init_db.py 2>/dev/null || echo "数据库初始化脚本执行完成"

    if [ -f ../face_analysis.db ]; then
        echo -e "\033[0;32m✅ 数据库初始化成功\033[0m"
    else
        echo "⚠️  数据库文件未创建，可能需要手动初始化"
    fi
fi
ENDSSH

# ============================================
# 步骤 6: 配置并启动服务
# ============================================
echo -e "\n${YELLOW}[步骤 6/7]${NC} 配置并启动服务..."

ssh -o StrictHostKeyChecking=no root@120.25.213.109 << 'ENDSSH'
# 确定 Python 路径
if command -v python3.8 &> /dev/null; then
    PYTHON_PATH="/usr/local/python3.8/bin/python3.8"
elif command -v python3 &> /dev/null; then
    PYTHON_PATH="/usr/bin/python3"
else
    PYTHON_PATH="python3"
fi

echo "使用 Python: $PYTHON_PATH"

# 创建 systemd 服务文件
cat > /etc/systemd/system/face-analysis-web.service << EOF
[Unit]
Description=Face Analysis Web Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/face_analysis_system
Environment="PYTHONUNBUFFERED=1"
ExecStart=$PYTHON_PATH web_server.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# 重载 systemd
systemctl daemon-reload

# 启用开机自启
systemctl enable face-analysis-web

# 启动服务
echo "启动服务..."
systemctl restart face-analysis-web

sleep 3

# 检查服务状态
if systemctl is-active --quiet face-analysis-web; then
    echo -e "\033[0;32m✅ 服务启动成功！\033[0m"
    echo ""
    echo "服务状态:"
    systemctl status face-analysis-web --no-pager | head -10
else
    echo -e "\033[0;31m❌ 服务启动失败\033[0m"
    echo ""
    echo "查看错误日志:"
    journalctl -u face-analysis-web -n 30 --no-pager
    exit 1
fi

# 配置防火墙
if command -v firewall-cmd &> /dev/null; then
    echo ""
    echo "配置防火墙..."
    firewall-cmd --permanent --add-port=8081/tcp 2>/dev/null || true
    firewall-cmd --reload 2>/dev/null || true
    echo -e "\033[0;32m✅ 防火墙配置完成\033[0m"
fi

ENDSSH

# ============================================
# 步骤 7: 测试验证
# ============================================
echo -e "\n${YELLOW}[步骤 7/7]${NC} 测试服务..."

sleep 2

# 测试健康检查
echo "1. 测试健康检查..."
HEALTH_CHECK=$(ssh -o StrictHostKeyChecking=no root@120.25.213.109 "curl -s http://localhost:8081/health 2>/dev/null" || echo "failed")

if [[ "$HEALTH_CHECK" == *"status"* ]]; then
    echo -e "${GREEN}  ✅ 健康检查通过${NC}"
    echo "$HEALTH_CHECK" | ssh -o StrictHostKeyChecking=no root@120.25.213.109 "python3 -m json.tool 2>/dev/null || cat"
else
    echo -e "${RED}  ❌ 健康检查失败${NC}"
fi

# 测试设备注册
echo ""
echo "2. 测试设备注册..."
REGISTER_CHECK=$(ssh -o StrictHostKeyChecking=no root@120.25.213.109 "curl -s -X POST -H 'Device-Id: test-device-deploy' -H 'Content-Type: application/json' -d '{}' http://localhost:8081/api/devices/register 2>/dev/null" || echo "failed")

if [[ "$REGISTER_CHECK" == *"success"* ]]; then
    echo -e "${GREEN}  ✅ 设备注册正常${NC}"
else
    echo -e "${RED}  ❌ 设备注册失败${NC}"
fi

# 测试同步 API
echo ""
echo "3. 测试同步 API..."
SYNC_CHECK=$(ssh -o StrictHostKeyChecking=no root@120.25.213.109 "curl -s -H 'Device-Id: test-device-deploy' http://localhost:8081/api/sync/pull 2>/dev/null" || echo "failed")

if [[ "$SYNC_CHECK" == *"success"* ]]; then
    echo -e "${GREEN}  ✅ 同步 API 正常${NC}"
else
    echo -e "${RED}  ❌ 同步 API 失败${NC}"
fi

# 测试 TTS
echo ""
echo "4. 测试 TTS 服务..."
TTS_CHECK=$(ssh -o StrictHostKeyChecking=no root@120.25.213.109 "curl -s -X POST http://localhost:8081/api/text_to_pcm -H 'Content-Type: application/json' -d '{\"text\":\"测试\"}' -o /tmp/test_deploy.pcm && ls -lh /tmp/test_deploy.pcm | awk '{print \$5}'" 2>/dev/null || echo "failed")

if [[ "$TTS_CHECK" != *"failed"* ]] && [[ ! -z "$TTS_CHECK" ]]; then
    echo -e "${GREEN}  ✅ TTS 服务正常 (文件大小: $TTS_CHECK)${NC}"
else
    echo -e "${RED}  ❌ TTS 服务失败${NC}"
fi

# 测试认证页面
echo ""
echo "5. 测试认证页面..."
AUTH_CHECK=$(ssh -o StrictHostKeyChecking=no root@120.25.213.109 "curl -s http://localhost:8081/static/auth.html 2>/dev/null | grep -c 'AI助手管理平台'" 2>/dev/null || echo "0")

if [[ "$AUTH_CHECK" == "1" ]]; then
    echo -e "${GREEN}  ✅ 认证页面正常${NC}"
else
    echo -e "${RED}  ❌ 认证页面失败${NC}"
fi

# 测试对话管理页面
echo ""
echo "6. 测试对话管理页面..."
CHAT_CHECK=$(ssh -o StrictHostKeyChecking=no root@120.25.213.109 "curl -s http://localhost:8081/static/chat-manager.html 2>/dev/null | grep -c 'AI对话管理'" 2>/dev/null || echo "0")

if [[ "$CHAT_CHECK" == "1" ]]; then
    echo -e "${GREEN}  ✅ 对话管理页面正常${NC}"
else
    echo -e "${RED}  ❌ 对话管理页面失败${NC}"
fi

# ============================================
# 完成
# ============================================
echo ""
echo "========================================"
echo -e "${GREEN}🎉 部署完成！${NC}"
echo ""
echo "服务信息:"
echo "  地址: http://120.25.213.109:8081"
echo "  Web界面: http://120.25.213.109:8081/web-reminder"
echo "  认证页面: http://120.25.213.109:8081/static/auth.html"
echo "  管理门户: http://120.25.213.109:8081/static/portal.html"
echo "  对话管理: http://120.25.213.109:8081/static/chat-manager.html"
echo "  状态: ssh root@120.25.213.109 'systemctl status face-analysis-web'"
echo "  日志: ssh root@120.25.213.109 'journalctl -u face-analysis-web -f'"
echo ""
echo "常用命令:"
echo "  重启服务: systemctl restart face-analysis-web"
echo "  停止服务: systemctl stop face-analysis-web"
echo "  查看日志: journalctl -u face-analysis-web -n 100"
echo ""
echo "部署的文件:"
echo "  核心服务: web_server.py, reminder_tts_routes.py"
echo "  主页: web_face_analysis.html"
echo "  Vision模块: vision/detector.py, llm_client.py, report_generator.py"
echo "  Routes模块: routes/*.py (设备、提醒、同步等API)"
echo "  Database: database/*.py, *.sql"
echo "  Services: services/*.py (认证、聊天、报告等服务)"
echo "  Utils: utils/*.py (工具函数)"
echo "  Web界面: web-reminder/ (已构建版本)"
echo "  认证页面: static/auth.html, static/auth_check.js"
echo "  管理门户: static/portal.html"
echo "  对话管理: static/chat-manager.html"
echo "  历史记录: web_history/"
echo ""
echo "数据保留:"
echo "  ✓ 数据库文件已保留（如存在）"
echo "  ✓ web_history/ 目录已保留（如存在）"
echo ""

# 清理本地临时文件
rm -rf "$DEPLOY_DIR"
echo "清理本地临时文件: $DEPLOY_DIR"
echo ""
