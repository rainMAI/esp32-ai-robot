#!/bin/bash
# ============================================
# 服务器迁移部署脚本
# 从: 192.140.190.183
# 到: 120.25.213.109
# ============================================

set -e

echo "=== 服务器迁移部署脚本 ==="
echo "源服务器: 192.140.190.183"
echo "目标服务器: 120.25.213.109"
echo ""

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查是否在目标服务器上运行
CURRENT_IP=$(hostname -I | awk '{print $1}')
echo "当前服务器IP: $CURRENT_IP"
echo ""

# ============================================
# 步骤 1: 从源服务器复制代码
# ============================================
echo -e "${YELLOW}[步骤 1/6]${NC} 从源服务器复制代码..."

# 创建临时目录
mkdir -p /tmp/server_migration
cd /tmp/server_migration

# 从源服务器下载代码包
echo "正在从 192.140.190.183 下载代码..."
ssh root@192.140.190.183 "cd /opt && tar -czf - face_analysis_system/" > face_analysis_system.tar.gz

if [ ! -f face_analysis_system.tar.gz ]; then
    echo -e "${RED}❌ 下载失败！${NC}"
    echo "请检查："
    echo "1. 源服务器 192.140.190.183 是否可访问"
    echo "2. SSH 密钥是否配置"
    echo "3. 网络连接是否正常"
    exit 1
fi

echo -e "${GREEN}✅ 代码下载成功${NC}"
ls -lh face_analysis_system.tar.gz

# ============================================
# 步骤 2: 解压到目标目录
# ============================================
echo -e "\n${YELLOW}[步骤 2/6]${NC} 解压代码到 /opt/face_analysis_system..."

# 停止现有服务（如果存在）
if systemctl is-active --quiet face-analysis-web 2>/dev/null; then
    echo "停止现有服务..."
    systemctl stop face-analysis-web
fi

# 备份现有安装（如果存在）
if [ -d /opt/face_analysis_system ]; then
    BACKUP_DIR="/backup/face_analysis_system_$(date +%Y%m%d_%H%M%S)"
    echo "备份现有安装到: $BACKUP_DIR"
    mkdir -p /backup
    mv /opt/face_analysis_system "$BACKUP_DIR"
fi

# 解压代码
cd /opt
tar -xzf /tmp/server_migration/face_analysis_system.tar.gz

echo -e "${GREEN}✅ 代码解压成功${NC}"

# ============================================
# 步骤 3: 检查并安装 Python 3.8
# ============================================
echo -e "\n${YELLOW}[步骤 3/6]${NC} 检查 Python 3.8..."

if command -v python3.8 &> /dev/null; then
    PYTHON_VERSION=$(python3.8 --version)
    echo -e "${GREEN}✅ Python 3.8 已安装: $PYTHON_VERSION${NC}"
else
    echo -e "${RED}❌ Python 3.8 未安装${NC}"
    echo "请先安装 Python 3.8，或运行安装脚本："
    echo "  curl -o /tmp/install_py38.sh https://example.com/install_python38_centos7.sh"
    echo "  chmod +x /tmp/install_py38.sh"
    echo "  /tmp/install_py38.sh"
    exit 1
fi

# ============================================
# 步骤 4: 检查系统依赖
# ============================================
echo -e "\n${YELLOW}[步骤 4/6]${NC} 检查系统依赖..."

MISSING_DEPS=()

# 检查 ffmpeg
if ! command -v ffmpeg &> /dev/null; then
    MISSING_DEPS+=("ffmpeg")
    echo -e "${YELLOW}⚠️  ffmpeg 未安装${NC}"
else
    echo -e "${GREEN}✅ ffmpeg 已安装${NC}"
fi

# 检查 espeak
if ! command -v espeak &> /dev/null; then
    MISSING_DEPS+=("espeak")
    echo -e "${YELLOW}⚠️  espeak 未安装${NC}"
else
    echo -e "${GREEN}✅ espeak 已安装${NC}"
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo ""
    echo -e "${YELLOW}缺少以下依赖: ${MISSING_DEPS[*]}${NC}"
    echo "安装命令:"
    echo "  yum install -y ffmpeg ffmpeg-devel espeak espeak-devel"
    read -p "是否现在安装? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        yum install -y ffmpeg ffmpeg-devel espeak espeak-devel
    fi
