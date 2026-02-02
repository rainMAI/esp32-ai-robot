#!/usr/bin/env python3
"""
批量更新ESP32项目中的服务器地址
从 192.140.190.183 更新到 114.66.28.207
"""

import os
import re

OLD_SERVER = "192.140.190.183"
NEW_SERVER = "114.66.28.207"

# 需要更新的关键代码文件
CRITICAL_FILES = [
    "main/chat/chat_recorder.cc",
    "main/mcp_server.cc",
    "main/application.cc",
]

def update_file(file_path):
    """更新文件中的服务器地址"""
    if not os.path.exists(file_path):
        print(f"⚠️  文件不存在: {file_path}")
        return False

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        if OLD_SERVER not in content:
            print(f"⊘  无需更新: {file_path}")
            return False

        # 替换所有出现的服务器地址
        new_content = content.replace(OLD_SERVER, NEW_SERVER)

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        # 统计替换次数
        count = content.count(OLD_SERVER)
        print(f"✅ 已更新: {file_path} ({count} 处)")
        return True

    except Exception as e:
        print(f"❌ 更新失败: {file_path} - {e}")
        return False

def main():
    print(f"开始批量更新ESP32项目服务器地址...")
    print(f"从: {OLD_SERVER}")
    print(f"到: {NEW_SERVER}")
    print()

    updated_count = 0

    for file_path in CRITICAL_FILES:
        if update_file(file_path):
            updated_count += 1

    print()
    print(f"✨ 完成! 共更新了 {updated_count} 个文件")

if __name__ == "__main__":
    main()
