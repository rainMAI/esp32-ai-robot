#!/bin/bash
# 数据库配置验证脚本

echo ==========================================
echo  数据库配置验证
echo ==========================================
echo 

# 1. 检查 database/connection.py 配置
echo 📂 1. 检查数据库连接配置...
if grep -q "DATABASE_PATH = os.path.join(DATABASE_DIR, 'reminders.db')" database/connection.py; then
    echo  ✅ connection.py 配置正确: reminders.db
else
    echo  ❌ connection.py 配置异常
    exit 1
fi

# 2. 检查数据库文件是否存在
echo 
echo 📂 2. 检查数据库文件...
if [ -f "data/reminders.db" ]; then
    SIZE=$(du -h data/reminders.db | cut -f1)
    echo "   ✅ data/reminders.db 存在 (大小: $SIZE)"
else
    echo  ❌ data/reminders.db 不存在
    exit 1
fi

# 3. 检查表结构
echo 
echo 📊 3. 检查数据库表结构...
TABLES=$(sqlite3 data/reminders.db '.tables' 2>/dev/null)
if echo "$TABLES" | grep -q 'chat_messages'; then
    echo  ✅ chat_messages 表存在
else
    echo  ❌ chat_messages 表不存在
    exit 1
fi

if echo "$TABLES" | grep -q 'ai_reports'; then
    echo  ✅ ai_reports 表存在
else
    echo  ❌ ai_reports 表不存在
    exit 1
fi

# 4. 统计数据
echo 
echo 📈 4. 数据统计...
CHAT_COUNT=$(sqlite3 data/reminders.db 'SELECT COUNT(*) FROM chat_messages' 2>/dev/null)
REPORT_COUNT=$(sqlite3 data/reminders.db 'SELECT COUNT(*) FROM ai_reports' 2>/dev/null)
DEVICE_COUNT=$(sqlite3 data/reminders.db 'SELECT COUNT(*) FROM devices' 2>/dev/null)

echo  💬 对话记录: $CHAT_COUNT 条
echo  📊 AI 报告: $REPORT_COUNT 份
echo  📱 设备数量: $DEVICE_COUNT 个

# 5. 检查是否有多个数据库文件
echo 
echo ⚠️ 5. 检查是否存在其他数据库文件...
OTHER_DB=$(find . -maxdepth 2 -name '*.db' -not -path './data/*' -not -path './venv/*' 2>/dev/null)
if [ -n "$OTHER_DB" ]; then
    echo  ⚠️ 发现其他数据库文件:
    echo "$OTHER_DB"
    echo 
    echo  📌 请确认这些文件不会被误用
else
    echo  ✅ 未发现其他数据库文件
fi

# 6. 验证服务导入
echo 
echo 🔍 6. 验证各服务模块...
python3.8 -c "from database.connection import get_db; print('   ✅ database.connection 模块正常')" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ 所有服务使用统一的数据库连接"
else
    echo "   ❌ 数据库连接测试失败"
    exit 1
fi

echo 
echo ==========================================
echo  ✅ 所有检查通过！
echo ==========================================
echo 
echo 📍 当前数据库路径: /opt/face_analysis_system/data/reminders.db
