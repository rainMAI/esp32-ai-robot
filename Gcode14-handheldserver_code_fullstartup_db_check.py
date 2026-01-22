#!/usr/bin/env python3
"""
启动时数据库检查模块
确保系统使用正确的数据库文件
"""
import os
import sys

def check_database_config():
    """检查数据库配置是否正确"""
    print("\n" + "="*50)
    print("  🔍 数据库配置检查")
    print("="*50)
    
    errors = []
    warnings = []
    
    # 1. 检查配置文件
    try:
        from database.connection import DATABASE_PATH, DATABASE_DIR
        expected_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'reminders.db')
        
        if os.path.abspath(DATABASE_PATH) == os.path.abspath(expected_path):
            print(f"\n✅ 数据库路径配置正确:")
            print(f"   {DATABASE_PATH}")
        else:
            errors.append(f"数据库路径配置错误!")
            errors.append(f"   预期: {expected_path}")
            errors.append(f"   实际: {DATABASE_PATH}")
    except Exception as e:
        errors.append(f"无法读取数据库配置: {e}")
        return False, errors, warnings
    
    # 2. 检查数据库文件是否存在
    if not os.path.exists(DATABASE_PATH):
        errors.append(f"数据库文件不存在: {DATABASE_PATH}")
        return False, errors, warnings
    
    file_size = os.path.getsize(DATABASE_PATH)
    print(f"\n✅ 数据库文件存在 (大小: {file_size:,} bytes = {file_size/1024:.1f} KB)")
    
    # 3. 检查表结构
    try:
        from database.connection import get_db
        db = get_db()
        
        # 检查关键表
        tables = db.execute("SELECT name FROM sqlite_master WHERE type='table'", fetch_all=True)
        table_names = [t[0] for t in tables] if tables else []
        
        required_tables = ['chat_messages', 'ai_reports', 'devices', 'reminders']
        missing_tables = [t for t in required_tables if t not in table_names]
        
        if missing_tables:
            errors.append(f"缺少必要的表: {', '.join(missing_tables)}")
        else:
            print(f"\n✅ 数据库表结构完整 ({len(table_names)} 个表)")
        
        # 4. 统计数据
        chat_count = db.execute('SELECT COUNT(*) FROM chat_messages', fetch_one=True)[0]
        report_count = db.execute('SELECT COUNT(*) FROM ai_reports', fetch_one=True)[0]
        device_count = db.execute('SELECT COUNT(*) FROM devices', fetch_one=True)[0]
        
        print(f"\n📊 数据统计:")
        print(f"   💬 对话记录: {chat_count} 条")
        print(f"   📈 AI 报告: {report_count} 份")
        print(f"   📱 设备数量: {device_count} 个")
        
    except Exception as e:
        errors.append(f"数据库查询失败: {e}")
        return False, errors, warnings
    
    # 5. 检查其他数据库文件
    project_dir = os.path.dirname(os.path.abspath(__file__))
    other_dbs = []
    for root, dirs, files in os.walk(project_dir):
        # 跳过 venv 和 data 目录
        if 'venv' in root or '__pycache__' in root:
            continue
        if root.endswith('/data'):
            continue
            
        for f in files:
            if f.endswith('.db') and f != 'reminders.db':
                other_dbs.append(os.path.join(root, f))
    
    if other_dbs:
        warnings.append(f"\n⚠️  发现其他数据库文件:")
        for db_path in other_dbs:
            warnings.append(f"   - {db_path}")
        warnings.append(f"   📌 请确认这些文件不会被误用")
    
    # 6. 最终结果
    print("\n" + "="*50)
    if errors:
        print("  ❌ 检查失败！")
        for error in errors:
            print(error)
        return False, errors, warnings
    else:
        print("  ✅ 所有检查通过！")
        if warnings:
            for warning in warnings:
                print(warning)
        print("="*50 + "\n")
        return True, errors, warnings

if __name__ == "__main__":
    success, errors, warnings = check_database_config()
    sys.exit(0 if success else 1)