fi

# ============================================
# 步骤 5: 安装 Python 依赖
# ============================================
echo -e "\n${YELLOW}[步骤 5/6]${NC} 安装 Python 依赖..."

cd /opt/face_analysis_system

if [ -f requirements.txt ]; then
    echo "安装 Python 包..."
    python3.8 -m pip install -r requirements.txt -q
    echo -e "${GREEN}✅ Python 依赖安装完成${NC}"
else
    echo -e "${YELLOW}⚠️  requirements.txt 不存在，跳过依赖安装${NC}"
fi

# 验证关键包
echo "验证关键包..."
python3.8 -c "import flask; print('  ✅ Flask')" 2>/dev/null || echo "  ❌ Flask"
python3.8 -c "import edge_tts; print('  ✅ edge-tts')" 2>/dev/null || echo "  ⚠️  edge-tts (可选)"
python3.8 -c "import cv2; print('  ✅ OpenCV')" 2>/dev/null || echo "  ❌ OpenCV"

# ============================================
# 步骤 6: 配置 systemd 服务
# ============================================
echo -e "\n${YELLOW}[步骤 6/6]${NC} 配置 systemd 服务..."

# 创建 systemd 服务文件
cat > /etc/systemd/system/face-analysis-web.service << 'EOF'
[Unit]
Description=Face Analysis Web Server (Python 3.8)
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/face_analysis_system
Environment="PYTHONUNBUFFERED=1"
ExecStart=/usr/local/python3.8/bin/python3.8 web_server.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

echo "systemd 服务文件已创建"

# 重载 systemd
systemctl daemon-reload

# 启用开机自启
systemctl enable face-analysis-web

echo -e "${GREEN}✅ 服务配置完成${NC}"

# ============================================
# 启动服务
# ============================================
echo ""
echo "========================================"
echo "启动服务..."
systemctl start face-analysis-web

sleep 3

# 检查服务状态
if systemctl is-active --quiet face-analysis-web; then
    echo -e "${GREEN}✅ 服务启动成功！${NC}"
    echo ""
    echo "服务状态:"
    systemctl status face-analysis-web --no-pager | head -10
else
    echo -e "${RED}❌ 服务启动失败${NC}"
    echo ""
    echo "查看错误日志:"
    journalctl -u face-analysis-web -n 20 --no-pager
    exit 1
fi

# ============================================
# 配置防火墙
# ============================================
echo ""
echo "========================================"
echo -e "${YELLOW}配置防火墙...${NC}"

if command -v firewall-cmd &> /dev/null; then
    echo "开放端口 8081..."
    firewall-cmd --permanent --add-port=8081/tcp 2>/dev/null || echo "  (防火墙可能未运行)"
    firewall-cmd --reload 2>/dev/null || true
    echo -e "${GREEN}✅ 防火墙配置完成${NC}"
else
    echo -e "${YELLOW}⚠️  firewall-cmd 未找到，跳过防火墙配置${NC}"
fi

# ============================================
# 测试验证
# ============================================
echo ""
echo "========================================"
echo -e "${YELLOW}测试服务...${NC}"

# 测试健康检查
echo "测试 /health 端点..."
sleep 2
HEALTH_CHECK=$(curl -s http://localhost:8081/health 2>/dev/null || echo "failed")

if [[ "$HEALTH_CHECK" == *"status"* ]]; then
    echo -e "${GREEN}✅ 服务响应正常${NC}"
    echo "$HEALTH_CHECK" | python3.8 -m json.tool 2>/dev/null || echo "$HEALTH_CHECK"
else
    echo -e "${RED}❌ 服务无响应${NC}"
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
echo "  状态: systemctl status face-analysis-web"
echo "  日志: journalctl -u face-analysis-web -f"
echo ""
echo "常用命令:"
echo "  重启服务: systemctl restart face-analysis-web"
echo "  停止服务: systemctl stop face-analysis-web"
echo "  查看日志: journalctl -u face-analysis-web -n 100"
echo ""
echo "清理临时文件:"
rm -rf /tmp/server_migration
echo "  rm -rf /tmp/server_migration"
echo ""
